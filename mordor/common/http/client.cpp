// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "client.h"

#include <algorithm>

#include <boost/bind.hpp>

#include "mordor/common/assert.h"
#include "mordor/common/log.h"
#include "mordor/common/scheduler.h"
#include "mordor/common/streams/null.h"
#include "mordor/common/streams/transfer.h"
#include "parser.h"

static Logger::ptr g_log = Log::lookup("mordor.common.http.client");

HTTP::ClientConnection::ClientConnection(Stream::ptr stream)
: Connection(stream),
  m_currentRequest(m_pendingRequests.end()),
  m_allowNewRequests(true),
  m_priorRequestFailed(false),
  m_priorResponseFailed(false),
  m_priorResponseClosed(false)
{}

HTTP::ClientRequest::ptr
HTTP::ClientConnection::request(const Request &requestHeaders)
{
    ClientRequest::ptr request(new ClientRequest(shared_from_this(), requestHeaders));
    request->doRequest();
    return request;
}

void
HTTP::ClientConnection::scheduleNextRequest(ClientRequest::ptr request)
{
    bool flush = false;
    {
        boost::mutex::scoped_lock lock(m_mutex);
        invariant();
        ASSERT(m_currentRequest != m_pendingRequests.end());
        ASSERT(request == *m_currentRequest);
        ASSERT(!request->m_requestDone);
        ASSERT(request->m_requestInFlight);
        std::list<ClientRequest::ptr>::iterator it(m_currentRequest);
        if (++it == m_pendingRequests.end()) {
            // Do *not* advance m_currentRequest, because we can't let someone else
            // start another request until our flush completes below
            flush = true;
        } else {
            request->m_requestInFlight = false;
            request->m_requestDone = true;
            ++m_currentRequest;
            if (m_currentRequest != m_pendingRequests.end()) {
                request = *m_currentRequest;
                request->m_requestInFlight = true;
                request->m_scheduler->schedule(request->m_fiber);
            }
        }
    }
    if (flush) {
        m_stream->flush();
        boost::mutex::scoped_lock lock(m_mutex);
        invariant();
        ClientRequest::ptr request = *m_currentRequest;
        ASSERT(request->m_fiber == Fiber::getThis());
        request->m_requestInFlight = false;
        request->m_requestDone = true;
        ++m_currentRequest;
        // Someone else may have queued up while we were flushing
        if (m_currentRequest != m_pendingRequests.end()) {
            (*m_currentRequest)->m_scheduler->schedule((*m_currentRequest)->m_fiber);
        }
    }
}

void
HTTP::ClientConnection::scheduleNextResponse(ClientRequest::ptr request)
{
    {
        boost::mutex::scoped_lock lock(m_mutex);
        invariant();
        ASSERT(!m_pendingRequests.empty());
        ASSERT(request == m_pendingRequests.front());
        ASSERT(!request->m_responseDone);
        ASSERT(request->m_responseInFlight);
        request->m_responseDone = true;
        request->m_responseInFlight = false;
        m_pendingRequests.pop_front();
        request.reset();
        if (!m_pendingRequests.empty()) {
            request = m_pendingRequests.front();
            ASSERT(!request->m_responseDone);
            ASSERT(!request->m_responseInFlight);
            std::set<ClientRequest::ptr>::iterator it = m_waitingResponses.find(request);
            if (request->m_cancelled) {
                ASSERT(it == m_waitingResponses.end());
                request->m_responseInFlight = true;
            } else if (it != m_waitingResponses.end()) {
                m_waitingResponses.erase(it);                
                request->m_responseInFlight = true;
                request->m_scheduler->schedule(request->m_fiber);
                request.reset();
            } else {
                request.reset();
            }
        }
    }
    if (request.get()) {
        ASSERT(request->m_cancelled);
        request->finish();
    }
}

void
HTTP::ClientConnection::scheduleAllWaitingRequests()
{
    ASSERT(m_priorRequestFailed || m_priorResponseFailed || m_priorResponseClosed);
    // ASSERT(m_mutex.locked());
    
    for (std::list<ClientRequest::ptr>::iterator it(m_currentRequest);
        it != m_pendingRequests.end();
        ) {
        ASSERT(!(*it)->m_requestDone);
        if (!(*it)->m_requestInFlight) {
            (*it)->m_scheduler->schedule((*it)->m_fiber);
            if (m_currentRequest == it) {
                m_currentRequest = it = m_pendingRequests.erase(it);
            } else {
                it = m_pendingRequests.erase(it);
            }
        } else {
            ++it;
        }
    }
}

void
HTTP::ClientConnection::scheduleAllWaitingResponses()
{
    ASSERT(m_priorResponseFailed || m_priorResponseClosed);
    // ASSERT(m_mutex.locked());
    for (std::list<ClientRequest::ptr>::iterator it(m_pendingRequests.begin());
        it != m_currentRequest;) {
        std::set<ClientRequest::ptr>::iterator waiting = m_waitingResponses.find(*it);
        if (waiting != m_waitingResponses.end()) {
            (*it)->m_scheduler->schedule((*it)->m_fiber);            
            it = m_pendingRequests.erase(it);
            m_waitingResponses.erase(waiting);
        } else if ((*it)->m_cancelled) {
            it = m_pendingRequests.erase(it);
        } else {
            ++it;
        }
    }
}

void
HTTP::ClientConnection::invariant() const
{
    // ASSERT(m_mutex.locked());
    bool seenFirstUnrequested = false;
    for (std::list<ClientRequest::ptr>::const_iterator it(m_pendingRequests.begin());
        it != m_pendingRequests.end();
        ++it) {
        ClientRequest::ptr request = *it;
        if (!request->m_requestDone)
            ASSERT(!request->m_responseDone);
        ASSERT(!request->m_responseDone);
        if (!seenFirstUnrequested) {
            if (!request->m_requestDone) {
                seenFirstUnrequested = true;
                ASSERT(m_currentRequest == it);
            } else if (it != m_pendingRequests.begin()) {
                // Response that's not the first can't be in flight
                ASSERT(!request->m_responseInFlight);
            }
        } else {
            ASSERT(!request->m_requestDone);
            // Request that's not the first (caught by previous iteration above)
            // can't be in flight
            ASSERT(!request->m_requestInFlight);            
        }
    }
    if (!seenFirstUnrequested) {
        ASSERT(m_currentRequest == m_pendingRequests.end());
    }
    for (std::set<ClientRequest::ptr>::const_iterator it(m_waitingResponses.begin());
        it != m_waitingResponses.end();
        ++it) {
        ClientRequest::ptr request = *it;
        ASSERT(!request->m_responseDone);
        ASSERT(!request->m_responseInFlight);
        ASSERT(std::find<std::list<ClientRequest::ptr>::const_iterator>
            (m_pendingRequests.begin(), m_currentRequest, request) != m_currentRequest);
    }
}


HTTP::ClientRequest::ClientRequest(ClientConnection::ptr conn, const Request &request)
: m_conn(conn),
  m_request(request),
  m_requestDone(false),
  m_requestInFlight(false),
  m_responseHeadersDone(false),
  m_responseDone(false),
  m_responseInFlight(false),
  m_cancelled(false),
  m_aborted(false),
  m_badTrailer(false),
  m_incompleteTrailer(false)
{
    m_scheduler = Scheduler::getThis();
    m_fiber = Fiber::getThis();
}

const HTTP::Request &
HTTP::ClientRequest::request()
{
    return m_request;
}

Stream::ptr
HTTP::ClientRequest::requestStream()
{
    if (m_requestStream)
        return m_requestStream;
    ASSERT(!m_requestMultipart);
    ASSERT(m_request.entity.contentType.type != "multipart");
    return m_requestStream = m_conn->getStream(m_request.general, m_request.entity,
        m_request.requestLine.method, INVALID,
        boost::bind(&ClientRequest::requestDone, this),
        boost::bind(&ClientRequest::cancel, this, true), false);
}

Multipart::ptr
HTTP::ClientRequest::requestMultipart()
{
    if (m_requestMultipart)
        return m_requestMultipart;
    ASSERT(m_request.entity.contentType.type == "multipart");
    ASSERT(!m_requestStream);
    HTTP::StringMap::const_iterator it = m_request.entity.contentType.parameters.find("boundary");
    if (it == m_request.entity.contentType.parameters.end()) {
        throw MissingMultipartBoundaryError();
    }
    m_requestStream = m_conn->getStream(m_request.general, m_request.entity,
        m_request.requestLine.method, INVALID,
        boost::bind(&ClientRequest::requestDone, this),
        boost::bind(&ClientRequest::cancel, this, true), false);
    m_requestMultipart.reset(new Multipart(m_requestStream, it->second));
    m_requestMultipart->multipartFinished = boost::bind(&ClientRequest::requestMultipartDone, this);
    return m_requestMultipart;
}

HTTP::EntityHeaders &
HTTP::ClientRequest::requestTrailer()
{
    // If transferEncoding is not empty, it must include chunked,
    // and it must include chunked in order to have a trailer
    ASSERT(!m_request.general.transferEncoding.empty());
    return m_requestTrailer;
}

const HTTP::Response &
HTTP::ClientRequest::response()
{
    ensureResponse();
    return m_response;
}

bool
HTTP::ClientRequest::hasResponseBody()
{
    ensureResponse();
    if (m_responseStream)
        return true;
    if (m_responseMultipart)
        return true;
    return Connection::hasMessageBody(m_response.general,
        m_response.entity,
        m_request.requestLine.method,
        m_response.status.status);

}

Stream::ptr
HTTP::ClientRequest::responseStream()
{
    if (m_responseStream)
        return m_responseStream;
    ASSERT(!m_responseMultipart);
    ensureResponse();
    ASSERT(m_response.entity.contentType.type != "multipart");
    return m_responseStream = m_conn->getStream(m_response.general, m_response.entity,
        m_request.requestLine.method, m_response.status.status,
        boost::bind(&ClientRequest::responseDone, this),
        boost::bind(&ClientRequest::cancel, this, true), true);
}

const HTTP::EntityHeaders &
HTTP::ClientRequest::responseTrailer() const
{
    if (m_badTrailer)
        throw BadMessageHeaderException();
    if (m_incompleteTrailer)
        throw IncompleteMessageHeaderException();
    ASSERT(m_responseDone);
    ASSERT(!m_response.general.transferEncoding.empty());
    return m_responseTrailer;
}

Multipart::ptr
HTTP::ClientRequest::responseMultipart()
{
    if (m_responseMultipart)
        return m_responseMultipart;
    ensureResponse();
    ASSERT(m_response.entity.contentType.type == "multipart");
    HTTP::StringMap::const_iterator it = m_response.entity.contentType.parameters.find("boundary");
    if (it == m_response.entity.contentType.parameters.end()) {
        throw MissingMultipartBoundaryError();
    }
    m_responseStream = m_conn->getStream(m_response.general, m_response.entity,
        m_request.requestLine.method, m_response.status.status,
        NULL,
        boost::bind(&ClientRequest::cancel, this, true), true);
    m_responseMultipart.reset(new Multipart(m_responseStream, it->second));
    m_responseMultipart->multipartFinished = boost::bind(&ClientRequest::responseDone, this);
    return m_responseMultipart;
}

void
HTTP::ClientRequest::cancel(bool abort)
{
    if (m_aborted)
        return;
    if (m_cancelled && !abort)
        return;
    m_cancelled = true;
    if (!abort && !m_requestInFlight && !m_responseInFlight) {
        if (!m_requestDone) {
            // Just abandon it
            std::list<ClientRequest::ptr>::iterator it =
                std::find(m_conn->m_pendingRequests.begin(), m_conn->m_pendingRequests.end(), shared_from_this());
            ASSERT(it != m_conn->m_pendingRequests.end());
            m_conn->m_pendingRequests.erase(it);
        }
        return;
    }
    if (!abort && m_requestDone) {
        ASSERT(m_responseInFlight);
        finish();
        return;
    }
    m_aborted = true;
    boost::mutex::scoped_lock lock(m_conn->m_mutex);
    m_conn->invariant();
    (m_requestDone ? m_conn->m_priorResponseFailed : m_conn->m_priorRequestFailed) = true;
    m_conn->scheduleAllWaitingRequests();
    m_conn->m_stream->close(Stream::READ);
    if (m_requestDone) {
        m_conn->scheduleAllWaitingResponses();
        m_conn->m_stream->close(Stream::BOTH);
    }
}

void
HTTP::ClientRequest::finish()
{
    if (!m_requestDone) {
        cancel(true);
        return;
    }
    if (hasResponseBody()) {
        if (m_response.entity.contentType.type == "multipart") {
            if (!m_responseMultipart) {
                m_responseMultipart = responseMultipart();
            }
            while(m_responseMultipart->nextPart());
        } else {
            if (!m_responseStream) {
                m_responseStream = responseStream();
            }
            ASSERT(m_responseStream);
            transferStream(m_responseStream, NullStream::get());
        }
    }
}

void
HTTP::ClientRequest::doRequest()
{
    RequestLine &requestLine = m_request.requestLine;
    // 1.0, 1.1, or defaulted
    ASSERT(requestLine.ver == Version() ||
           requestLine.ver == Version(1, 0) ||
           requestLine.ver == Version(1, 1));
    // Have to request *something*
    ASSERT(requestLine.uri.isDefined());
    // Host header required with HTTP/1.1
    ASSERT(!m_request.request.host.empty() || requestLine.ver != Version(1, 1));
    // If any transfer encodings, must include chunked, must have chunked only once, and must be the last one
    const ParameterizedList &transferEncoding = m_request.general.transferEncoding;
    if (!transferEncoding.empty()) {
        ASSERT(transferEncoding.back().value == "chunked");
        for (ParameterizedList::const_iterator it(transferEncoding.begin());
            it + 1 != transferEncoding.end();
            ++it) {
            // Only the last one can be chunked
            ASSERT(it->value != "chunked");
            // identity is only acceptable in the TE header field
            ASSERT(it->value != "identity");
            if (it->value == "gzip" ||
                it->value == "x-gzip" ||
                it->value == "deflate") {
                // Known Transfer-Codings
                continue;
            } else if (it->value == "compress" ||
                it->value == "x-compress") {
                // Unsupported Transfer-Codings
                ASSERT(false);
            } else {
                // Unknown Transfer-Coding
                ASSERT(false);
            }
        }
    }

    bool close;
    // Default HTTP version... 1.1 if possible
    if (requestLine.ver == Version()) {
        if (m_request.request.host.empty())
            requestLine.ver = Version(1, 0);
        else
            requestLine.ver = Version(1, 1);
    }
    // If not specified, try to keep the connection open
    StringSet &connection = m_request.general.connection;
    if (connection.find("close") == connection.end() && requestLine.ver == Version(1, 0)) {
        connection.insert("Keep-Alive");
    }
    // Determine if we're closing the connection after this request
    if (requestLine.ver == Version(1, 0)) {
        if (connection.find("Keep-Alive") != connection.end()) {
            close = false;
        } else {
            close = true;
            connection.insert("close");
        }
    } else {
        if (connection.find("close") != connection.end()) {
            close = true;
        } else {
            close = false;
        }
    }
    // TE is a connection-specific header
    if (!m_request.request.te.empty())
        m_request.general.connection.insert("TE");

    bool firstRequest;
    // Put the request in the queue
    {
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        if (!m_conn->m_allowNewRequests)
            throw ConnectionVoluntarilyClosedException();
        if (m_conn->m_priorResponseClosed)
            throw ConnectionVoluntarilyClosedException();
        if (m_conn->m_priorRequestFailed || m_conn->m_priorResponseFailed)
            throw PriorRequestFailedException();
        firstRequest = m_conn->m_currentRequest == m_conn->m_pendingRequests.end();
        m_conn->m_pendingRequests.push_back(shared_from_this());
        if (firstRequest) {
            m_conn->m_currentRequest = m_conn->m_pendingRequests.end();
            --m_conn->m_currentRequest;
            m_requestInFlight = true;
        }
        if (close) {
            m_conn->m_allowNewRequests = false;
        }
    }
    // If we weren't the first request in the queue, we have to wait for
    // another request to schedule us
    if (!firstRequest) {
        Scheduler::getThis()->yieldTo();
        // Check for problems that occurred while we were waiting
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        if (m_conn->m_priorResponseClosed)
            throw ConnectionVoluntarilyClosedException();
        if (m_conn->m_priorRequestFailed || m_conn->m_priorResponseFailed)
            throw PriorRequestFailedException();
        m_requestInFlight = true;
    }

    try {
        // Do the request
        std::ostringstream os;
        os << m_request;
        std::string str = os.str();
        LOG_TRACE(g_log) << str;
        m_conn->m_stream->write(str.c_str(), str.size());

        if (!Connection::hasMessageBody(m_request.general, m_request.entity, requestLine.method, INVALID)) {
            m_conn->scheduleNextRequest(shared_from_this());
        }
    } catch(...) {
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        m_conn->m_priorRequestFailed = true;
        m_conn->m_currentRequest = m_conn->m_pendingRequests.erase(m_conn->m_currentRequest);
        m_conn->scheduleAllWaitingRequests();        
        throw;
    }
}

void
HTTP::ClientRequest::ensureResponse()
{
    // TODO: need to queue up other people waiting for this response if m_responseInFlight
    if (m_responseHeadersDone)
        return;
    ASSERT(!m_responseInFlight);
    bool wait = false;
    {
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        if (m_conn->m_priorResponseFailed || m_conn->m_priorResponseClosed) {
            std::list<ClientRequest::ptr>::iterator it;
            it = std::find(m_conn->m_pendingRequests.begin(), m_conn->m_pendingRequests.end(),
                shared_from_this());
            ASSERT(it != m_conn->m_pendingRequests.end());             
            m_conn->m_pendingRequests.erase(it);
            if (m_conn->m_priorResponseClosed)
                throw ConnectionVoluntarilyClosedException();
            else
                throw PriorRequestFailedException();
        }
        ASSERT(!m_conn->m_pendingRequests.empty());
        ClientRequest::ptr request = m_conn->m_pendingRequests.front();
        if (request.get() != this) {
            bool inserted = m_conn->m_waitingResponses.insert(shared_from_this()).second;
            ASSERT(inserted);
            wait = true;
        } else {
            m_responseInFlight = true;
        }
    }
    // If we weren't the first response in the queue, wait for someone
    // else to schedule us
    if (wait) {
        Scheduler::getThis()->yieldTo();
        // Check for problems that occurred while we were waiting
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        if (m_conn->m_priorResponseClosed)
            throw ConnectionVoluntarilyClosedException();
        if (m_conn->m_priorResponseFailed)
            throw PriorRequestFailedException();
    }

    try {
        // Read and parse headers
        ResponseParser parser(m_response);
        parser.run(m_conn->m_stream);
        if (parser.error())
            throw BadMessageHeaderException();
        if (!parser.complete())
            throw IncompleteMessageHeaderException();
        LOG_TRACE(g_log) << m_response;

        bool close = false;
        StringSet &connection = m_response.general.connection;
        if (m_response.status.ver == Version(1, 0)) {
            if (connection.find("Keep-Alive") == connection.end()) {
                close = true;
            }
        } else if (m_response.status.ver == Version(1, 1)) {
            if (connection.find("close") != connection.end()) {
                close = true;
            }
        } else {
            throw BadMessageHeaderException();
        }
        ParameterizedList &transferEncoding = m_response.general.transferEncoding;
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
                throw InvalidTransferEncodingException("The last transfer-coding is not chunked.");
            }
            for (ParameterizedList::const_iterator it(transferEncoding.begin());
                it + 1 != transferEncoding.end();
                ++it) {
                if (stricmp(it->value.c_str(), "chunked") == 0) {
                    throw InvalidTransferEncodingException("chunked transfer-coding applied multiple times");
                } else if (stricmp(it->value.c_str(), "deflate") == 0 ||
                    stricmp(it->value.c_str(), "gzip") == 0 ||
                    stricmp(it->value.c_str(), "x-gzip") == 0) {
                    // Supported transfer-codings
                } else if (stricmp(it->value.c_str(), "compress") == 0 ||
                    stricmp(it->value.c_str(), "x-compress") == 0) {
                    throw InvalidTransferEncodingException("compress transfer-coding is unsupported");
                } else {
                    throw InvalidTransferEncodingException("Unrecognized transfer-coding: " + it->value);
                }
            }
        }

        // If the there is a message body, but it's undelimited, make sure we're
        // closing the connection
        if (Connection::hasMessageBody(m_response.general, m_response.entity,
            m_request.requestLine.method, m_response.status.status) &&
            transferEncoding.empty() && m_response.entity.contentLength == ~0ull &&
            m_response.entity.contentType.type != "multipart") {
            close = true;
        }

        if (close) {
            boost::mutex::scoped_lock lock(m_conn->m_mutex);
            m_conn->invariant();
            m_conn->m_priorResponseClosed = true;
            m_conn->scheduleAllWaitingRequests();
            m_conn->scheduleAllWaitingResponses();
        }
        m_responseHeadersDone = true;

        if (!Connection::hasMessageBody(m_response.general, m_response.entity,
            m_request.requestLine.method, m_response.status.status)) {
            if (close) {
                m_conn->m_stream->close();
            } else {
                m_conn->scheduleNextResponse(shared_from_this());
            }
        }
    } catch (...) {
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        m_conn->m_priorResponseFailed = true;
        ASSERT(m_conn->m_pendingRequests.front() == shared_from_this());
        m_conn->m_pendingRequests.pop_front();
        m_conn->scheduleAllWaitingRequests();
        m_conn->scheduleAllWaitingResponses();
        throw;
    }
}

void
HTTP::ClientRequest::requestMultipartDone()
{
    m_requestStream->close();
}

void
HTTP::ClientRequest::requestDone()
{
    m_requestStream.reset();
    if (!m_request.general.transferEncoding.empty()) {
        std::ostringstream os;
        os << m_requestTrailer << "\r\n";
        std::string str = os.str();
        LOG_TRACE(g_log) << str;
        m_conn->m_stream->write(str.c_str(), str.size());        
    }
    m_conn->scheduleNextRequest(shared_from_this());
}

void
HTTP::ClientRequest::responseDone()
{
    m_responseStream.reset();
    if (!m_response.general.transferEncoding.empty()) {
        // Read and parse the trailer
        TrailerParser parser(m_responseTrailer);
        parser.run(m_conn->m_stream);
        if (parser.error()) {
            cancel(true);
            m_badTrailer = true;
            return;
        }
        if (!parser.complete()) {
            cancel(true);
            m_incompleteTrailer = true;
            return;
        }
        LOG_TRACE(g_log) << m_responseTrailer;
    }
    m_conn->scheduleNextResponse(shared_from_this());
}
