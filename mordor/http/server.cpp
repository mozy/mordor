// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "server.h"

#include <boost/bind.hpp>

#include "mordor/exception.h"
#include "mordor/log.h"
#include "mordor/streams/null.h"
#include "mordor/streams/transfer.h"
#include "parser.h"

namespace Mordor {
namespace HTTP {

static Logger::ptr g_log = Log::lookup("mordor:http:server");

ServerConnection::ServerConnection(Stream::ptr stream, boost::function<void (ServerRequest::ptr)> dg, size_t maxPipelineDepth)
: Connection(stream),
  m_dg(dg),
  m_priorRequestFailed(false),
  m_priorRequestClosed(false),
  m_priorResponseClosed(false),
  m_maxPipelineDepth(maxPipelineDepth)
{
    MORDOR_ASSERT(m_dg);
}

void
ServerConnection::processRequests()
{
    boost::mutex::scoped_lock lock(m_mutex);
    invariant();
    scheduleSingleRequest();
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
ServerConnection::scheduleSingleRequest()
{
    if ((m_pendingRequests.empty() ||
        (m_pendingRequests.back()->m_requestDone &&
        m_pendingRequests.size() < m_maxPipelineDepth)) &&
        !m_priorRequestFailed && !m_priorRequestClosed && !m_priorResponseClosed) {
        ServerRequest::ptr requestPtr(new ServerRequest(shared_from_this()));
        m_pendingRequests.push_back(requestPtr.get());
        MORDOR_LOG_TRACE(g_log) << this << " " << requestPtr.get() << " scheduling request";
        Scheduler::getThis()->schedule(boost::bind(&ServerRequest::doRequest, requestPtr));
    }
}

void
ServerConnection::scheduleNextRequest(ServerRequest *request)
{
    MORDOR_ASSERT(request);
    bool close = false;
    {
        boost::mutex::scoped_lock lock(m_mutex);
        invariant();
        MORDOR_ASSERT(!m_pendingRequests.empty());
        MORDOR_ASSERT(request == m_pendingRequests.back());
        MORDOR_ASSERT(!request->m_requestDone);
        MORDOR_LOG_TRACE(g_log) << this << " " << request << " request complete";
        request->m_requestDone = true;
        if (request->m_responseDone) {
            MORDOR_ASSERT(request == m_pendingRequests.front());
            m_pendingRequests.pop_front();
        }
        close = request->m_willClose;
        if (!close) {
            scheduleSingleRequest();
        } else {
            m_priorRequestClosed = true;
            MORDOR_LOG_TRACE(g_log) << this << " closing";
        }
    }
    if (close && m_stream->supportsHalfClose())
        m_stream->close(Stream::READ);
}

void
ServerConnection::scheduleNextResponse(ServerRequest *request)
{
    {
        boost::mutex::scoped_lock lock(m_mutex);
        invariant();
        MORDOR_ASSERT(!request->m_responseDone);
        MORDOR_ASSERT(request->m_responseInFlight);
        MORDOR_ASSERT(!m_pendingRequests.empty());
        MORDOR_LOG_TRACE(g_log) << this << " " << request << " response complete";
        std::list<ServerRequest *>::iterator it = m_pendingRequests.begin();
        if (request != *it) {
            it = std::find(it, m_pendingRequests.end(), request);
            MORDOR_ASSERT(it != m_pendingRequests.end());
            request->m_responseInFlight = false;
            request->m_responseDone = true;
            m_pendingRequests.erase(it);
            scheduleSingleRequest();
            return;
        }
        ++it;
        if (it != m_pendingRequests.end()) {
            std::set<ServerRequest *>::iterator waitIt(m_waitingResponses.find(*it));
            if (waitIt != m_waitingResponses.end()) {
                request->m_responseInFlight = false;
                request->m_responseDone = true;
                if (request->m_requestDone) {
                    m_pendingRequests.pop_front();
                    scheduleSingleRequest();
                }
                m_waitingResponses.erase(waitIt);
                request = *it;
                request->m_responseInFlight = true;
                MORDOR_LOG_TRACE(g_log) << this << " " << request << " scheduling response";
                request->m_scheduler->schedule(request->m_fiber);
                return;
            }
        }
        MORDOR_LOG_TRACE(g_log) << this << " flushing";
        if (request->m_willClose) {
            m_priorResponseClosed = true;
            MORDOR_LOG_TRACE(g_log) << this << " closing";
        }
    }
    m_stream->flush();
    if (request->m_willClose) {
        try {
            m_stream->close();
        } catch (...) {
            request->cancel();
            throw;
        }
    }
    boost::mutex::scoped_lock lock(m_mutex);
    invariant();
    MORDOR_ASSERT(!m_pendingRequests.empty());
    MORDOR_ASSERT(request == m_pendingRequests.front());
    request->m_responseInFlight = false;
    request->m_responseDone = true;
    if (request->m_requestDone)
        m_pendingRequests.pop_front();

    // Someone else may have queued up while we were flushing
    if (!m_pendingRequests.empty()) {
        request = m_pendingRequests.front();
        std::set<ServerRequest *>::iterator waitIt(m_waitingResponses.find(request));
        if (waitIt != m_waitingResponses.end()) {
            m_waitingResponses.erase(waitIt);
            request->m_responseInFlight = true;
            MORDOR_LOG_TRACE(g_log) << this << " " << request << " scheduling response";
            request->m_scheduler->schedule(request->m_fiber);
            return;
        }
    }
}

void
ServerConnection::scheduleAllWaitingResponses()
{
    MORDOR_ASSERT(m_priorRequestFailed || m_priorResponseClosed);
    // MORDOR_ASSERT(m_mutex.locked());
    MORDOR_LOG_TRACE(g_log) << this << " scheduling all responses";

    for (std::list<ServerRequest *>::iterator it(m_pendingRequests.begin());
        it != m_pendingRequests.end();
        ++it) {
        ServerRequest *request = *it;
        std::set<ServerRequest *>::iterator waiting = m_waitingResponses.find(request);
        if (waiting != m_waitingResponses.end()) {
            MORDOR_LOG_TRACE(g_log) << this << " " << request << " scheduling response";
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
        MORDOR_ASSERT(!(request->m_requestDone && request->m_responseDone));
        if (seenResponseNotDone) {
            MORDOR_ASSERT(!request->m_responseDone);
        } else {
            seenResponseNotDone = !request->m_responseDone;
        }
        if (!request->m_requestDone) {
            ++it;
            MORDOR_ASSERT(it == m_pendingRequests.end());
            break;
        }
    }
    for (std::set<ServerRequest *>::const_iterator it(m_waitingResponses.begin());
        it != m_waitingResponses.end();
        ++it) {
        MORDOR_ASSERT(!(*it)->m_responseDone);
        MORDOR_ASSERT(!(*it)->m_responseInFlight);
    }
}


ServerRequest::ServerRequest(ServerConnection::ptr conn)
: m_conn(conn),
  m_scheduler(NULL),
  m_requestDone(false),
  m_committed(false),
  m_responseDone(false),
  m_responseInFlight(false),
  m_aborted(false),
  m_willClose(false)
{}

ServerRequest::~ServerRequest()
{
    cancel();
#ifdef DEBUG
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
    if (m_requestStream)
        return m_requestStream;
    MORDOR_ASSERT(!m_requestMultipart);
    MORDOR_ASSERT(m_request.entity.contentType.type != "multipart");
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
    MORDOR_ASSERT(m_requestDone);
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
    if (m_responseStream)
        return m_responseStream;
    MORDOR_ASSERT(!m_responseMultipart);
    MORDOR_ASSERT(m_response.entity.contentType.type != "multipart");
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
ServerRequest::cancel()
{
    if (m_requestDone && m_responseDone)
        return;
    if (m_aborted)
        return;
    MORDOR_LOG_TRACE(g_log) << m_conn << " " << this << " aborting";
    m_aborted = true;
    boost::mutex::scoped_lock lock(m_conn->m_mutex);
    m_conn->invariant();
    m_conn->m_priorRequestFailed = true;
    std::list<ServerRequest *>::iterator it =
        std::find(m_conn->m_pendingRequests.begin(),
            m_conn->m_pendingRequests.end(), this);
    MORDOR_ASSERT(it != m_conn->m_pendingRequests.end());
    m_conn->m_pendingRequests.erase(it);
    m_conn->scheduleAllWaitingResponses();
}

void
ServerRequest::finish()
{
    if (m_responseDone)
        return;
    if (committed() && hasResponseBody()) {
        cancel();
        return;
    }
    commit();
    if (hasResponseBody()) {
        cancel();
        return;
    }
    if (!m_requestDone && hasRequestBody() && !m_willClose) {
        if (m_request.entity.contentType.type == "multipart") {
            if (!m_requestMultipart) {
                m_requestMultipart = requestMultipart();
            }
            while(m_requestMultipart->nextPart());
        } else {
            if (!m_requestStream) {
                m_requestStream = requestStream();
            }
            MORDOR_ASSERT(m_requestStream);
            transferStream(m_requestStream, NullStream::get());
        }
    }
}

void
ServerRequest::doRequest()
{
    MORDOR_ASSERT(!m_requestDone);

    try {
        // Read and parse headers
        RequestParser parser(m_request);
        try {
            unsigned long long consumed = parser.run(m_conn->m_stream);
            if (consumed == 0 && !parser.error() && !parser.complete()) {
                // EOF
                cancel();
                return;
            }
            if (parser.error() || !parser.complete()) {
                m_requestDone = true;
                m_conn->m_priorRequestClosed = true;
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
            MORDOR_LOG_DEBUG(g_log) << m_conn << " " << this << " " << m_request;
            if (!webAuth.empty())
                m_request.request.authorization.base64 = webAuth;
            if (!proxyAuth.empty())
                m_request.request.proxyAuthorization.base64 = proxyAuth;
        } else {
            MORDOR_LOG_VERBOSE(g_log) << m_conn << " " << this << " " << m_request.requestLine;
        }

        if (m_request.requestLine.ver.major != 1) {
            m_requestDone = true;
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
            m_requestDone = true;
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
                m_requestDone = true;
                respondError(shared_from_this(), BAD_REQUEST, "The last transfer-coding is not chunked.", true);
                return;
            }
            if (!transferEncoding.back().parameters.empty()) {
                m_requestDone = true;
                respondError(shared_from_this(), NOT_IMPLEMENTED, "Unknown parameter to chunked transfer-coding.", true);
                return;
            }
            for (ParameterizedList::const_iterator it(transferEncoding.begin());
                it + 1 != transferEncoding.end();
                ++it) {
                if (stricmp(it->value.c_str(), "chunked") == 0) {
                    m_requestDone = true;
                    respondError(shared_from_this(), BAD_REQUEST, "chunked transfer-coding applied multiple times.", true);
                    return;
                } else if (stricmp(it->value.c_str(), "deflate") == 0 ||
                    stricmp(it->value.c_str(), "gzip") == 0 ||
                    stricmp(it->value.c_str(), "x-gzip") == 0) {
                    // Supported transfer-codings
                } else if (stricmp(it->value.c_str(), "compress") == 0 ||
                    stricmp(it->value.c_str(), "x-compress") == 0) {
                    m_requestDone = true;
                    respondError(shared_from_this(), NOT_IMPLEMENTED, "compress transfer-coding is not supported", false);
                    return;
                } else {
                    m_requestDone = true;
                    respondError(shared_from_this(), NOT_IMPLEMENTED, "Unrecognized transfer-coding: " + it->value, false);
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
                    m_requestDone = true;
                    respondError(shared_from_this(), EXPECTATION_FAILED, "Unrecognized parameters to 100-continue expectation", false);
                    return;
                }
            } else {
                m_requestDone = true;
                respondError(shared_from_this(), EXPECTATION_FAILED, "Unrecognized expectation: " + it->key, false);
                return;
            }
        }

        // TE is a connection-specific header
        if (!m_request.request.te.empty())
            m_request.general.connection.insert("TE");

        if (!Connection::hasMessageBody(m_request.general, m_request.entity,
            m_request.requestLine.method, INVALID, false)) {
            MORDOR_LOG_TRACE(g_log) << m_conn << " " << this << " no request body";
            m_conn->scheduleNextRequest(this);
        }
        m_conn->m_dg(shared_from_this());
        finish();
    } catch (OperationAbortedException &) {
        // Do nothing (this occurs when a pipelined request fails because a prior request closed the connection
    } catch (Assertion &) {
        throw;
    } catch (...) {
        if (m_aborted)
            return;
        MORDOR_LOG_ERROR(g_log) << this << " Unexpected exception: "
            << boost::current_exception_diagnostic_information();
        if (!m_responseDone) {
            try {
                if (!committed())
                    respondError(shared_from_this(), INTERNAL_SERVER_ERROR);
                finish();
            } catch(...) {
                // Swallow any exceptions that happen while trying to report the error
            }
            boost::mutex::scoped_lock lock(m_conn->m_mutex);
            m_conn->invariant();
            m_conn->m_priorRequestFailed = true;
            m_conn->scheduleAllWaitingResponses();
        }
    }
}


void
ServerRequest::commit()
{
    if (m_committed)
        return;
    m_committed = true;

    if (m_response.general.connection.find("close") != m_response.general.connection.end())
        m_willClose = true;

    if (m_response.status.ver == Version()) {
        m_response.status.ver = m_request.requestLine.ver;
        if (m_response.status.ver == Version())
            m_response.status.ver = Version(1, 1);
    }
    MORDOR_ASSERT(m_response.status.ver == Version(1, 0) ||
           m_response.status.ver == Version(1, 1));

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
        if (m_conn->m_priorRequestFailed || m_conn->m_priorResponseClosed) {
            std::list<ServerRequest *>::iterator it;
            it = std::find(m_conn->m_pendingRequests.begin(), m_conn->m_pendingRequests.end(),
                this);
            MORDOR_ASSERT(it != m_conn->m_pendingRequests.end());
            m_conn->m_pendingRequests.erase(it);
            if (m_conn->m_priorRequestFailed)
                MORDOR_THROW_EXCEPTION(PriorRequestFailedException());
            else
                MORDOR_THROW_EXCEPTION(ConnectionVoluntarilyClosedException());
        }
        MORDOR_ASSERT(!m_conn->m_pendingRequests.empty());
        ServerRequest *request = m_conn->m_pendingRequests.front();
        if (request != this) {
#ifdef DEBUG
            bool inserted =
#endif
            m_conn->m_waitingResponses.insert(this).second;
            MORDOR_ASSERT(inserted);
            wait = true;
            MORDOR_LOG_TRACE(g_log) << m_conn << " " << this << " waiting to respond";
        } else {
            m_responseInFlight = true;
            MORDOR_LOG_TRACE(g_log) << m_conn << " " << this << " responding";
            if (m_willClose) {
                m_conn->m_priorResponseClosed = true;
                m_conn->scheduleAllWaitingResponses();
            }
        }
    }
    // If we weren't the first response in the queue, wait for someone
    // else to schedule us
    if (wait) {
        m_scheduler = Scheduler::getThis();
        m_fiber = Fiber::getThis();
        Scheduler::yieldTo();
        m_scheduler = NULL;
        m_fiber.reset();
        MORDOR_LOG_TRACE(g_log) << m_conn << " " << this << " responding";
        // Check for problems that occurred while we were waiting
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        MORDOR_ASSERT(!m_conn->m_pendingRequests.empty());
        MORDOR_ASSERT(m_conn->m_pendingRequests.front() == this);
        if (m_conn->m_priorRequestFailed)
            MORDOR_THROW_EXCEPTION(PriorRequestFailedException());
        else if (m_conn->m_priorResponseClosed)
            MORDOR_THROW_EXCEPTION(ConnectionVoluntarilyClosedException());
        if (m_willClose) {
            m_conn->m_priorResponseClosed = true;
            m_conn->scheduleAllWaitingResponses();
        }
        m_responseInFlight = true;
    }

    try {
        // Write the headers
        std::ostringstream os;
        os << m_response;
        std::string str = os.str();
        if (g_log->enabled(Log::DEBUG)) {
            MORDOR_LOG_DEBUG(g_log) << m_conn << " " << this << " " << str;
        } else {
            MORDOR_LOG_VERBOSE(g_log) << m_conn << " " << this << " " << m_response.status;
        }
        m_conn->m_stream->write(str.c_str(), str.size());

        if (!Connection::hasMessageBody(m_response.general, m_response.entity, m_request.requestLine.method, m_response.status.status, false)) {
            MORDOR_LOG_TRACE(g_log) << m_conn << " " << this << " " << " no response body";
            responseDone();
        }
    } catch(...) {
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        m_conn->m_priorRequestFailed = true;
        m_conn->scheduleAllWaitingResponses();
        throw;
    }
}


void
ServerRequest::requestDone()
{
    MORDOR_LOG_TRACE(g_log) << m_conn << " " << this << " request complete";
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
        MORDOR_LOG_DEBUG(g_log) << m_conn << " " << this << " " << m_requestTrailer;
    }
    m_conn->scheduleNextRequest(this);
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
    MORDOR_LOG_TRACE(g_log) << m_conn << " " << this << " response complete";
    if (m_responseStream && m_responseStream->supportsSize() && m_responseStream->supportsTell())
        MORDOR_ASSERT(m_responseStream->size() == m_responseStream->tell());
    m_responseStream.reset();
    if (!m_response.general.transferEncoding.empty()) {
        std::ostringstream os;
        os << m_responseTrailer << "\r\n";
        std::string str = os.str();
        MORDOR_LOG_DEBUG(g_log) << m_conn << " " << this << " " << str;
        m_conn->m_stream->write(str.c_str(), str.size());
    }
    MORDOR_LOG_INFO(g_log) << m_conn << " " << this << " " << m_request.requestLine << " " << m_response.status.status;
    m_conn->scheduleNextResponse(this);
    if (!m_requestDone && hasRequestBody() && !m_willClose) {
        if (!m_requestStream)
            m_requestStream = requestStream();
        MORDOR_ASSERT(m_requestStream);
        transferStream(m_requestStream, NullStream::get());
    }
}


void
respondError(ServerRequest::ptr request, Status status, const std::string &message, bool closeConnection)
{
    MORDOR_ASSERT(!request->committed());
    request->response().status.status = status;
    if (closeConnection)
        request->response().general.connection.insert("close");
    request->response().general.transferEncoding.clear();
    request->response().entity.contentLength = message.size();
    request->response().entity.contentType.type.clear();
    if (!message.empty()) {
        request->response().entity.contentType.type = "text";
        request->response().entity.contentType.subtype = "plain";
        Stream::ptr responseStream = request->responseStream();
        responseStream->write(message.c_str(), message.size());
        responseStream->close();
    } else {
        request->finish();
    }
}

void
respondStream(ServerRequest::ptr request, Stream::ptr response)
{
    MORDOR_ASSERT(!request->committed());
    unsigned long long size = ~0ull;
    request->response().response.acceptRanges.insert("bytes");
    if (response->supportsSize()) {
        size = response->size();
    }
    request->response().general.transferEncoding.clear();
    const RangeSet &range = request->request().request.range;
    bool fullEntity = range.empty();
    // Validate range request
    // TODO: sort and merge overlapping ranges
    unsigned long long previousLast = 0;
    for (RangeSet::const_iterator it(range.begin());
        it != range.end();
        ++it) {
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
            if (it->first <= previousLast && !response->supportsSeek()) {
                fullEntity = true;
                break;
            }
        }
        if (it->first == ~0ull)
            previousLast = size - 1;
        else
            previousLast = it->second;
    }
    if (!fullEntity) {
        if (range.size() > 1) {
            MediaType contentType = request->response().entity.contentType;
            request->response().entity.contentLength = ~0;
            request->response().entity.contentType.type = "multipart";
            request->response().entity.contentType.subtype = "byteranges";
            request->response().entity.contentType.parameters["boundary"] = Multipart::randomBoundary();
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
                        cr.last = std::min(it->second, size - 1);
                    }
                    if (response->supportsSeek()) {
                        response->seek(cr.first, Stream::BEGIN);
                    } else {
                        transferStream(response, NullStream::get(), cr.first - currentPos);
                    }
                    transferStream(response, part->stream(), cr.last - cr.first + 1);
                    part->stream()->close();
                    currentPos = cr.last + 1;
                }
                multipart->finish();
            }
        } else {
            ContentRange &cr = request->response().entity.contentRange;
            cr.instance = size;

            if (range.front().first == ~0ull) {
                if (range.front().second > size)
                    cr.first = 0;
                else
                    cr.first = size - range.front().second;
                cr.last = size - 1;
            } else {
                cr.first = range.front().first;
                cr.last = std::min(range.front().second, size - 1);
            }
            request->response().entity.contentLength = cr.last - cr.first + 1;

            if (response->supportsSeek()) {
                try {
                   response->seek(cr.first, Stream::BEGIN);
                } catch (UnexpectedEofException) {
                    respondError(request, REQUESTED_RANGE_NOT_SATISFIABLE);
                    return;
                }
            } else {
                try {
                    transferStream(response, NullStream::get(), cr.first);
                } catch (UnexpectedEofException) {
                    respondError(request, REQUESTED_RANGE_NOT_SATISFIABLE);
                    return;
                }
                if (request->request().requestLine.method != HEAD) {
                    transferStream(response, request->responseStream(), cr.last - cr.first + 1);
                    request->responseStream()->close();
                }
            }
        }
    }
    if (fullEntity) {
        request->response().entity.contentLength = size;
        if (request->request().requestLine.ver >= Version(1, 1)) {
            AcceptListWithParameters available;
            available.push_back(AcceptValueWithParameters("deflate", 1000));
            available.push_back(AcceptValueWithParameters("gzip", 500));
            available.push_back(AcceptValueWithParameters("x-gzip", 500));
            const AcceptValueWithParameters *preferredEncoding =
                preferred(request->request().request.te, available);
            if (preferredEncoding) {
                ValueWithParameters vp;
                vp.value = preferredEncoding->value;
                request->response().general.transferEncoding.push_back(vp);
            }

            if ((size == ~0ull && isAcceptable(request->request().request.te,
                AcceptValueWithParameters("chunked"), true)) ||
                !request->response().general.transferEncoding.empty()) {
                ValueWithParameters vp;
                vp.value = "chunked";
                request->response().general.transferEncoding.push_back(vp);
            } else if (size == ~0ull) {
                request->response().general.connection.insert("close");
            }
        } else if (size == ~0ull) {
            request->response().general.connection.insert("close");
        }
        if (request->request().requestLine.method != HEAD) {
            transferStream(response, request->responseStream());
            request->responseStream()->close();
        }
    }
    if (request->request().requestLine.method == HEAD)
        request->finish();
}

}}
