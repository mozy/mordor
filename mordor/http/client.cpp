// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "client.h"

#include <algorithm>

#include <boost/bind.hpp>

#include "mordor/assert.h"
#include "mordor/log.h"
#include "mordor/scheduler.h"
#include "mordor/streams/notify.h"
#include "mordor/streams/null.h"
#include "mordor/streams/transfer.h"
#include "parser.h"

namespace Mordor {
namespace HTTP {

static Logger::ptr g_log = Log::lookup("mordor:http:client");

ClientConnection::ClientConnection(Stream::ptr stream)
: Connection(stream),
  m_currentRequest(m_pendingRequests.end()),
  m_allowNewRequests(true),
  m_priorRequestFailed(false),
  m_priorResponseFailed(false),
  m_priorResponseClosed(false)
{}

ClientRequest::ptr
ClientConnection::request(const Request &requestHeaders)
{
    ClientRequest::ptr request(new ClientRequest(shared_from_this(), requestHeaders));
    request->doRequest();
    return request;
}

bool
ClientConnection::newRequestsAllowed()
{
    boost::mutex::scoped_lock lock(m_mutex);
    return m_allowNewRequests && !m_priorResponseClosed &&
        !m_priorRequestFailed && !m_priorResponseFailed;
}

void
ClientConnection::scheduleNextRequest(ClientRequest *request)
{
    bool flush = false;
    {
        boost::mutex::scoped_lock lock(m_mutex);
        invariant();
        MORDOR_ASSERT(m_currentRequest != m_pendingRequests.end());
        MORDOR_ASSERT(request == *m_currentRequest);
        MORDOR_ASSERT(!request->m_requestDone);
        MORDOR_ASSERT(request->m_requestInFlight);
        std::list<ClientRequest *>::iterator it(m_currentRequest);
        ++it;
        if (it == m_pendingRequests.end()) {
            // Do *not* advance m_currentRequest, because we can't let someone else
            // start another request until our flush completes below
            flush = true;
        } else {
            request->m_requestInFlight = false;
            request->m_requestDone = true;
            m_currentRequest = it;
            request = *it;
            request->m_requestInFlight = true;
            MORDOR_ASSERT(request->m_scheduler);
            MORDOR_ASSERT(request->m_fiber);
            request->m_scheduler->schedule(request->m_fiber);
        }        
    }
    if (flush) {
        m_stream->flush();
        boost::mutex::scoped_lock lock(m_mutex);
        invariant();
        request->m_requestInFlight = false;
        request->m_requestDone = true;
        ++m_currentRequest;
        // Someone else may have queued up while we were flushing
        if (m_currentRequest != m_pendingRequests.end()) {
            request = *m_currentRequest;
            request->m_requestInFlight = true;
            MORDOR_ASSERT(request->m_scheduler);
            MORDOR_ASSERT(request->m_fiber);
            request->m_scheduler->schedule(request->m_fiber);
        }
    }
}

void
ClientConnection::scheduleNextResponse(ClientRequest *request)
{
    ClientRequest::ptr currentRequest;
    bool close = false;
    {
        boost::mutex::scoped_lock lock(m_mutex);
        invariant();
        MORDOR_ASSERT(!m_pendingRequests.empty());
        MORDOR_ASSERT(request == m_pendingRequests.front());
        MORDOR_ASSERT(!request->m_responseDone);
        MORDOR_ASSERT(request->m_responseInFlight);
        request->m_responseDone = true;
        request->m_responseInFlight = false;
        m_pendingRequests.pop_front();
        if (m_priorResponseClosed || m_priorResponseFailed)
            close = true;
        if (!m_pendingRequests.empty()) {
            request = m_pendingRequests.front();
            MORDOR_ASSERT(request);
            MORDOR_ASSERT(!request->m_responseDone);
            MORDOR_ASSERT(!request->m_responseInFlight);
            std::set<ClientRequest *>::iterator it = m_waitingResponses.find(request);
            if (request->m_cancelled) {
                MORDOR_ASSERT(it == m_waitingResponses.end());
                request->m_responseInFlight = true;
                currentRequest = request->shared_from_this();
            } else if (it != m_waitingResponses.end()) {
                m_waitingResponses.erase(it);                
                request->m_responseInFlight = true;
                MORDOR_ASSERT(request->m_scheduler);
                MORDOR_ASSERT(request->m_fiber);
                request->m_scheduler->schedule(request->m_fiber);
            }
        }
    }
    if (currentRequest) {
        MORDOR_ASSERT(currentRequest->m_cancelled);
        currentRequest->finish();
    }
    if (close) {
        MORDOR_ASSERT(!currentRequest);
        m_stream->close(Stream::BOTH);
    }
}

void
ClientConnection::scheduleAllWaitingRequests()
{
    MORDOR_ASSERT(m_priorRequestFailed || m_priorResponseFailed || m_priorResponseClosed);
    // MORDOR_ASSERT(m_mutex.locked());
    
    for (std::list<ClientRequest *>::iterator it(m_currentRequest);
        it != m_pendingRequests.end();
        ) {
        ClientRequest *request = *it;
        MORDOR_ASSERT(!request->m_requestDone);
        if (!request->m_requestInFlight) {
            MORDOR_ASSERT(request->m_scheduler);
            MORDOR_ASSERT(request->m_fiber);
            request->m_scheduler->schedule(request->m_fiber);
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
ClientConnection::scheduleAllWaitingResponses()
{
    MORDOR_ASSERT(m_priorResponseFailed || m_priorResponseClosed);
    // MORDOR_ASSERT(m_mutex.locked());
    for (std::list<ClientRequest *>::iterator it(m_pendingRequests.begin());
        it != m_currentRequest;) {
        ClientRequest *request = *it;
        std::set<ClientRequest *>::iterator waiting = m_waitingResponses.find(*it);
        if (waiting != m_waitingResponses.end()) {
            MORDOR_ASSERT(request->m_scheduler);
            MORDOR_ASSERT(request->m_fiber);
            request->m_scheduler->schedule(request->m_fiber);            
            it = m_pendingRequests.erase(it);
            m_waitingResponses.erase(waiting);
        } else if (request->m_cancelled) {
            it = m_pendingRequests.erase(it);
        } else {
            ++it;
        }
    }
}

void
ClientConnection::invariant() const
{
    // MORDOR_ASSERT(m_mutex.locked());
    bool seenFirstUnrequested = false;
    for (std::list<ClientRequest *>::const_iterator it(m_pendingRequests.begin());
        it != m_pendingRequests.end();
        ++it) {
        ClientRequest *request = *it;
        if (!request->m_requestDone)
            MORDOR_ASSERT(!request->m_responseDone);
        MORDOR_ASSERT(!request->m_responseDone);
        if (!seenFirstUnrequested) {
            if (!request->m_requestDone) {
                seenFirstUnrequested = true;
                MORDOR_ASSERT(m_currentRequest == it);
            } else if (it != m_pendingRequests.begin()) {
                // Response that's not the first can't be in flight
                MORDOR_ASSERT(!request->m_responseInFlight);
            }
        } else {
            MORDOR_ASSERT(!request->m_requestDone);
            // Request that's not the first (caught by previous iteration above)
            // can't be in flight
            MORDOR_ASSERT(!request->m_requestInFlight);            
        }
    }
    if (!seenFirstUnrequested)
        MORDOR_ASSERT(m_currentRequest == m_pendingRequests.end());
    for (std::set<ClientRequest *>::const_iterator it(m_waitingResponses.begin());
        it != m_waitingResponses.end();
        ++it) {
        ClientRequest *request = *it;
        MORDOR_ASSERT(request);
        MORDOR_ASSERT(!request->m_responseDone);
        MORDOR_ASSERT(!request->m_responseInFlight);
        MORDOR_ASSERT(std::find<std::list<ClientRequest *>::const_iterator>
            (m_pendingRequests.begin(), m_currentRequest, request) != m_currentRequest);
    }
}


ClientRequest::ClientRequest(ClientConnection::ptr conn, const Request &request)
: m_conn(conn),
  m_scheduler(NULL),
  m_request(request),
  m_requestDone(false),
  m_requestInFlight(false),
  m_responseHeadersDone(false),
  m_responseDone(false),
  m_responseInFlight(false),
  m_cancelled(false),
  m_aborted(false),
  m_badTrailer(false),
  m_incompleteTrailer(false),
  m_hasResponseBody(false)
{
    MORDOR_ASSERT(m_conn);
}

ClientRequest::~ClientRequest()
{
    cancel(true);
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

const Request &
ClientRequest::request()
{
    return m_request;
}

Stream::ptr
ClientRequest::requestStream()
{
    if (m_requestStream)
        return m_requestStream;
    MORDOR_ASSERT(!m_requestMultipart);
    MORDOR_ASSERT(m_request.entity.contentType.type != "multipart");
    MORDOR_ASSERT(!m_requestDone);
    return m_requestStream = m_conn->getStream(m_request.general, m_request.entity,
        m_request.requestLine.method, INVALID,
        boost::bind(&ClientRequest::requestDone, this),
        boost::bind(&ClientRequest::cancel, this, true), false);
}

Multipart::ptr
ClientRequest::requestMultipart()
{
    if (m_requestMultipart)
        return m_requestMultipart;
    MORDOR_ASSERT(m_request.entity.contentType.type == "multipart");
    MORDOR_ASSERT(!m_requestStream);
    MORDOR_ASSERT(!m_requestDone);
    StringMap::const_iterator it = m_request.entity.contentType.parameters.find("boundary");
    if (it == m_request.entity.contentType.parameters.end()) {
        MORDOR_THROW_EXCEPTION(MissingMultipartBoundaryException());
    }
    m_requestStream = m_conn->getStream(m_request.general, m_request.entity,
        m_request.requestLine.method, INVALID,
        boost::bind(&ClientRequest::requestDone, this),
        boost::bind(&ClientRequest::cancel, this, true), false);
    m_requestMultipart.reset(new Multipart(m_requestStream, it->second));
    m_requestMultipart->multipartFinished = boost::bind(&ClientRequest::requestMultipartDone, shared_from_this());
    return m_requestMultipart;
}

EntityHeaders &
ClientRequest::requestTrailer()
{
    // If transferEncoding is not empty, it must include chunked,
    // and it must include chunked in order to have a trailer
    MORDOR_ASSERT(!m_request.general.transferEncoding.empty());
    return m_requestTrailer;
}

const Response &
ClientRequest::response()
{
    ensureResponse();
    return m_response;
}

bool
ClientRequest::hasResponseBody()
{
    ensureResponse();
    if (m_hasResponseBody)
        return true;
    return Connection::hasMessageBody(m_response.general,
        m_response.entity,
        m_request.requestLine.method,
        m_response.status.status);
}

Stream::ptr
ClientRequest::responseStream()
{
    Stream::ptr result = m_responseStream.lock();
    if (result)
        return result;
    // You can only ask for the response stream once
    // (to avoid circular references)
    MORDOR_ASSERT(!m_hasResponseBody);
    MORDOR_ASSERT(!m_responseDone);
    ensureResponse();
    MORDOR_ASSERT(m_response.entity.contentType.type != "multipart");
    result = m_conn->getStream(m_response.general, m_response.entity,
        m_request.requestLine.method, m_response.status.status,
        boost::bind(&ClientRequest::responseDone, shared_from_this()),
        boost::bind(&ClientRequest::cancel, shared_from_this(), true), true);
    m_hasResponseBody = true;
    m_responseStream = result;
    return result;
}

const EntityHeaders &
ClientRequest::responseTrailer() const
{
    if (m_badTrailer)
        MORDOR_THROW_EXCEPTION(BadMessageHeaderException());
    if (m_incompleteTrailer)
        MORDOR_THROW_EXCEPTION(IncompleteMessageHeaderException());
    MORDOR_ASSERT(m_responseDone);
    MORDOR_ASSERT(!m_response.general.transferEncoding.empty());
    return m_responseTrailer;
}

Stream::ptr
ClientRequest::stream()
{
    MORDOR_ASSERT(m_request.requestLine.method == CONNECT);
    ensureResponse();
    MORDOR_ASSERT(m_response.status.status == OK);
    return m_conn->m_stream;
}

Multipart::ptr
ClientRequest::responseMultipart()
{
    Multipart::ptr result = m_responseMultipart.lock();
    if (result)
        return result;
    // You can only ask for the response stream once
    // (to avoid circular references)
    MORDOR_ASSERT(!m_hasResponseBody);
    ensureResponse();
    MORDOR_ASSERT(m_response.entity.contentType.type == "multipart");
    StringMap::const_iterator it = m_response.entity.contentType.parameters.find("boundary");
    if (it == m_response.entity.contentType.parameters.end()) {
        MORDOR_THROW_EXCEPTION(MissingMultipartBoundaryException());
    }
    Stream::ptr stream = m_conn->getStream(m_response.general, m_response.entity,
        m_request.requestLine.method, m_response.status.status,
        NULL,
        boost::bind(&ClientRequest::cancel, shared_from_this(), true), true);
    m_responseStream = stream;
    result.reset(new Multipart(stream, it->second));
    result->multipartFinished = boost::bind(&ClientRequest::responseDone, shared_from_this());
    m_responseMultipart = result;
    return result;
}

void
ClientRequest::cancel(bool abort)
{
    if (m_aborted)
        return;
    if (m_responseDone)
        return;
    if (m_cancelled && !abort)
        return;
    m_cancelled = true;
    if (!abort && !m_requestInFlight && !m_responseInFlight) {
        if (!m_requestDone) {
            // Just abandon it
            std::list<ClientRequest *>::iterator it =
                std::find(m_conn->m_pendingRequests.begin(),
                m_conn->m_pendingRequests.end(), this);
            MORDOR_ASSERT(it != m_conn->m_pendingRequests.end());
            m_conn->m_pendingRequests.erase(it);
            return;
        }
    }
    if (!abort && m_requestDone) {
        if (m_responseInFlight && m_responseHeadersDone) {
            if (m_responseHeadersDone) {
                finish();
                return;
            }
            MORDOR_NOTREACHED();
            // If the response headers aren't done, but the
            // response *is* in flight, we got bad headers, and
            // have to abort
        } else {
            // We haven't even started on this response yet;
            // It's probably safer to just abort it, because we're
            // probably in the destructor as an exception is unwinding the
            // the stack (or just shoddy programming)
        }
    }
    m_aborted = true;
    if (m_requestStream) {
        // Break the circular reference
        NotifyStream::ptr notify =
            boost::dynamic_pointer_cast<NotifyStream>(m_requestStream);
        MORDOR_ASSERT(notify);
        notify->notifyOnClose = NULL;
        notify->notifyOnEof = NULL;
        notify->notifyOnException = NULL;
        notify->parent(Stream::ptr(new Stream()));
    }
    Stream::ptr responseStream = m_responseStream.lock();
    if (responseStream) {
        NotifyStream::ptr notify =
            boost::dynamic_pointer_cast<NotifyStream>(responseStream);
        MORDOR_ASSERT(notify);
        notify->notifyOnClose = NULL;
        notify->notifyOnEof = NULL;
        notify->notifyOnException = NULL;
    }
    boost::mutex::scoped_lock lock(m_conn->m_mutex);
    m_conn->invariant();
    (m_requestDone ? m_conn->m_priorResponseFailed : m_conn->m_priorRequestFailed) = true;
    if (m_requestDone) {
        MORDOR_ASSERT(!m_conn->m_pendingRequests.empty());
        std::list<ClientRequest *>::iterator it =
            std::find(m_conn->m_pendingRequests.begin(),
            m_conn->m_pendingRequests.end(), this);
        MORDOR_ASSERT(it != m_conn->m_pendingRequests.end());
        m_conn->m_pendingRequests.erase(it);
    } else {
        MORDOR_ASSERT(m_conn->m_currentRequest != m_conn->m_pendingRequests.end());
        MORDOR_ASSERT(*m_conn->m_currentRequest == this);
        m_conn->m_currentRequest = m_conn->m_pendingRequests.erase(m_conn->m_currentRequest);
    }
    m_conn->scheduleAllWaitingRequests();
    m_conn->m_stream->close(Stream::READ);
    if (m_requestDone) {
        m_conn->scheduleAllWaitingResponses();
        m_conn->m_stream->close(Stream::BOTH);
    }
}

void
ClientRequest::finish()
{
    if (!m_requestDone) {
        cancel(true);
        return;
    }
    if (hasResponseBody()) {
        if (m_response.entity.contentType.type == "multipart") {
            Multipart::ptr multipart;
            if (m_hasResponseBody)
                multipart = m_responseMultipart.lock();
            else
                multipart = responseMultipart();
            if (!multipart)
                cancel(true);
            else
                while(multipart->nextPart());
        } else {
            Stream::ptr stream;
            if (m_hasResponseBody)
                stream = m_responseStream.lock();
            else
                stream = responseStream();
            if (!stream)
                cancel(true);
            else
                transferStream(stream, NullStream::get());
        }
    }
}

void
ClientRequest::doRequest()
{
    RequestLine &requestLine = m_request.requestLine;
    // 1.0, 1.1, or defaulted
    MORDOR_ASSERT(requestLine.ver == Version() ||
           requestLine.ver == Version(1, 0) ||
           requestLine.ver == Version(1, 1));
    // Have to request *something*
    MORDOR_ASSERT(requestLine.uri.isDefined());
    // Host header required with HTTP/1.1
    MORDOR_ASSERT(!m_request.request.host.empty() || requestLine.ver != Version(1, 1));
    // If any transfer encodings, must include chunked, must have chunked only once, and must be the last one
    const ParameterizedList &transferEncoding = m_request.general.transferEncoding;
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
                continue;
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
        if (!m_conn->m_allowNewRequests) {
            m_aborted = true;
            MORDOR_THROW_EXCEPTION(ConnectionVoluntarilyClosedException());
        }
        if (m_conn->m_priorResponseClosed) {
            m_aborted = true;
            MORDOR_THROW_EXCEPTION(ConnectionVoluntarilyClosedException());
        }
        if (m_conn->m_priorRequestFailed || m_conn->m_priorResponseFailed) {
            m_aborted = true;
            MORDOR_THROW_EXCEPTION(PriorRequestFailedException());
        }
        firstRequest = m_conn->m_currentRequest == m_conn->m_pendingRequests.end();
        m_conn->m_pendingRequests.push_back(this);
        if (firstRequest) {
            m_conn->m_currentRequest = m_conn->m_pendingRequests.end();
            --m_conn->m_currentRequest;
            m_requestInFlight = true;
        } else {
            m_scheduler = Scheduler::getThis();
            m_fiber = Fiber::getThis();
            MORDOR_ASSERT(m_scheduler);
            MORDOR_ASSERT(m_fiber);
        }
        if (close) {
            m_conn->m_allowNewRequests = false;
        }
    }
    // If we weren't the first request in the queue, we have to wait for
    // another request to schedule us
    if (!firstRequest) {
        m_scheduler->yieldTo();
        m_scheduler = NULL;
        m_fiber.reset();
        // Check for problems that occurred while we were waiting
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        if (m_conn->m_priorResponseClosed)
            MORDOR_THROW_EXCEPTION(ConnectionVoluntarilyClosedException());
        if (m_conn->m_priorRequestFailed || m_conn->m_priorResponseFailed)
            MORDOR_THROW_EXCEPTION(PriorRequestFailedException());
        m_requestInFlight = true;
    }

    try {
        // Do the request
        std::ostringstream os;
        os << m_request;
        std::string str = os.str();
        if (g_log->enabled(Log::VERBOSE)) {
            std::string webAuth, proxyAuth;
            if (stricmp(m_request.request.authorization.scheme.c_str(), "Basic") == 0) {
                webAuth = m_request.request.authorization.base64;
                m_request.request.authorization.base64 = "<hidden>";
            }
            if (stricmp(m_request.request.proxyAuthorization.scheme.c_str(), "Basic") == 0) {
                proxyAuth = m_request.request.proxyAuthorization.base64;
                m_request.request.proxyAuthorization.base64 = "<hidden>";
            }
            MORDOR_LOG_VERBOSE(g_log) << this << " " << m_request;
            if (!webAuth.empty())
                m_request.request.authorization.base64 = webAuth;
            if (!proxyAuth.empty())
                m_request.request.proxyAuthorization.base64 = proxyAuth;
        } else {
            MORDOR_LOG_TRACE(g_log) << this << " " << m_request.requestLine;
        }
        m_conn->m_stream->write(str.c_str(), str.size());

        if (!Connection::hasMessageBody(m_request.general, m_request.entity, requestLine.method, INVALID)) {
            m_conn->scheduleNextRequest(this);
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
ClientRequest::ensureResponse()
{
    // TODO: need to queue up other people waiting for this response if m_responseInFlight
    if (m_responseHeadersDone)
        return;
    MORDOR_ASSERT(!m_responseInFlight);
    bool wait = false;
    {
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        if (m_conn->m_priorResponseFailed || m_conn->m_priorResponseClosed) {
            std::list<ClientRequest *>::iterator it;
            it = std::find(m_conn->m_pendingRequests.begin(),
                m_conn->m_pendingRequests.end(), this);
            MORDOR_ASSERT(it != m_conn->m_pendingRequests.end());
            m_aborted = true;
            m_conn->m_pendingRequests.erase(it);
            if (m_conn->m_priorResponseClosed)
                MORDOR_THROW_EXCEPTION(ConnectionVoluntarilyClosedException());
            else
                MORDOR_THROW_EXCEPTION(PriorRequestFailedException());
        }
        MORDOR_ASSERT(!m_conn->m_pendingRequests.empty());
        ClientRequest *request = m_conn->m_pendingRequests.front();
        if (request != this) {
            m_scheduler = Scheduler::getThis();
            m_fiber = Fiber::getThis();
            MORDOR_ASSERT(m_scheduler);
            MORDOR_ASSERT(m_fiber);
#ifdef DEBUG
            bool inserted = 
#endif
            m_conn->m_waitingResponses.insert(this).second;
            MORDOR_ASSERT(inserted);
            wait = true;
        } else {
            m_responseInFlight = true;
        }
    }
    // If we weren't the first response in the queue, wait for someone
    // else to schedule us
    if (wait) {
        m_scheduler->yieldTo();
        m_scheduler = NULL;
        m_fiber.reset();
        // Check for problems that occurred while we were waiting
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        // Probably means that the Scheduler exited in the above yieldTo,
        // and returned to us, because there is no other work to be done
        try {
            MORDOR_ASSERT(!m_conn->m_pendingRequests.empty());
            MORDOR_ASSERT(m_conn->m_pendingRequests.front() == this);
        } catch(...) {
            std::set<ClientRequest *>::iterator it = m_conn->m_waitingResponses.find(this);
            if (it != m_conn->m_waitingResponses.end())
                m_conn->m_waitingResponses.erase(it);
            throw;
        }
        if (m_conn->m_priorResponseClosed || m_conn->m_priorResponseFailed) {
            m_aborted = true;
            m_conn->m_pendingRequests.pop_front();
            if (m_conn->m_priorResponseClosed)
                MORDOR_THROW_EXCEPTION(ConnectionVoluntarilyClosedException());
            else
                MORDOR_THROW_EXCEPTION(PriorRequestFailedException());
        }
    }

    try {
        // Read and parse headers
        ResponseParser parser(m_response);
        parser.run(m_conn->m_stream);
        if (parser.error())
            MORDOR_THROW_EXCEPTION(BadMessageHeaderException());
        if (!parser.complete())
            MORDOR_THROW_EXCEPTION(IncompleteMessageHeaderException());
        if (g_log->enabled(Log::VERBOSE)) {
            MORDOR_LOG_VERBOSE(g_log) << this << " " << m_response;
        } else {
            MORDOR_LOG_TRACE(g_log) << this << " " << m_response.status;
        }

        bool close = false;
        StringSet &connection = m_response.general.connection;
        if (m_response.status.ver == Version(1, 0)) {
            if (connection.find("Keep-Alive") == connection.end())
                close = true;
        } else if (m_response.status.ver == Version(1, 1)) {
            if (connection.find("close") != connection.end())
                close = true;
        } else {
            MORDOR_THROW_EXCEPTION(BadMessageHeaderException());
        }
        // NON-STANDARD!!!
        StringSet &proxyConnection = m_response.general.proxyConnection;
        if (proxyConnection.find("close") != proxyConnection.end())
            close = true;

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
                MORDOR_THROW_EXCEPTION(InvalidTransferEncodingException("The last transfer-coding is not chunked."));
            }
            for (ParameterizedList::const_iterator it(transferEncoding.begin());
                it + 1 != transferEncoding.end();
                ++it) {
                if (stricmp(it->value.c_str(), "chunked") == 0) {
                    MORDOR_THROW_EXCEPTION(InvalidTransferEncodingException("chunked transfer-coding applied multiple times"));
                } else if (stricmp(it->value.c_str(), "deflate") == 0 ||
                    stricmp(it->value.c_str(), "gzip") == 0 ||
                    stricmp(it->value.c_str(), "x-gzip") == 0) {
                    // Supported transfer-codings
                } else if (stricmp(it->value.c_str(), "compress") == 0 ||
                    stricmp(it->value.c_str(), "x-compress") == 0) {
                    MORDOR_THROW_EXCEPTION(InvalidTransferEncodingException("compress transfer-coding is unsupported"));
                } else {
                    MORDOR_THROW_EXCEPTION(InvalidTransferEncodingException("Unrecognized transfer-coding: " + it->value));
                }
            }
        }

        // If the there is a message body, but it's undelimited, make sure we're
        // closing the connection
        bool hasBody = Connection::hasMessageBody(m_response.general, m_response.entity,
            m_request.requestLine.method, m_response.status.status);
        if (hasBody &&
            transferEncoding.empty() && m_response.entity.contentLength == ~0ull &&
            m_response.entity.contentType.type != "multipart") {
            close = true;
        }

        bool connect = m_request.requestLine.method == CONNECT &&
            m_response.status.status == OK;
        if (connect)
            close = true;

        if (close) {
            boost::mutex::scoped_lock lock(m_conn->m_mutex);
            m_conn->invariant();
            m_conn->m_priorResponseClosed = true;
            MORDOR_ASSERT(!m_conn->m_pendingRequests.empty());
            MORDOR_ASSERT(m_conn->m_pendingRequests.front() == this);
            if (!hasBody && !connect)
                m_conn->m_pendingRequests.pop_front();
            m_conn->scheduleAllWaitingRequests();
            m_conn->scheduleAllWaitingResponses();
        }
        m_responseHeadersDone = true;

        if (!hasBody && !connect) {
            if (close) {
                m_responseDone = true;
                m_conn->m_stream->close();
            } else {
                m_conn->scheduleNextResponse(this);
            }
        }
    } catch (...) {
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        m_conn->m_priorResponseFailed = true;
        MORDOR_ASSERT(m_conn->m_pendingRequests.front() == this);
        m_conn->m_pendingRequests.pop_front();
        m_conn->scheduleAllWaitingRequests();
        m_conn->scheduleAllWaitingResponses();
        throw;
    }
}

void
ClientRequest::requestMultipartDone()
{
    MORDOR_ASSERT(m_requestStream);
    m_requestStream->close();
}

void
ClientRequest::requestDone()
{
    if (m_requestDone)
        return;
    MORDOR_ASSERT(m_requestStream);
    // Break the circular reference
    NotifyStream::ptr notify =
        boost::dynamic_pointer_cast<NotifyStream>(m_requestStream);
    MORDOR_ASSERT(notify);
    notify->notifyOnClose = NULL;
    notify->notifyOnEof = NULL;
    notify->notifyOnException = NULL;
    if (m_requestStream->supportsSize()) {
        if (m_requestStream->size() !=
            m_requestStream->seek(0, Stream::CURRENT)) {
            cancel(true);
            MORDOR_THROW_EXCEPTION(UnexpectedEofException());
        }
    }
    if (!m_request.general.transferEncoding.empty()) {
        std::ostringstream os;
        os << m_requestTrailer << "\r\n";
        std::string str = os.str();
        MORDOR_LOG_VERBOSE(g_log) << this << " " << str;
        m_conn->m_stream->write(str.c_str(), str.size());        
    }
    m_conn->scheduleNextRequest(this);
}

void
ClientRequest::responseDone()
{
    if (m_responseDone)
        return;
    // Keep an extra ref to ourself around so we don't destruct if the only ref
    // is in the response stream
    ClientRequest::ptr self;
    try {
        self = shared_from_this();
    } catch (boost::bad_weak_ptr &) {
        // means we're in the destructor
    }
    Stream::ptr stream = m_responseStream.lock();
    MORDOR_ASSERT(stream);
    NotifyStream::ptr notify =
        boost::dynamic_pointer_cast<NotifyStream>(stream);
    MORDOR_ASSERT(notify);
    notify->notifyOnClose = NULL;
    notify->notifyOnEof = NULL;
    notify->notifyOnException = NULL;
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
        MORDOR_LOG_VERBOSE(g_log) << this << " " << m_responseTrailer;
    }
    m_conn->scheduleNextResponse(this);
}

}}
