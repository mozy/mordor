// Copyright (c) 2009 - Mozy, Inc.

#include "server.h"

#include <boost/bind.hpp>

#include "mordor/fiber.h"
#include "mordor/scheduler.h"
#include "mordor/socket.h"
#include "mordor/streams/null.h"
#include "mordor/streams/transfer.h"
#include "multipart.h"
#include "parser.h"

namespace Mordor {
namespace HTTP {

static Logger::ptr g_log = Log::lookup("mordor:http:server");

ServerConnection::ServerConnection(Stream::ptr stream, boost::function<void (ServerRequest::ptr)> dg)
: Connection(stream),
  m_dg(dg),
  m_requestCount(0),
  m_priorRequestFailed(~0ull),
  m_priorRequestClosed(~0ull),
  m_priorResponseClosed(~0ull)
{
    MORDOR_ASSERT(m_dg);
}

void
ServerConnection::processRequests()
{
    boost::mutex::scoped_lock lock(m_mutex);
    invariant();
    scheduleNextRequest(NULL);
}

std::vector<ServerRequest::const_ptr>
ServerConnection::requests()
{
    std::vector<ServerRequest::const_ptr> result;
    boost::mutex::scoped_lock lock(m_mutex);
    invariant();
    for (std::list<ServerRequest *>::const_iterator it(m_pendingRequests.begin());
        it != m_pendingRequests.end();
        ++it) {
        result.push_back((*it)->shared_from_this());
    }
    return result;
}

void
ServerConnection::scheduleNextRequest(ServerRequest *request)
{
    // MORDOR_ASSERT(m_mutex.locked());
    MORDOR_ASSERT(request || m_requestCount == 0);
    if (m_requestCount == 0 ||
        (request && request->m_requestNumber == m_requestCount &&
        request->m_requestState == ServerRequest::COMPLETE &&
        m_priorRequestFailed == ~0ull && m_priorRequestClosed == ~0ull &&
        m_priorResponseClosed == ~0ull)) {
        ServerRequest::ptr nextRequest(new ServerRequest(shared_from_this()));
        m_pendingRequests.push_back(nextRequest.get());
        MORDOR_LOG_TRACE(g_log) << this << "-" << nextRequest->m_requestNumber
            << " scheduling request";
        Scheduler::getThis()->schedule(boost::bind(&ServerRequest::doRequest,
            nextRequest));
    }
}

void
ServerConnection::requestComplete(ServerRequest *request)
{
    MORDOR_ASSERT(request);
    bool close = false;
    {
        boost::mutex::scoped_lock lock(m_mutex);
        invariant();
        MORDOR_ASSERT(!m_pendingRequests.empty());
        MORDOR_ASSERT(request == m_pendingRequests.back());
        MORDOR_ASSERT(request->m_requestState == ServerRequest::HEADERS ||
            request->m_requestState == ServerRequest::BODY);
        MORDOR_LOG_TRACE(g_log) << this << "-" << request->m_requestNumber
            << " request complete";
        request->m_requestState = ServerRequest::COMPLETE;
        close = request->m_willClose;
        if (request->m_responseState >= ServerRequest::COMPLETE) {
            MORDOR_ASSERT(request == m_pendingRequests.front());
            m_pendingRequests.pop_front();
            if (!close)
                scheduleNextRequest(request);
        }
        if (!close) {
            if (request->m_pipeline)
                scheduleNextRequest(request);
        } else {
            m_priorRequestClosed = request->m_requestNumber;
            MORDOR_LOG_TRACE(g_log) << this << " closing";
        }
    }
    if (close && m_stream->supportsHalfClose())
        m_stream->close(Stream::READ);
}

void
ServerConnection::responseComplete(ServerRequest *request)
{
    {
        boost::mutex::scoped_lock lock(m_mutex);
        invariant();
        MORDOR_ASSERT(request->m_responseState == ServerRequest::HEADERS ||
            ServerRequest::BODY);
        MORDOR_ASSERT(!m_pendingRequests.empty());
        MORDOR_LOG_TRACE(g_log) << this << "-" << request->m_requestNumber
            << " response complete";
        std::list<ServerRequest *>::iterator it = m_pendingRequests.begin();
        MORDOR_ASSERT(request == *it);
        ++it;
        if (it != m_pendingRequests.end()) {
            std::set<ServerRequest *>::iterator waitIt(m_waitingResponses.find(*it));
            if (waitIt != m_waitingResponses.end()) {
                request->m_responseState = ServerRequest::COMPLETE;
                if (request->m_requestState >= ServerRequest::COMPLETE) {
                    m_pendingRequests.pop_front();
                    scheduleNextRequest(request);
                }
                request = *it;
                request->m_responseState = ServerRequest::HEADERS;
                m_waitingResponses.erase(waitIt);
                MORDOR_LOG_TRACE(g_log) << this << "-" << request->m_requestNumber
                    << " scheduling response";
                request->m_scheduler->schedule(request->m_fiber);
                return;
            }
        } else {
            // Do not remove from m_pendingRequests until we finish flushing
            // The next request can start before the flush completes, though
            if (request->m_requestState >= ServerRequest::COMPLETE)
                scheduleNextRequest(request);
        }
        if (request->m_willClose) {
            m_priorResponseClosed = request->m_requestNumber;
            MORDOR_LOG_TRACE(g_log) << this << " closing";
        } else {
            MORDOR_LOG_TRACE(g_log) << this << " flushing";
        }
    }
    if (request->m_willClose) {
        try {
            m_stream->close();
        } catch (...) {
            request->cancel();
            throw;
        }
    } else {
        m_stream->flush();
    }
    boost::mutex::scoped_lock lock(m_mutex);
    invariant();
    MORDOR_ASSERT(!m_pendingRequests.empty());
    MORDOR_ASSERT(request == m_pendingRequests.front());
    request->m_responseState = ServerRequest::COMPLETE;
    if (request->m_requestState >= ServerRequest::COMPLETE)
        m_pendingRequests.pop_front();

    // Someone else may have queued up while we were flushing
    if (!m_pendingRequests.empty()) {
        request = m_pendingRequests.front();
        std::set<ServerRequest *>::iterator waitIt(m_waitingResponses.find(request));
        if (waitIt != m_waitingResponses.end()) {
            m_waitingResponses.erase(waitIt);
            request->m_responseState = ServerRequest::HEADERS;
            MORDOR_LOG_TRACE(g_log) << this << "-" << request->m_requestNumber
                << " scheduling response";
            request->m_scheduler->schedule(request->m_fiber);
            return;
        }
    }
}

void
ServerConnection::scheduleAllWaitingResponses()
{
    MORDOR_ASSERT(m_priorRequestFailed != ~0ull || m_priorResponseClosed != ~0ull);
    // MORDOR_ASSERT(m_mutex.locked());
    MORDOR_LOG_TRACE(g_log) << this << " scheduling all responses";

    unsigned long long firstFailedRequest = (std::min)(m_priorRequestFailed,
        m_priorResponseClosed);
    for (std::list<ServerRequest *>::iterator it(m_pendingRequests.begin());
        it != m_pendingRequests.end();
        ++it) {
        ServerRequest *request = *it;
        if (request->m_requestNumber < firstFailedRequest)
            continue;
        std::set<ServerRequest *>::iterator waiting = m_waitingResponses.find(request);
        if (waiting != m_waitingResponses.end()) {
            MORDOR_LOG_TRACE(g_log) << this << "-" << request->m_requestNumber
                << " scheduling response";
            request->m_scheduler->schedule(request->m_fiber);
            it = m_pendingRequests.erase(it);
            --it;
            m_waitingResponses.erase(waiting);
        }
    }
}

void
ServerConnection::invariant() const
{
    // MORDOR_ASSERT(m_mutex.locked());
    bool seenResponseNotDone = false;
    for (std::list<ServerRequest *>::const_iterator it(m_pendingRequests.begin());
        it != m_pendingRequests.end();
        ++it) {
        ServerRequest *request = *it;
        MORDOR_ASSERT(request->m_requestState < ServerRequest::COMPLETE ||
            request->m_responseState < ServerRequest::COMPLETE);
        if (seenResponseNotDone) {
            MORDOR_ASSERT(request->m_responseState < ServerRequest::COMPLETE);
        } else {
            seenResponseNotDone = request->m_responseState
                < ServerRequest::COMPLETE;
        }
        if (request->m_requestState < ServerRequest::COMPLETE) {
            ++it;
            MORDOR_ASSERT(it == m_pendingRequests.end());
            break;
        }
    }
    for (std::set<ServerRequest *>::const_iterator it(m_waitingResponses.begin());
        it != m_waitingResponses.end();
        ++it) {
        MORDOR_ASSERT((*it)->m_responseState == ServerRequest::WAITING);
    }
}


ServerRequest::ServerRequest(ServerConnection::ptr conn)
: m_conn(conn),
  m_requestNumber(++conn->m_requestCount),
  m_scheduler(NULL),
  m_requestState(HEADERS),
  m_responseState(PENDING),
  m_willClose(false),
  m_pipeline(false)
{
    std::ostringstream os;
    os << m_conn << "-" << m_requestNumber;
    m_context = os.str();
}

ServerRequest::~ServerRequest()
{
    cancel();
#ifndef NDEBUG
    MORDOR_ASSERT(m_conn);
    boost::mutex::scoped_lock lock(m_conn->m_mutex);
    MORDOR_ASSERT(std::find(m_conn->m_pendingRequests.begin(),
        m_conn->m_pendingRequests.end(),
        this) == m_conn->m_pendingRequests.end());
    MORDOR_ASSERT(m_conn->m_waitingResponses.find(this) ==
        m_conn->m_waitingResponses.end());
#endif
}

bool
ServerRequest::hasRequestBody() const
{
    if (m_requestStream)
        return true;
    return Connection::hasMessageBody(m_request.general,
        m_request.entity,
        m_request.requestLine.method,
        INVALID);
}

Stream::ptr
ServerRequest::requestStream()
{
    MORDOR_ASSERT(!m_requestMultipart);
    MORDOR_ASSERT(m_request.entity.contentType.type != "multipart");
    if (m_requestStream)
        return m_requestStream;
    return m_requestStream = m_conn->getStream(m_request.general, m_request.entity,
        m_request.requestLine.method, INVALID,
        boost::bind(&ServerRequest::requestDone, this),
        boost::bind(&ServerRequest::cancel, this), true);
}

Multipart::ptr
ServerRequest::requestMultipart()
{
    if (m_requestMultipart)
        return m_requestMultipart;
    MORDOR_ASSERT(m_request.entity.contentType.type == "multipart");
    MORDOR_ASSERT(!m_requestStream);
    StringMap::const_iterator it = m_request.entity.contentType.parameters.find("boundary");
    if (it == m_request.entity.contentType.parameters.end() || it->second.empty()) {
        throw std::runtime_error("No boundary with multipart");
    }
    m_requestStream = m_conn->getStream(m_request.general, m_request.entity,
        m_request.requestLine.method, INVALID,
        NULL,
        boost::bind(&ServerRequest::cancel, this), true);
    m_requestMultipart.reset(new Multipart(m_requestStream, it->second));
    m_requestMultipart->multipartFinished = boost::bind(&ServerRequest::requestDone, this);
    return m_requestMultipart;
}

const EntityHeaders &
ServerRequest::requestTrailer() const
{
    // If transferEncoding is not empty, it must include chunked,
    // and it must include chunked in order to have a trailer
    MORDOR_ASSERT(!m_request.general.transferEncoding.empty());
    MORDOR_ASSERT(m_requestState == COMPLETE);
    return m_requestTrailer;
}

bool
ServerRequest::hasResponseBody() const
{
    if (m_responseStream)
        return true;
    return Connection::hasMessageBody(m_response.general,
        m_response.entity,
        m_request.requestLine.method,
        m_response.status.status,
        false);
}

Stream::ptr
ServerRequest::responseStream()
{
    MORDOR_ASSERT(!m_responseMultipart);
    MORDOR_ASSERT(m_response.entity.contentType.type != "multipart");
    if (m_responseStream)
        return m_responseStream;
    commit();
    return m_responseStream = m_conn->getStream(m_response.general, m_response.entity,
        m_request.requestLine.method, m_response.status.status,
        boost::bind(&ServerRequest::responseDone, this),
        boost::bind(&ServerRequest::cancel, this), false);
}

Multipart::ptr
ServerRequest::responseMultipart()
{
    if (m_responseMultipart)
        return m_responseMultipart;
    MORDOR_ASSERT(m_response.entity.contentType.type == "multipart");
    MORDOR_ASSERT(!m_responseStream);
    StringMap::const_iterator it = m_response.entity.contentType.parameters.find("boundary");
    if (it == m_response.entity.contentType.parameters.end()) {
        throw std::runtime_error("No boundary with multipart");
    }
    commit();
    m_responseStream = m_conn->getStream(m_response.general, m_response.entity,
        m_request.requestLine.method, m_response.status.status,
        boost::bind(&ServerRequest::responseDone, this),
        boost::bind(&ServerRequest::cancel, this), false);
    m_responseMultipart.reset(new Multipart(m_responseStream, it->second));
    m_responseMultipart->multipartFinished = boost::bind(&ServerRequest::responseMultipartDone, this);
    return m_responseMultipart;
}

EntityHeaders &
ServerRequest::responseTrailer()
{
    MORDOR_ASSERT(!m_response.general.transferEncoding.empty());
    return m_responseTrailer;
}

void
ServerRequest::processNextRequest()
{
    boost::mutex::scoped_lock lock(m_conn->m_mutex);
    m_pipeline = true;
    m_conn->invariant();
    m_conn->scheduleNextRequest(this);
}

void
ServerRequest::cancel()
{
    if (m_requestState >= COMPLETE && m_responseState >= COMPLETE)
        return;
    MORDOR_LOG_TRACE(g_log) << m_context << " aborting";
    boost::mutex::scoped_lock lock(m_conn->m_mutex);
    m_conn->invariant();
    if (m_requestState < COMPLETE)
        m_requestState = ERROR;
    if (m_responseState < COMPLETE)
        m_responseState = ERROR;
    m_conn->m_stream->cancelRead();
    m_conn->m_stream->cancelWrite();
    m_conn->m_priorRequestFailed = (std::min)(m_conn->m_priorRequestFailed,
        m_requestNumber);
    std::list<ServerRequest *>::iterator it =
        std::find(m_conn->m_pendingRequests.begin(),
            m_conn->m_pendingRequests.end(), this);
    if (it != m_conn->m_pendingRequests.end())
        m_conn->m_pendingRequests.erase(it);
    m_conn->scheduleAllWaitingResponses();
}

void
ServerRequest::finish()
{
    if (m_responseState < COMPLETE) {
        if (hasResponseBody())
            MORDOR_LOG_WARNING(g_log) << m_context
                << " incomplete response";
        if (committed() && hasResponseBody()) {
            cancel();
            return;
        }
        commit();
        if (hasResponseBody()) {
            cancel();
            return;
        }
    }
    if (m_requestState == BODY && !m_willClose) {
        if (m_request.entity.contentType.type == "multipart") {
            if (!m_requestMultipart)
                m_requestMultipart = requestMultipart();
            while(m_requestMultipart->nextPart());
        } else {
            if (!m_requestStream)
                m_requestStream = requestStream();
            MORDOR_ASSERT(m_requestStream);
            transferStream(m_requestStream, NullStream::get());
        }
    }
}

void
ServerRequest::doRequest()
{
    MORDOR_ASSERT(m_requestState == HEADERS);

    try {
        // Read and parse headers
        RequestParser parser(m_request);
        try {
            unsigned long long consumed = parser.run(m_conn->m_stream);
            if (consumed == 0 && !parser.error() && !parser.complete()) {
                // EOF
                MORDOR_LOG_TRACE(g_log) << m_conn << " No more request";
                cancel();
                return;
            }
            if (parser.error() || !parser.complete()) {
                m_requestState = ERROR;
                m_conn->m_priorRequestClosed = m_requestNumber;
                respondError(shared_from_this(), BAD_REQUEST, "Unable to parse request.", true);
                return;
            }
        } catch (SocketException &) {
            cancel();
            return;
        } catch (BrokenPipeException &) {
            cancel();
            return;
        } catch (UnexpectedEofException &) {
            cancel();
            return;
        }
        if (g_log->enabled(Log::DEBUG)) {
            std::string webAuth, proxyAuth;
            if (stricmp(m_request.request.authorization.scheme.c_str(), "Basic") == 0) {
                webAuth = m_request.request.authorization.base64;
                m_request.request.authorization.base64 = "<hidden>";
            }
            if (stricmp(m_request.request.proxyAuthorization.scheme.c_str(), "Basic") == 0) {
                proxyAuth = m_request.request.proxyAuthorization.base64;
                m_request.request.proxyAuthorization.base64 = "<hidden>";
            }
            MORDOR_LOG_DEBUG(g_log) << m_context << " " << m_request;
            if (!webAuth.empty())
                m_request.request.authorization.base64 = webAuth;
            if (!proxyAuth.empty())
                m_request.request.proxyAuthorization.base64 = proxyAuth;
        } else {
            MORDOR_LOG_VERBOSE(g_log) << m_context
                << " " << m_request.requestLine;
        }

        if (m_request.requestLine.ver.major != 1) {
            m_requestState = ERROR;
            respondError(shared_from_this(), HTTP_VERSION_NOT_SUPPORTED, "", true);
            return;
        }
        StringSet &connection = m_request.general.connection;
        if (m_request.requestLine.ver == Version(1, 0) &&
            connection.find("Keep-Alive") == connection.end())
            m_willClose = true;
        if (connection.find("close") != connection.end())
            m_willClose = true;

        // Host header required with HTTP/1.1
        if (m_request.requestLine.ver >= Version(1, 1) && m_request.request.host.empty()) {
            m_requestState = ERROR;
            respondError(shared_from_this(), BAD_REQUEST, "Host header is required with HTTP/1.1", true);
            return;
        }

        ParameterizedList &transferEncoding = m_request.general.transferEncoding;
        // Remove identity from the Transfer-Encodings
        for (ParameterizedList::iterator it(transferEncoding.begin());
            it != transferEncoding.end();
            ++it) {
            if (stricmp(it->value.c_str(), "identity") == 0) {
                it = transferEncoding.erase(it);
                --it;
            }
        }
        if (!transferEncoding.empty()) {
            if (stricmp(transferEncoding.back().value.c_str(), "chunked") != 0) {
                m_requestState = ERROR;
                respondError(shared_from_this(), BAD_REQUEST, "The last transfer-coding is not chunked.", true);
                return;
            }
            if (!transferEncoding.back().parameters.empty()) {
                m_requestState = ERROR;
                respondError(shared_from_this(), NOT_IMPLEMENTED, "Unknown parameter to chunked transfer-coding.", true);
                return;
            }
            for (ParameterizedList::const_iterator it(transferEncoding.begin());
                it + 1 != transferEncoding.end();
                ++it) {
                if (stricmp(it->value.c_str(), "chunked") == 0) {
                    m_requestState = ERROR;
                    respondError(shared_from_this(), BAD_REQUEST, "chunked transfer-coding applied multiple times.", true);
                    return;
                } else if (stricmp(it->value.c_str(), "deflate") == 0 ||
                    stricmp(it->value.c_str(), "gzip") == 0 ||
                    stricmp(it->value.c_str(), "x-gzip") == 0) {
                    // Supported transfer-codings
                } else if (stricmp(it->value.c_str(), "compress") == 0 ||
                    stricmp(it->value.c_str(), "x-compress") == 0) {
                    m_requestState = ERROR;
                    respondError(shared_from_this(), NOT_IMPLEMENTED,
                        "compress transfer-coding is not supported");
                    return;
                } else {
                    m_requestState = ERROR;
                    respondError(shared_from_this(), NOT_IMPLEMENTED,
                        "Unrecognized transfer-coding: " + it->value);
                    return;
                }
            }
        }

        // Check expectations
        const ParameterizedKeyValueList &expect = m_request.request.expect;
        for (ParameterizedKeyValueList::const_iterator it(expect.begin());
            it != expect.end();
            ++it) {
            if (stricmp(it->key.c_str(), "100-continue") == 0) {
                if (!it->value.empty() || !it->parameters.empty()) {
                    m_requestState = ERROR;
                    respondError(shared_from_this(), EXPECTATION_FAILED,
                        "Unrecognized parameters to 100-continue expectation");
                    return;
                }
                // http://www.w3.org/Protocols/rfc2616/rfc2616-sec8.html#sec8.2.3
                // A client MUST NOT send an Expect request-header field (section
                // 14.20) with the "100-continue" expectation if it does not intend
                // to send a request body.
                if (!Connection::hasMessageBody(m_request.general, m_request.entity,
                    m_request.requestLine.method, INVALID, false)) {
                    m_requestState = ERROR;
                    respondError(shared_from_this(), BAD_REQUEST,
                        "Cannot use 100-continue expectation without a request body");
                    return;
                }
            } else {
                m_requestState = ERROR;
                respondError(shared_from_this(), EXPECTATION_FAILED,
                    "Unrecognized expectation: " + it->key);
                return;
            }
        }

        // TE is a connection-specific header
        if (!m_request.request.te.empty())
            m_request.general.connection.insert("TE");

        if (!Connection::hasMessageBody(m_request.general, m_request.entity,
            m_request.requestLine.method, INVALID, false)) {
            MORDOR_LOG_TRACE(g_log) << m_context << " no request body";
            m_conn->requestComplete(this);
        } else {
            m_requestState = BODY;
        }
        m_conn->m_dg(shared_from_this());
        finish();
    } catch (OperationAbortedException &) {
        // Do nothing (this occurs when a pipelined request fails because a
        // prior request closed the connection)
    } catch (PriorRequestFailedException &) {
        MORDOR_LOG_ERROR(g_log) << m_context << " Prior request failed since: "
            << m_conn << "-" << m_conn->m_priorRequestFailed;
    } catch (Assertion &) {
         if (m_requestState == ERROR || m_responseState == ERROR)
            throw;
        MORDOR_LOG_ERROR(g_log) << m_context
            << " Unexpected exception: "
            << boost::current_exception_diagnostic_information();
        if (m_responseState < COMPLETE) {
            try {
                if (!committed())
                    respondError(shared_from_this(), INTERNAL_SERVER_ERROR);
                finish();
                // Somebody got our 500; they should report the error, so drop
                // the assertion
                return;
            } catch(...) {
                // Swallow any exceptions that happen while trying to report
                // the error - re-throw the assertion instead
            }
        }
        throw;
    } catch (...) {
        if (m_requestState == ERROR || m_responseState == ERROR)
            return;
        MORDOR_LOG_ERROR(g_log) << m_context
            << " Unexpected exception: "
            << boost::current_exception_diagnostic_information();
        if (m_responseState < COMPLETE) {
            try {
                if (!committed())
                    respondError(shared_from_this(), INTERNAL_SERVER_ERROR);
                finish();
            } catch(...) {
                // Swallow any exceptions that happen while trying to report the error
            }
        }
    }
}


void
ServerRequest::commit()
{
    if (m_responseState != PENDING)
        return;

    if (m_response.general.connection.find("close") != m_response.general.connection.end())
        m_willClose = true;

    if (m_response.status.ver == Version()) {
        m_response.status.ver = m_request.requestLine.ver;
        if (m_response.status.ver == Version())
            m_response.status.ver = Version(1, 1);
    }
    MORDOR_ASSERT(m_response.status.ver == Version(1, 0) ||
           m_response.status.ver == Version(1, 1));

    // Use chunked encoding for undelimited bodies on 1.1, or force the
    // connection to close on 1.0
    if (m_response.entity.contentLength == ~0ull &&
        m_response.general.transferEncoding.empty() &&
        m_response.entity.contentType.type != "multipart") {
        if (m_response.status.ver == Version(1, 1) && isAcceptable(m_request.request.te,
            AcceptValueWithParameters("chunked"), true))
            m_response.general.transferEncoding.push_back("chunked");
        else
            m_willClose = true;
    }

    if (m_willClose)
        m_response.general.connection.insert("close");
    else if (m_response.status.ver == Version(1, 0))
        m_response.general.connection.insert("Keep-Alive");

    if (m_response.status.reason.empty())
        m_response.status.reason = reason(m_response.status.status);

    const ParameterizedList &transferEncoding = m_request.general.transferEncoding;
    // Transfer encodings only allowed on HTTP/1.1+
    MORDOR_ASSERT(m_response.status.ver >= Version(1, 1) || transferEncoding.empty());

    // If any transfer encodings, must include chunked, must have chunked only once, and must be the last one
    if (!transferEncoding.empty()) {
        MORDOR_ASSERT(m_response.status.ver == Version(1, 1));
        MORDOR_ASSERT(transferEncoding.back().value == "chunked");
        for (ParameterizedList::const_iterator it(transferEncoding.begin());
            it + 1 != transferEncoding.end();
            ++it) {
            // Only the last one can be chunked
            MORDOR_ASSERT(it->value != "chunked");
            // identity is only acceptable in the TE header field
            MORDOR_ASSERT(it->value != "identity");
            if (it->value == "gzip" ||
                it->value == "x-gzip" ||
                it->value == "deflate") {
                // Known Transfer-Codings
            } else if (it->value == "compress" ||
                it->value == "x-compress") {
                // Unsupported Transfer-Codings
                MORDOR_ASSERT(false);
            } else {
                // Unknown Transfer-Coding
                MORDOR_ASSERT(false);
            }
        }
    }
    if (m_response.status.status == UNAUTHORIZED) {
        MORDOR_ASSERT(!m_response.response.wwwAuthenticate.empty());
    } else if (m_response.status.status == PROXY_AUTHENTICATION_REQUIRED) {
        MORDOR_ASSERT(!m_response.response.proxyAuthenticate.empty());
    }

    MORDOR_ASSERT(m_response.status.status != INVALID);

    bool wait = false;
    {
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        if (m_conn->m_priorRequestFailed < m_requestNumber ||
            m_conn->m_priorResponseClosed < m_requestNumber) {
            std::list<ServerRequest *>::iterator it;
            it = std::find(m_conn->m_pendingRequests.begin(), m_conn->m_pendingRequests.end(),
                this);
            MORDOR_ASSERT(it != m_conn->m_pendingRequests.end());
            m_conn->m_pendingRequests.erase(it);
            if (m_conn->m_priorRequestFailed < m_requestNumber)
                MORDOR_THROW_EXCEPTION(PriorRequestFailedException());
            else
                MORDOR_THROW_EXCEPTION(ConnectionVoluntarilyClosedException());
        }
        MORDOR_ASSERT(!m_conn->m_pendingRequests.empty());
        ServerRequest *request = m_conn->m_pendingRequests.front();
        if (request != this) {
            m_responseState = WAITING;
            MORDOR_VERIFY(m_conn->m_waitingResponses.insert(this).second);
            m_scheduler = Scheduler::getThis();
            m_fiber = Fiber::getThis();
            wait = true;
            MORDOR_LOG_TRACE(g_log) << m_context << " waiting to respond";
        } else {
            m_responseState = HEADERS;
            MORDOR_LOG_TRACE(g_log) << m_context << " responding";
            if (m_willClose) {
                m_conn->m_priorResponseClosed = m_requestNumber;
                m_conn->scheduleAllWaitingResponses();
            }
        }
    }
    // If we weren't the first response in the queue, wait for someone
    // else to schedule us
    if (wait) {
        Scheduler::yieldTo();
        m_scheduler = NULL;
        m_fiber.reset();
        MORDOR_LOG_TRACE(g_log) << m_context << " responding";
        // Check for problems that occurred while we were waiting
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        if (m_conn->m_priorRequestFailed <= m_requestNumber)
            MORDOR_THROW_EXCEPTION(PriorRequestFailedException());
        else if (m_conn->m_priorResponseClosed <= m_requestNumber)
            MORDOR_THROW_EXCEPTION(ConnectionVoluntarilyClosedException());
        MORDOR_ASSERT(!m_conn->m_pendingRequests.empty());
        MORDOR_ASSERT(m_conn->m_pendingRequests.front() == this);
        if (m_willClose) {
            m_conn->m_priorResponseClosed = m_requestNumber;
            m_conn->scheduleAllWaitingResponses();
        }
        m_responseState = HEADERS;
    }

    try {
        // Write the headers
        std::ostringstream os;
        os << m_response;
        std::string str = os.str();
        if (g_log->enabled(Log::DEBUG)) {
            MORDOR_LOG_DEBUG(g_log) << m_context << " " << str;
        } else {
            MORDOR_LOG_VERBOSE(g_log) << m_context << " " << m_response.status;
        }
        m_conn->m_stream->write(str.c_str(), str.size());

        if (!Connection::hasMessageBody(m_response.general, m_response.entity,
            m_request.requestLine.method, m_response.status.status, false)) {
            MORDOR_LOG_TRACE(g_log) << m_context << " no response body";
            responseDone();
        } else {
            m_responseState = BODY;
        }
    } catch(...) {
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        m_conn->m_priorRequestFailed = (std::min)(m_conn->m_priorRequestFailed,
            m_requestNumber);
        m_conn->scheduleAllWaitingResponses();
        throw;
    }
}


void
ServerRequest::requestDone()
{
    MORDOR_LOG_TRACE(g_log) << m_context << " request complete";
    m_requestStream.reset();
    if (!m_request.general.transferEncoding.empty()) {
        // Read and parse the trailer
        TrailerParser parser(m_requestTrailer);
        parser.run(m_conn->m_stream);
        if (parser.error()) {
            cancel();
            throw std::runtime_error("Error parsing trailer");
        }
        MORDOR_ASSERT(parser.complete());
        MORDOR_LOG_DEBUG(g_log) << m_context << " " << m_requestTrailer;
    }
    m_conn->requestComplete(this);
}

void
ServerRequest::responseMultipartDone()
{
    MORDOR_ASSERT(m_responseStream);
    m_responseStream->close();
}

void
ServerRequest::responseDone()
{
    MORDOR_LOG_TRACE(g_log) << m_context << " response complete";
    if (m_responseStream && m_responseStream->supportsSize() && m_responseStream->supportsTell())
        MORDOR_ASSERT(m_responseStream->size() == m_responseStream->tell());
    m_responseStream.reset();
    if (!m_response.general.transferEncoding.empty() &&
        m_request.requestLine.method != HEAD) {
        std::ostringstream os;
        os << m_responseTrailer << "\r\n";
        std::string str = os.str();
        MORDOR_LOG_DEBUG(g_log) << m_context << " " << str;
        m_conn->m_stream->write(str.c_str(), str.size());
    }
    MORDOR_LOG_INFO(g_log) << m_context << " "
        << m_request.requestLine << " " << m_response.status.status;
    m_conn->responseComplete(this);
}

void
respondError(ServerRequest::ptr request, Status status,
    const std::string &message, bool closeConnection,
    bool clearContentType, bool clearETag)
{
    MORDOR_ASSERT(!request->committed());
    request->response().status.status = status;
    if (closeConnection)
        request->response().general.connection.insert("close");
    request->response().general.transferEncoding.clear();
    request->response().general.trailer.clear();
    request->response().entity.contentLength = message.size();
    if (clearContentType)
        request->response().entity.contentType.type.clear();
    if (clearETag)
        request->response().response.eTag = ETag();
    if (!message.empty()) {
        if (clearContentType)
            request->response().entity.contentType =
                MediaType("text", "plain");
        if (request->request().requestLine.method == HEAD) {
            request->finish();
        } else {
            Stream::ptr responseStream = request->responseStream();
            responseStream->write(message.c_str(), message.size());
            responseStream->close();
        }
    } else {
        request->finish();
    }
}

void
respondStream(ServerRequest::ptr request, Stream &response)
{
    MORDOR_ASSERT(!request->committed());
    unsigned long long size = ~0ull;
    if (response.supportsSize())
        size = response.size();
    bool trailers = (size == ~0ull &&
        isAcceptable(request->request().request.te, "trailers"));
    // Only advertise byte range request if it would be possible on the current
    // request
    if (trailers || size != ~0ull)
        request->response().response.acceptRanges.insert("bytes");
    ParameterizedList &transferEncoding =
        request->response().general.transferEncoding;
    transferEncoding.clear();
    const RangeSet &range = request->request().request.range;
    bool fullEntity = range.empty();
    // Validate range request
    // If we don't know the size, and we don't support trailers, have to send
    // the whole thing
    if (size == ~0ull && !trailers)
        fullEntity = true;
    // If we're using trailers, it only works for a single range
    if (trailers && range.size() > 1)
        fullEntity = true;
    const ETag *ifRange =
        boost::get<ETag>(&request->request().request.ifRange);
    if (!fullEntity && ifRange && !ifRange->unspecified &&
        !ifRange->strongCompare(request->response().response.eTag))
        fullEntity = true;
    // Timestamps with If-Range is not supported (at this level we can't know
    // if it was possible for the entity to have changed multiple times in a
    // single second; see
    // http://www.w3.org/Protocols/rfc2616/rfc2616-sec13.html#sec13.3.3)
    const boost::posix_time::ptime *httpDate =
        boost::get<boost::posix_time::ptime>(
            &request->request().request.ifRange);
    if (httpDate && !httpDate->is_not_a_date_time())
        fullEntity = true;

    // TODO: sort and merge overlapping ranges
    unsigned long long previousLast = 0;
    for (RangeSet::const_iterator it(range.begin());
        it != range.end();
        ++it) {
        if (fullEntity)
            break;
        // Invalid; first is after last
        if (it->first > it->second && it->first != ~0ull) {
            fullEntity = true;
            break;
        }
        // Unsupported - suffix range when we can't determine the size
        if (it->first == ~0ull && size == ~0ull) {
            fullEntity = true;
            break;
        }
        // First byte is beyond end of stream
        if (it->first >= size && size != ~0ull && it->first != ~0ull) {
            respondError(request, REQUESTED_RANGE_NOT_SATISFIABLE);
            return;
        }
        // Suffix range is greater than entire stream
        if (it->first == ~0ull && it->second >= size) {
            fullEntity = true;
            break;
        }
        // Regular range contains entire stream
        if (it->first == 0 && it->second >= size - 1) {
            fullEntity = true;
            break;
        }
        // Unsupported: un-ordered range and stream doesn't support seeking
        if (it != range.begin()) {
            if (it->first <= previousLast && !response.supportsSeek()) {
                fullEntity = true;
                break;
            }
        }
        if (it->first == ~0ull)
            previousLast = size - 1;
        else
            previousLast = it->second;
    }
    // We can satisfy the range request
    if (!fullEntity) {
        // Multiple ranges; must use multipart; we don't support chunked
        // encoding (does it make sense?), so don't advertise or worry about
        // trailers or transfer encodings
        if (range.size() > 1) {
            MediaType contentType = request->response().entity.contentType;
            request->response().entity.contentLength = ~0;
            request->response().entity.contentType.type = "multipart";
            request->response().entity.contentType.subtype = "byteranges";
            request->response().entity.contentType.parameters["boundary"] =
                Multipart::randomBoundary();
            unsigned long long currentPos = 0;

            if (request->request().requestLine.method != HEAD) {
                Multipart::ptr multipart = request->responseMultipart();
                for (RangeSet::const_iterator it(range.begin());
                    it != range.end();
                    ++it) {
                    BodyPart::ptr part = multipart->nextPart();
                    part->headers().contentType = contentType;
                    ContentRange &cr = part->headers().contentRange;
                    cr.instance = size;
                    if (it->first == ~0ull) {
                        if (it->second > size)
                            cr.first = 0;
                        else
                            cr.first = size - it->second;
                    } else {
                        cr.first = it->first;
                        cr.last = (std::min)(it->second, size - 1);
                    }
                    if (response.supportsSeek())
                        response.seek(cr.first);
                    else
                        transferStream(response, NullStream::get(), cr.first - currentPos);
                    transferStream(response, part->stream(), cr.last - cr.first + 1);
                    part->stream()->close();
                    currentPos = cr.last + 1;
                }
                multipart->finish();
            }
        } else {
            // Single range; we'll support compression (advertise it, and use
            // it if requested) and trailers (if necessary)
            request->response().status.status = PARTIAL_CONTENT;
            if (request->request().requestLine.ver >= Version(1, 1)) {
                AcceptListWithParameters available;
                available.push_back(AcceptValueWithParameters("deflate", 1000));
                available.push_back(AcceptValueWithParameters("gzip", 500));
                available.push_back(AcceptValueWithParameters("x-gzip", 500));
                const AcceptValueWithParameters *preferredEncoding =
                    preferred(request->request().request.te, available);
                if (preferredEncoding) {
                    transferEncoding.push_back(preferredEncoding->value);
                    transferEncoding.push_back("chunked");
                }
            }
            // Set up headers (for trailers, or not for trailers)
            ContentRange *cr;
            if (trailers) {
                request->response().general.trailer.insert("Content-Range");
                if (transferEncoding.empty())
                    transferEncoding.push_back("chunked");
                cr = &request->responseTrailer().contentRange;
                cr->first = range.front().first;
                cr->last = range.front().second;
                MORDOR_ASSERT(cr->first != ~0ull);
            } else {
                cr = &request->response().entity.contentRange;
                cr->instance = size;
                if (range.front().first == ~0ull) {
                    if (range.front().second > size)
                        cr->first = 0;
                    else
                        cr->first = size - range.front().second;
                    cr->last = size - 1;
                } else {
                    cr->first = range.front().first;
                    cr->last = (std::min)(range.front().second, size - 1);
                }
                request->response().entity.contentLength = cr->last - cr->first + 1;
            }

            bool isHead = (request->request().requestLine.method == HEAD);
            
            // Seek to the correct position            
            // Skip the seek if it's a HEAD, and we don't need to check for
            // out-of-range because we already know the size
            if (!isHead || trailers) {
                if (response.supportsSeek()) {
                    response.seek(cr->first, Stream::BEGIN);
                } else {
                    // Can't seek, have to do this the old fashioned way
                    unsigned long long transferred = transferStream(
                        response, NullStream::get(), cr->first, UNTILEOF);
                    // Make sure we're not out-of-range
                    if (transferred != cr->first) {
                        respondError(request, REQUESTED_RANGE_NOT_SATISFIABLE);
                        return;
                    }
                }
            }
            if (isHead) {
                // On a head, when we don't know the size of the stream, we
                // just need to verify that the Range isn't beyond the end of
                // the stream (by reading a single byte)
                if (trailers) {
                    char byte;
                    if (response.read(&byte, 1) == 0) {
                        respondError(request, REQUESTED_RANGE_NOT_SATISFIABLE);
                        return;
                    }
                }
            } else {
                unsigned long long transferred = 0;
                // There's an edge case where either the stream is seekable,
                // but not sizable, or just not sizeable, and so we're pointing
                // exactly at eof (or beyond), and don't know it; need to read
                // a byte without committing to see if we can actually satisfy
                // the request
                if (trailers) {
                    char byte;
                    transferred = response.read(&byte, 1);
                    if (transferred == 0) {
                        respondError(request, REQUESTED_RANGE_NOT_SATISFIABLE);
                        return;
                    }
                    request->responseStream()->write(&byte, 1);
                }
                // Actually transfer the requested range
                transferred += transferStream(response,
                    request->responseStream(),
                    cr->last - cr->first + 1 - transferred,
                    trailers ? UNTILEOF : EXACT);
                if (trailers) {
                    // Set how much was actually transferred
                    cr->last = cr->first + transferred - 1;
                    // And discard the rest of the stream to get the size
                    cr->instance = cr->last + transferStream(response,
                        NullStream::get()) + 1;
                } else if (transferred != cr->last - cr->first + 1) {
                    // The stream LIED about its size!
                    MORDOR_THROW_EXCEPTION(UnexpectedEofException());
                }
                request->responseStream()->close();
            }
        }
    } else {
        // User only asked for, or it's only possible to send the full entity
        request->response().entity.contentLength = size;
        if (request->request().requestLine.ver >= Version(1, 1)) {
            AcceptListWithParameters available;
            available.push_back(AcceptValueWithParameters("deflate", 1000));
            available.push_back(AcceptValueWithParameters("gzip", 500));
            available.push_back(AcceptValueWithParameters("x-gzip", 500));
            const AcceptValueWithParameters *preferredEncoding =
                preferred(request->request().request.te, available);
            if (preferredEncoding) {
                transferEncoding.push_back(preferredEncoding->value);
                transferEncoding.push_back("chunked");
            }
        }
        if (request->request().requestLine.method != HEAD) {
            if (size != 0u) {
                transferStream(response, request->responseStream(), size);
                request->responseStream()->close();
            }
        }
    }
    if (request->request().requestLine.method == HEAD)
        request->finish();
}

void
respondStream(ServerRequest::ptr request, Stream::ptr response)
{
    respondStream(request, *response);
}

bool ifMatch(ServerRequest::ptr request, const ETag &eTag)
{
    const std::set<ETag> &ifMatch = request->request().request.ifMatch;

    // Special case *; if it's there, and there's no ETag, fail
    if (ifMatch.size() == 1 && ifMatch.begin()->unspecified) {
        if (eTag.unspecified) {
            respondError(request, PRECONDITION_FAILED);
            return false;
        }
    } else if (!ifMatch.empty()) {
        bool matched = false;
        for (std::set<ETag>::const_iterator it(ifMatch.begin());
            it != ifMatch.end();
            ++it) {
            if (it->strongCompare(eTag)) {
                matched = true;
                break;
            }
        }
        if (!matched) {
            respondError(request, PRECONDITION_FAILED);
            return false;
        }
    }

    const std::string &method = request->request().requestLine.method;
    bool getOrHead = (method == GET || method == HEAD);
    const std::set<ETag> &ifNoneMatch = request->request().request.ifNoneMatch;
    // Special case *; if it's there, and there is an entity, fail
    if (ifNoneMatch.size() == 1 && ifNoneMatch.begin()->unspecified) {
        if (!eTag.unspecified) {
            if (getOrHead)
                request->response().response.eTag = eTag;
            respondError(request, getOrHead ? NOT_MODIFIED : PRECONDITION_FAILED,
                std::string(), false, true, !getOrHead);
            return false;
        }
    } else {
        bool matched = false;
        for (std::set<ETag>::const_iterator it(ifNoneMatch.begin());
            it != ifNoneMatch.end();
            ++it) {
            if (getOrHead && it->weakCompare(eTag)) {
                matched = true;
                break;
            } else if (!getOrHead && it->strongCompare(eTag)) {
                matched = true;
                break;
            }
        }
        if (matched) {
            if (getOrHead)
                request->response().response.eTag = eTag;
            respondError(request,
                getOrHead ? NOT_MODIFIED : PRECONDITION_FAILED, std::string(),
                false, true, !getOrHead);
            return false;
        }
    }

    return true;
}

}}
