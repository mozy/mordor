// Copyright (c) 2009 - Decho Corp.

#include "client.h"

#include <algorithm>
#include <cassert>

#include <boost/bind.hpp>

#include "common/scheduler.h"
#include "common/streams/null.h"
#include "common/streams/transfer.h"
#include "parser.h"

HTTP::ClientConnection::ClientConnection(Stream *stream, bool own)
: Connection(stream, own),
  m_currentRequest(m_pendingRequests.end()),
  m_allowNewRequests(true),
  m_requestException(""),
  m_responseException("")
{}

HTTP::ClientConnection::~ClientConnection()
{
    assert(m_pendingRequests.empty());
    assert(m_waitingResponses.empty());
}

HTTP::ClientRequest::ptr
HTTP::ClientConnection::request(const Request &requestHeaders)
{
    ClientRequest::ptr request(new ClientRequest(this, requestHeaders));
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
        assert(m_currentRequest != m_pendingRequests.end());
        assert(request == *m_currentRequest);
        assert(!request->m_requestDone);
        assert(request->m_inFlight);
        std::list<ClientRequest::ptr>::iterator it(m_currentRequest);
        if (++it == m_pendingRequests.end()) {
            // Do *not* advance m_currentRequest, because we can't let someone else
            // start another request until our flush completes below
            flush = true;
        } else {
            request->m_inFlight = false;
            request->m_requestDone = true;
            ++m_currentRequest;
            if (m_currentRequest != m_pendingRequests.end()) {
                request = *m_currentRequest;
                request->m_inFlight = true;
                request->m_scheduler->schedule(request->m_fiber);
            }
        }
    }
    if (flush) {
        m_stream->flush();
        boost::mutex::scoped_lock lock(m_mutex);
        invariant();
        ClientRequest::ptr request = *m_currentRequest;
        assert(request->m_fiber == Fiber::getThis());
        request->m_inFlight = false;
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
        assert(!m_pendingRequests.empty());
        assert(request == m_pendingRequests.front());
        assert(!request->m_responseDone);
        assert(request->m_inFlight);
        request->m_responseDone = true;
        request->m_inFlight = false;
        m_pendingRequests.pop_front();
        request.reset();
        if (!m_pendingRequests.empty()) {
            request = m_pendingRequests.front();
            assert(request->m_requestDone);
            assert(!request->m_responseDone);
            assert(!request->m_inFlight);
            std::set<ClientRequest::ptr>::iterator it = m_waitingResponses.find(request);
            if (request->m_cancelled) {
                assert(it == m_waitingResponses.end());
                request->m_inFlight = true;
            } else if (it != m_waitingResponses.end()) {
                m_waitingResponses.erase(it);                
                request->m_inFlight = true;
                request->m_scheduler->schedule(request->m_fiber);
                request.reset();
            } else {
                request.reset();
            }
        }
    }
    if (request.get()) {
        assert(request->m_cancelled);
        request->finish();
    }
}

void
HTTP::ClientConnection::scheduleAllWaitingRequests()
{
    assert(*m_requestException.what() || *m_responseException.what());
    // assert(m_mutex.locked());
    
    for (std::list<ClientRequest::ptr>::iterator it(m_currentRequest);
        it != m_pendingRequests.end();
        ) {
        assert(!(*it)->m_requestDone);
        if (!(*it)->m_inFlight) {
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
    assert(*m_responseException.what());
    // assert(m_mutex.locked());
    for (std::list<ClientRequest::ptr>::iterator it(m_pendingRequests.begin());
        it != m_currentRequest;
        ++it) {
        std::set<ClientRequest::ptr>::iterator waiting = m_waitingResponses.find(*it);
        if (waiting != m_waitingResponses.end()) {
            (*it)->m_scheduler->schedule((*it)->m_fiber);            
            it = m_pendingRequests.erase(it);
            --it;
            m_waitingResponses.erase(waiting);
        } else if ((*it)->m_cancelled) {
            it = m_pendingRequests.erase(it);
            --it;
        }
    }
}

void
HTTP::ClientConnection::invariant() const
{
    // assert(m_mutex.locked());
    bool seenFirstUnrequested = false;
    for (std::list<ClientRequest::ptr>::const_iterator it(m_pendingRequests.begin());
        it != m_pendingRequests.end();
        ++it) {
        ClientRequest::ptr request = *it;
        if (!request->m_requestDone)
            assert(!request->m_responseDone);
        assert (!request->m_responseDone);
        if (!seenFirstUnrequested) {
            if (!request->m_requestDone) {
                seenFirstUnrequested = true;
                assert(m_currentRequest == it);
            } else if (it != m_pendingRequests.begin()) {
                // Response that's not the first can't be in flight
                assert(!request->m_inFlight);
            }
        } else {
            assert(!request->m_requestDone);
            // Request that's not the first (caught by previous iteration above)
            // can't be in flight
            assert(!request->m_inFlight);            
        }
    }
    if (!seenFirstUnrequested) {
        assert(m_currentRequest == m_pendingRequests.end());
    }
    for (std::set<ClientRequest::ptr>::const_iterator it(m_waitingResponses.begin());
        it != m_waitingResponses.end();
        ++it) {
        ClientRequest::ptr request = *it;
        assert(request->m_requestDone);
        assert(!request->m_responseDone);
        assert(!request->m_inFlight);
        assert(std::find<std::list<ClientRequest::ptr>::const_iterator>
            (m_pendingRequests.begin(), m_currentRequest, request) != m_currentRequest);
    }
}


HTTP::ClientRequest::ClientRequest(ClientConnection *conn, const Request &request)
: m_conn(conn),
  m_request(request),
  m_requestDone(false),
  m_responseHeadersDone(false),
  m_responseDone(false),
  m_inFlight(false),
  m_cancelled(false),
  m_aborted(false),
  m_requestStream(NULL),
  m_responseStream(NULL)
{
    m_scheduler = Scheduler::getThis();
    m_fiber = Fiber::getThis();
}

Stream *
HTTP::ClientRequest::requestStream()
{
    assert(m_request.entity.contentType.type != "multipart");
    if (m_requestStream)
        return m_requestStream;
    return m_requestStream = m_conn->getStream(m_request.general, m_request.entity,
        m_request.requestLine.method, INVALID,
        boost::bind(&ClientRequest::requestDone, this),
        boost::bind(&ClientRequest::cancel, this, true), false);
}

HTTP::EntityHeaders &
HTTP::ClientRequest::requestTrailer()
{
    // If transferEncoding is not empty, it must include chunked,
    // and it must include chunked in order to have a trailer
    assert(!m_request.general.transferEncoding.empty());
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
    assert(m_response.entity.contentType.type != "multipart");
    if (m_responseStream)
        return true;
    return Connection::hasMessageBody(m_response.general,
        m_response.entity,
        m_request.requestLine.method,
        m_response.status.status);

}

Stream *
HTTP::ClientRequest::responseStream()
{
    if (m_responseStream)
        return m_responseStream;
    ensureResponse();
    return m_responseStream = m_conn->getStream(m_response.general, m_response.entity,
        m_request.requestLine.method, m_response.status.status,
        boost::bind(&ClientRequest::responseDone, this),
        boost::bind(&ClientRequest::cancel, this, true), true);
}

const HTTP::EntityHeaders &
HTTP::ClientRequest::responseTrailer() const
{
    return m_responseTrailer;
}

void
HTTP::ClientRequest::cancel(bool abort)
{
    if (m_aborted)
        return;
    if (m_cancelled && !abort)
        return;
    m_cancelled = true;
    if (!abort && !m_inFlight) {
        if (!m_requestDone) {
            // Just abandon it
            std::list<ClientRequest::ptr>::iterator it =
                std::find(m_conn->m_pendingRequests.begin(), m_conn->m_pendingRequests.end(), shared_from_this());
            assert(it != m_conn->m_pendingRequests.end());
            m_conn->m_pendingRequests.erase(it);
        }
        return;
    }
    if (!abort && m_requestDone) {
        assert(m_inFlight);
        finish();
        return;
    }
    m_aborted = true;
    boost::mutex::scoped_lock lock(m_conn->m_mutex);
    m_conn->invariant();
    (m_requestDone ? m_conn->m_responseException : m_conn->m_requestException) =
        std::runtime_error("No more requests are possible because a prior request failed");
    m_conn->scheduleAllWaitingRequests();
    if (m_requestDone)
        m_conn->scheduleAllWaitingResponses();
}

void
HTTP::ClientRequest::finish()
{
    if (!m_requestDone)
        cancel(true);
    if (hasResponseBody()) {
        std::auto_ptr<Stream> responsePtr;
        if (!m_responseStream) {
            responsePtr.reset(responseStream());
        }
        assert(m_responseStream);
        transferStream(m_responseStream, NullStream::get());
    }
}

void
HTTP::ClientRequest::doRequest()
{
    RequestLine &requestLine = m_request.requestLine;
    // 1.0, 1.1, or defaulted
    assert(requestLine.ver == Version() ||
           requestLine.ver == Version(1, 0) ||
           requestLine.ver == Version(1, 1));
    // Have to request *something*
    assert(requestLine.uri.isDefined());
    // Host header required with HTTP/1.1
    assert(!m_request.request.host.empty() || requestLine.ver != Version(1, 1));
    // If any transfer encodings, must include chunked, must have chunked only once, and must be the last one
    const ParameterizedList &transferEncoding = m_request.general.transferEncoding;
    if (!transferEncoding.empty()) {
        assert(transferEncoding.back().value == "chunked");
        for (ParameterizedList::const_iterator it(transferEncoding.begin());
            it + 1 != transferEncoding.end();
            ++it) {
            // Only the last one can be chunked
            assert(it->value != "chunked");
            // identity is only acceptable in the TE header field
            assert(it->value != "identity");
            if (it->value == "gzip" ||
                it->value == "x-gzip" ||
                it->value == "deflate") {
                // Known Transfer-Codings; TODO: just break
                assert(false);
            } else if (it->value == "compress" ||
                it->value == "x-compress") {
                // Unsupported Transfer-Codings
                assert(false);
            } else {
                // Unknown Transfer-Coding
                assert(false);
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

    bool firstRequest;
    // Put the request in the queue
    {
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        if (!m_conn->m_allowNewRequests) {
            throw std::runtime_error("No more requests are possible because the connection was voluntarily closed");
        }
        if (*m_conn->m_responseException.what()) {
            throw m_conn->m_requestException;
        }
        if (*m_conn->m_requestException.what()) {
            throw m_conn->m_requestException;
        }
        firstRequest = m_conn->m_currentRequest == m_conn->m_pendingRequests.end();
        m_conn->m_pendingRequests.push_back(shared_from_this());
        if (firstRequest) {
            m_conn->m_currentRequest = m_conn->m_pendingRequests.end();
            --m_conn->m_currentRequest;
            m_inFlight = true;
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
        if (*m_conn->m_responseException.what()) {
            throw m_conn->m_requestException;
        }
        if (*m_conn->m_requestException.what()) {
            throw m_conn->m_requestException;
        }
        m_inFlight = true;
    }

    try {
        // Do the request
        std::ostringstream os;
        os << m_request;
        std::string str = os.str();
        m_conn->m_stream->write(str.c_str(), str.size());

        if (!Connection::hasMessageBody(m_request.general, m_request.entity, requestLine.method, INVALID)) {
            m_conn->scheduleNextRequest(shared_from_this());
        }
    } catch(...) {
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        m_conn->m_requestException = std::runtime_error("No more requests are possible because a prior request failed");
        m_conn->m_currentRequest = m_conn->m_pendingRequests.erase(m_conn->m_currentRequest);
        m_conn->scheduleAllWaitingRequests();        
        throw;
    }
}

void
HTTP::ClientRequest::ensureResponse()
{
    assert(m_requestDone);
    if (m_responseHeadersDone)
        return;
    assert(!m_inFlight);
    bool wait = false;
    {
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        if (*m_conn->m_responseException.what()) {
            std::list<ClientRequest::ptr>::iterator it;
            it = std::find(m_conn->m_pendingRequests.begin(), m_conn->m_pendingRequests.end(),
                shared_from_this());
            assert(it != m_conn->m_pendingRequests.end());             
            m_conn->m_pendingRequests.erase(it);
            throw m_conn->m_responseException;
        }
        assert(!m_conn->m_pendingRequests.empty());
        ClientRequest::ptr request = m_conn->m_pendingRequests.front();
        if (request.get() != this) {
            bool inserted = m_conn->m_waitingResponses.insert(shared_from_this()).second;
            assert(inserted);
            wait = true;
        } else {
            m_inFlight = true;
        }
    }
    // If we weren't the first response in the queue, wait for someone
    // else to schedule us
    if (wait) {
        Scheduler::getThis()->yieldTo();
        // Check for problems that occurred while we were waiting
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        if (*m_conn->m_responseException.what()) {
            throw m_conn->m_responseException;
        }
    }

    try {
        // Read and parse headers
        ResponseParser parser(m_response);
        parser.run(m_conn->m_stream);
        if (parser.error()) {
            throw std::runtime_error("Error parsing response");
        }
        assert(parser.complete());

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
            throw std::runtime_error("Unrecognized HTTP server version.");
        }
        ParameterizedList &transferEncoding = m_response.general.transferEncoding;
        // Remove identity from the Transfer-Encodings
        for (ParameterizedList::iterator it(transferEncoding.begin());
            it != transferEncoding.end();
            ++it) {
            if (it->value == "identity") {
                it = transferEncoding.erase(it);
                --it;
            }
        }
        if (!transferEncoding.empty()) {
            if (transferEncoding.back().value != "chunked") {
                throw std::runtime_error("The last transfer-coding is not chunked.");
            }
            for (ParameterizedList::const_iterator it(transferEncoding.begin());
                it + 1 != transferEncoding.end();
                ++it) {
                if (it->value == "chunked") {
                    throw std::runtime_error("chunked transfer-coding applied multiple times");
                } else if (it->value == "deflate" ||
                    it->value == "gzip" ||
                    it->value == "x-gzip") {
                    // Supported transfer-codings
                    throw std::runtime_error("deflate and gzip transfer-codings are not yet supported");
                } else if (it->value == "compress" ||
                    it->value == "x-compress") {
                    throw std::runtime_error("compress transfer-coding is unsupported");
                } else {
                    throw std::runtime_error("Unrecognized transfer-coding: " + it->value);
                }
            }
        }

        // If the there is a message body, but it's undelimited, make sure we're
        // closing the connection
        if (Connection::hasMessageBody(m_response.general, m_response.entity,
            m_request.requestLine.method, m_response.status.status) &&
            transferEncoding.empty() && m_response.entity.contentLength == ~0 &&
            m_response.entity.contentType.type != "multipart") {
            close = true;
        }

        if (close) {
            boost::mutex::scoped_lock lock(m_conn->m_mutex);
            m_conn->invariant();
            m_conn->m_responseException = std::runtime_error("No more requests are possible because the connection was voluntarily closed");
            m_conn->scheduleAllWaitingRequests();
            m_conn->scheduleAllWaitingResponses();
        }
        m_responseHeadersDone = true;

        if (!Connection::hasMessageBody(m_response.general, m_response.entity,
            m_request.requestLine.method, m_response.status.status)) {
            m_responseDone = true;
            if (close) {
                m_conn->m_stream->close();
            } else {
                m_conn->scheduleNextResponse(shared_from_this());
            }
        }
    } catch (...) {
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        m_conn->m_responseException = std::runtime_error("No more requests are possible because a prior request failed");
        assert(m_conn->m_pendingRequests.front() == shared_from_this());
        m_conn->m_pendingRequests.pop_front();
        m_conn->scheduleAllWaitingRequests();
        m_conn->scheduleAllWaitingResponses();
        throw;
    }
}

void
HTTP::ClientRequest::requestDone()
{
    if (!m_request.general.transferEncoding.empty()) {
        std::ostringstream os;
        os << m_requestTrailer;
        std::string str = os.str();;
        m_conn->m_stream->write(str.c_str(), str.size());        
    }
    m_conn->scheduleNextRequest(shared_from_this());
}

void
HTTP::ClientRequest::responseDone()
{
    if (!m_response.general.transferEncoding.empty()) {
        // Read and parse the trailer
        TrailerParser parser(m_responseTrailer);
        parser.run(m_conn->m_stream);
        if (parser.error()) {
            cancel(true);
            throw std::runtime_error("Error parsing trailer");
        }
        assert(parser.complete());
    }
    m_conn->scheduleNextResponse(shared_from_this());
}
