// Copyright (c) 2009 - Decho Corp.

#include "client.h"

#include <cassert>

#include <boost/bind.hpp>

#include "common/scheduler.h"
#include "common/streams/stream.h"
#include "parser.h"

HTTP::ClientConnection::ClientConnection(Stream *stream, bool own)
: Connection(stream, own),
  m_currentRequest(m_pendingRequests.end())
{}

HTTP::ClientConnection::~ClientConnection()
{
    assert(m_pendingRequests.empty());
    assert(m_waitingResponses.empty());
}

HTTP::ClientRequest::ptr
HTTP::ClientConnection::request(Request requestHeaders)
{
    RequestLine &requestLine = requestHeaders.requestLine;
    // 1.0, 1.1, or defaulted
    assert(requestLine.ver == Version() ||
           requestLine.ver == Version(1, 0) ||
           requestLine.ver == Version(1, 1));
    // Have to request *something*
    assert(requestLine.uri.isDefined());
    // Host header required with HTTP/1.1
    assert(!requestHeaders.request.host.empty() || requestLine.ver != Version(1, 1));
    // If any transfer encodings, must include chunked, must have chunked only once, and must be the last one
    const ParameterizedList &transferEncoding = requestHeaders.general.transferEncoding;
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
        if (requestHeaders.request.host.empty())
            requestLine.ver = Version(1, 0);
        else
            requestLine.ver = Version(1, 1);
    }
    // If not specified, try to keep the connection open
    StringSet &connection = requestHeaders.general.connection;
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

    ClientRequest::ptr request(new ClientRequest(this, requestHeaders));

    bool firstRequest;
    // Put the request in the queue
    {
        boost::mutex::scoped_lock lock(m_mutex);
        invariant();
        // TODO: exceptions
        firstRequest = m_currentRequest == m_pendingRequests.end();
        m_pendingRequests.push_back(request);
        if (firstRequest) {
            m_currentRequest = m_pendingRequests.end();
            --m_currentRequest;
        }
        if (firstRequest && close) {
            // TODO: prevent subsequent requests on this conn
        }
    }
    // If we weren't the first request in the queue, we have to wait for
    // another request to schedule us
    if (!firstRequest) {
        Scheduler::getThis()->yieldTo();
    }

    try {
        // Do the request
        std::ostringstream os;
        os << requestHeaders;
        std::string str = os.str();
        m_stream->write(str.c_str(), str.size());

        if (!hasMessageBody(requestHeaders.general, requestHeaders.entity, requestLine.method, INVALID)) {
            request->m_requestDone = true;
            scheduleNextRequest();
        }
    } catch(...) {
        // TODO: clear out request queue
        throw;
    }
    
    return request;
}

void
HTTP::ClientConnection::scheduleNextRequest()
{
    bool flush = false;
    {
        boost::mutex::scoped_lock lock(m_mutex);
        invariant();
        assert(m_currentRequest != m_pendingRequests.end());
        assert((*m_currentRequest)->m_fiber == Fiber::getThis());
        assert((*m_currentRequest)->m_requestDone);
        std::list<ClientRequest::ptr>::iterator it(m_currentRequest);
        if (++it == m_pendingRequests.end()) {
            // Do *not* advance m_currentRequest, because we can't let someone else
            // start another request until our flush completes below
            flush = true;
        } else {
            ++m_currentRequest;
            if (m_currentRequest != m_pendingRequests.end()) {
                (*m_currentRequest)->m_scheduler->schedule((*m_currentRequest)->m_fiber);
            }
        }
    }
    if (flush) {
        m_stream->flush();
        boost::mutex::scoped_lock lock(m_mutex);
        invariant();
        assert((*m_currentRequest)->m_fiber == Fiber::getThis());
        ++m_currentRequest;
        // Someone else may have queued up while we were flushing
        if (m_currentRequest != m_pendingRequests.end()) {
            (*m_currentRequest)->m_scheduler->schedule((*m_currentRequest)->m_fiber);
        }
    }
}

void
HTTP::ClientConnection::scheduleNextResponse()
{
    boost::mutex::scoped_lock lock(m_mutex);
    invariant();
    assert(!m_pendingRequests.empty());
    assert(m_pendingRequests.front()->m_responseDone);
    m_pendingRequests.pop_front();
    if (!m_pendingRequests.empty()) {
        std::set<ClientRequest::ptr>::iterator it = m_waitingResponses.find(m_pendingRequests.front());
        if (it != m_waitingResponses.end()) {
            ClientRequest::ptr request = *it;
            m_waitingResponses.erase(it);
            assert(request->m_requestDone);
            assert(!request->m_responseDone);
            request->m_scheduler->schedule(request->m_fiber);
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
        if (request->m_responseDone) {
            assert(request->m_requestDone);
            // Only the first one can be done with the response
            assert(it == m_pendingRequests.begin());
        }
        if (!seenFirstUnrequested) {
            if (!request->m_requestDone) {
                seenFirstUnrequested = true;
                std::list<ClientRequest::ptr>::const_iterator it2(it);
                --it2;
                assert(m_currentRequest == it || m_currentRequest == it2);
            }
        } else {
            assert(!request->m_requestDone);
        }
    }
    if (!seenFirstUnrequested) {
        if (m_currentRequest != m_pendingRequests.end()) {
            std::list<ClientRequest::ptr>::const_iterator it(m_pendingRequests.end());
            --it;
            assert(m_currentRequest == it);
        }
    }
}


HTTP::ClientRequest::ClientRequest(ClientConnection *conn, const Request &request)
: m_conn(conn),
  m_request(request),
  m_requestDone(false),
  m_hasResponse(false),
  m_hasTrailer(false),
  m_responseDone(false),
  m_requestStream(NULL),
  m_responseStream(NULL)
{
    m_scheduler = Scheduler::getThis();
    m_fiber = Fiber::getThis();
}

Stream *
HTTP::ClientRequest::requestStream()
{
    if (m_requestStream)
        return m_requestStream;
    return m_requestStream = m_conn->getStream(m_request.general, m_request.entity,
        m_request.requestLine.method, INVALID,
        boost::bind(&ClientRequest::requestDone, this), false);
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

Stream *
HTTP::ClientRequest::responseStream()
{
    if (m_responseStream)
        return m_responseStream;
    ensureResponse();
    return m_responseStream = m_conn->getStream(m_response.general, m_response.entity,
        m_request.requestLine.method, m_response.status.status,
        boost::bind(&ClientRequest::responseDone, this), true);
}

const HTTP::EntityHeaders &
HTTP::ClientRequest::responseTrailer() const
{
    assert(m_hasTrailer);
    return m_responseTrailer;
}

void
HTTP::ClientRequest::ensureResponse()
{
    if (m_hasResponse)
        return;
    m_hasResponse = true;
    bool wait = false;
    {
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        assert(!m_conn->m_pendingRequests.empty());
        ClientRequest::ptr request = m_conn->m_pendingRequests.front();
        if (request.get() != this) {
            m_conn->m_waitingResponses.insert(shared_from_this());
            wait = true;
        }
    }
    // If we weren't the first response in the queue, wait for someone
    // else to schedule us
    if (wait) {
        Scheduler::getThis()->yieldTo();
    }

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

    if (close) {
        // TODO: error out pending requests
    }

    if (!m_conn->hasMessageBody(m_response.general, m_response.entity,
        m_request.requestLine.method, m_response.status.status)) {
        m_responseDone = true;
        if (close) {
            m_conn->m_stream->close();
        } else {
            m_conn->scheduleNextResponse();
        }
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
    m_requestDone = true;
    m_conn->scheduleNextRequest();
}

void
HTTP::ClientRequest::responseDone()
{
    if (!m_response.general.transferEncoding.empty()) {
        // Read and parse the trailer
        TrailerParser parser(m_responseTrailer);
        parser.run(m_conn->m_stream);
        if (parser.error()) {
            throw std::runtime_error("Error parsing trailer");
        }
        assert(parser.complete());
        m_hasTrailer = true;
    }
    m_responseDone = true;
    m_conn->scheduleNextResponse();
}
