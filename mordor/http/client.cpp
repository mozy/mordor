// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "client.h"

#include <algorithm>

#include <boost/bind.hpp>

#include "chunked.h"
#include "mordor/assert.h"
#include "mordor/log.h"
#include "mordor/scheduler.h"
#include "mordor/streams/limited.h"
#include "mordor/streams/notify.h"
#include "mordor/streams/null.h"
#include "mordor/streams/timeout.h"
#include "mordor/streams/transfer.h"
#include "mordor/util.h"
#include "parser.h"

namespace Mordor {
namespace HTTP {

static Logger::ptr g_log = Log::lookup("mordor:http:client");

ClientConnection::ClientConnection(Stream::ptr stream, TimerManager *timerManager)
: Connection(stream),
  m_readTimeout(~0ull),
  m_idleTimeout(~0ull),
  m_timerManager(timerManager),
  m_currentRequest(m_pendingRequests.end()),
  m_allowNewRequests(true),
  m_priorRequestFailed(false),
  m_requestCount(0),
  m_priorResponseFailed(~0ull),
  m_priorResponseClosed(~0ull)
{
    if (timerManager) {
        FilterStream::ptr previous;
        FilterStream::ptr filter = boost::dynamic_pointer_cast<FilterStream>(m_stream);
        while (filter) {
            previous = filter;
            filter = boost::dynamic_pointer_cast<FilterStream>(filter->parent());
        }
        // Put the timeout stream as close to the actual source stream as
        // possible, to avoid registering timeouts for stuff that's going to
        // complete immediately anyway
        if (previous) {
            m_timeoutStream.reset(new TimeoutStream(previous->parent(),
                *timerManager));
            previous->parent(m_timeoutStream);
        } else {
            m_timeoutStream.reset(new TimeoutStream(m_stream, *timerManager));
            m_stream = m_timeoutStream;
        }
    }
}

ClientConnection::~ClientConnection()
{
    if (m_idleTimer)
        m_idleTimer->cancel();
}

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
    return m_allowNewRequests && m_priorResponseClosed == ~0ull &&
        !m_priorRequestFailed && m_priorResponseFailed == ~0ull;
}

size_t
ClientConnection::outstandingRequests()
{
    boost::mutex::scoped_lock lock(m_mutex);
    invariant();
    return m_pendingRequests.size();
}

bool
ClientConnection::supportsTimeouts() const
{
    return !!m_timerManager;
}

void
ClientConnection::readTimeout(unsigned long long us)
{
    MORDOR_ASSERT(m_timeoutStream);
    m_readTimeout = us;
}

void
ClientConnection::writeTimeout(unsigned long long us)
{
    MORDOR_ASSERT(m_timeoutStream);
    m_timeoutStream->writeTimeout(us);
}

void
ClientConnection::idleTimeout(unsigned long long us, boost::function<void ()> dg)
{
    MORDOR_ASSERT(m_timerManager);
    boost::mutex::scoped_lock lock(m_mutex);
    if (m_idleTimer) {
        m_idleTimer->cancel();
        m_idleTimer.reset();
    }
    m_idleTimeout = us;
    m_idleDg = dg;
    if (m_idleTimeout != ~0ull && m_pendingRequests.empty())
        m_idleTimer = m_timerManager->registerTimer(m_idleTimeout, dg);
}

void
ClientConnection::scheduleNextRequest(ClientRequest *request)
{
    bool flush = false;
    boost::mutex::scoped_lock lock(m_mutex);
    invariant();
    MORDOR_ASSERT(m_currentRequest != m_pendingRequests.end());
    MORDOR_ASSERT(request == *m_currentRequest);
    MORDOR_ASSERT(request->m_requestState == ClientRequest::BODY);
    MORDOR_LOG_TRACE(g_log) << this << "-" << request->m_requestNumber << " request complete";
    std::list<ClientRequest *>::iterator it(m_currentRequest);
    ++it;
    if (it == m_pendingRequests.end()) {
        // Do *not* advance m_currentRequest, because we can't let someone else
        // start another request until our flush completes below
        flush = true;
    } else {
        request->m_requestState = ClientRequest::COMPLETE;
        if (request->m_responseState >= ClientRequest::COMPLETE) {
            MORDOR_ASSERT(request == m_pendingRequests.front());
            m_pendingRequests.pop_front();
        }
        m_currentRequest = it;
        request = *it;
        request->m_requestState = ClientRequest::HEADERS;
        MORDOR_ASSERT(request->m_scheduler);
        MORDOR_ASSERT(request->m_fiber);
        MORDOR_LOG_TRACE(g_log) << this << "-" << request->m_requestNumber << " scheduling request";
        request->m_scheduler->schedule(request->m_fiber);
        request->m_scheduler = NULL;
        request->m_fiber.reset();
    }
    bool close = false;
    if (flush) {
        // Take a trip through the Scheduler, trying to let someone else
        // attempt to pipeline
        if (Scheduler::getThis()) {
            lock.unlock();
            Scheduler::yield();
            lock.lock();
        }
        invariant();
        std::list<ClientRequest *>::iterator it(m_currentRequest);
        ++it;
        if (it == m_pendingRequests.end()) {
            // Nope, still the end, we really do have to flush
            lock.unlock();
        } else {
            flush = false;
        }
        if (flush) {
            flush = false;
            try {
                MORDOR_LOG_TRACE(g_log) << this << " flushing";
                m_stream->flush();
            } catch (...) {
                request->requestFailed();
                throw;
            }
            lock.lock();
            invariant();
        }
        request->m_requestState = ClientRequest::COMPLETE;
        ++m_currentRequest;
        if (request->m_responseState >= ClientRequest::COMPLETE) {
            MORDOR_ASSERT(request == m_pendingRequests.front());
            m_pendingRequests.pop_front();
            if (m_priorResponseClosed <= request->m_requestNumber ||
                m_priorResponseFailed <= request->m_requestNumber) {
                MORDOR_ASSERT(m_pendingRequests.empty());
                close = true;
                lock.unlock();
                MORDOR_LOG_TRACE(g_log) << this << " closing";
            }
        }
        // Someone else may have queued up while we were flushing
        if (!flush && m_currentRequest != m_pendingRequests.end()) {
            request = *m_currentRequest;
            request->m_requestState = ClientRequest::HEADERS;
            MORDOR_ASSERT(request->m_scheduler);
            MORDOR_ASSERT(request->m_fiber);
            MORDOR_LOG_TRACE(g_log) << this << "-" << request->m_requestNumber << " scheduling request";
            request->m_scheduler->schedule(request->m_fiber);
            request->m_scheduler = NULL;
            request->m_fiber.reset();
        } else {
            if (m_timeoutStream)
                m_timeoutStream->readTimeout(m_readTimeout);
        }
    }
    if (close)
        m_stream->close();
}

void
ClientConnection::scheduleNextResponse(ClientRequest *request)
{
    bool close = false;
    {
        boost::mutex::scoped_lock lock(m_mutex);
        invariant();
        MORDOR_ASSERT(!m_pendingRequests.empty());
        MORDOR_ASSERT(request == m_pendingRequests.front());
        MORDOR_ASSERT(request->m_responseState == ClientRequest::BODY ||
            request->m_responseState == ClientRequest::HEADERS);
        request->m_responseState = ClientRequest::COMPLETE;
        MORDOR_LOG_TRACE(g_log) << this << "-" << request->m_requestNumber << " response complete";
        std::list<ClientRequest *>::iterator it = m_pendingRequests.begin();
        ++it;
        if (request->m_requestState >= ClientRequest::COMPLETE) {
            m_pendingRequests.pop_front();
            if (m_priorResponseClosed <= request->m_requestNumber ||
                m_priorResponseFailed <= request->m_requestNumber)
                close = true;
        }
        if (it != m_pendingRequests.end()) {
            request = *it;
            MORDOR_ASSERT(request);
            MORDOR_ASSERT(request->m_responseState <= ClientRequest::WAITING ||
                request->m_responseState == ClientRequest::CANCELED);
            if (request->m_responseState == ClientRequest::WAITING) {
                std::set<ClientRequest *>::iterator it2 = m_waitingResponses.find(request);
                MORDOR_ASSERT(it2 != m_waitingResponses.end());
                m_waitingResponses.erase(it2);
                request->m_responseState = ClientRequest::HEADERS;
                MORDOR_ASSERT(request->m_scheduler);
                MORDOR_ASSERT(request->m_fiber);
                MORDOR_LOG_TRACE(g_log) << this << "-" << request->m_requestNumber << " scheduling response";
                request->m_scheduler->schedule(request->m_fiber);
                request->m_scheduler = NULL;
                request->m_fiber.reset();
                request = NULL;
            } else if (request->m_responseState == ClientRequest::PENDING) {
                request = NULL;
            }
        } else {
            if (m_idleTimeout != ~0ull) {
                MORDOR_ASSERT(!m_idleTimer);
                MORDOR_ASSERT(m_timerManager);
                MORDOR_ASSERT(m_idleDg);
                m_idleTimer = m_timerManager->registerTimer(m_idleTimeout, m_idleDg);
            }
            request = NULL;
        }
    }
    if (request) {
        MORDOR_ASSERT(request->m_responseState == ClientRequest::CANCELED);
        MORDOR_LOG_TRACE(g_log) << this << "-" << request->m_requestNumber << " skipping response";
        request->finish();
    }
    if (close) {
        MORDOR_ASSERT(!request);
        MORDOR_LOG_TRACE(g_log) << this << " closing";
        m_stream->close();
    }
}

void
ClientConnection::scheduleAllWaitingRequests()
{
    MORDOR_ASSERT(m_priorRequestFailed || m_priorResponseFailed != ~0ull ||
        m_priorResponseClosed != ~0ull);
    // MORDOR_ASSERT(m_mutex.locked());
    MORDOR_LOG_TRACE(g_log) << this << " scheduling all requests";

    for (std::list<ClientRequest *>::iterator it(m_currentRequest);
        it != m_pendingRequests.end();
        ) {
        ClientRequest *request = *it;
        MORDOR_ASSERT(request->m_requestState != ClientRequest::COMPLETE);
        if (request->m_requestState == ClientRequest::WAITING) {
            MORDOR_ASSERT(request->m_scheduler);
            MORDOR_ASSERT(request->m_fiber);
            MORDOR_LOG_TRACE(g_log) << this << "-" << request->m_requestNumber << " scheduling request";
            request->m_scheduler->schedule(request->m_fiber);
            request->m_scheduler = NULL;
            request->m_fiber.reset();
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
    MORDOR_ASSERT(m_priorResponseFailed != ~0ull || m_priorResponseClosed != ~0ull);
    // MORDOR_ASSERT(m_mutex.locked());
    MORDOR_LOG_TRACE(g_log) << this << " scheduling all responses";
    unsigned long long firstResponseToSchedule =
        std::min(m_priorResponseFailed, m_priorResponseClosed);

    std::list<ClientRequest *>::iterator end = m_currentRequest;
    if (end != m_pendingRequests.end())
        ++end;
    for (std::list<ClientRequest *>::iterator it(m_pendingRequests.begin());
        it != end;) {
        ClientRequest *request = *it;
        if (request->m_requestNumber > firstResponseToSchedule) {
            switch (request->m_responseState) {
                case ClientRequest::PENDING:
                case ClientRequest::ERROR:
                    ++it;
                    continue;
                case ClientRequest::WAITING:
                {
                    std::set<ClientRequest *>::iterator waiting =
                        m_waitingResponses.find(request);
                    MORDOR_ASSERT(waiting != m_waitingResponses.end());
                    MORDOR_ASSERT(request->m_scheduler);
                    MORDOR_ASSERT(request->m_fiber);
                    MORDOR_ASSERT(request->m_responseState ==
                        ClientRequest::WAITING);
                    MORDOR_LOG_TRACE(g_log) << this << "-"
                        << request->m_requestNumber << " scheduling response";
                    request->m_responseState = ClientRequest::ERROR;
                    request->m_scheduler->schedule(request->m_fiber);
                    request->m_scheduler = NULL;
                    request->m_fiber.reset();
                    if (request->m_requestState >= ClientRequest::COMPLETE)
                        it = m_pendingRequests.erase(it);
                    m_waitingResponses.erase(waiting);
                    continue;
                }
                default:
                    MORDOR_NOTREACHED();
            }
        }
        ++it;
    }
}

void
ClientConnection::invariant() const
{
#ifdef DEBUG
    // MORDOR_ASSERT(m_mutex.locked());
    bool seenFirstUnrequested = false;
    unsigned long long lastRequestNumber = 0;
    for (std::list<ClientRequest *>::const_iterator it(m_pendingRequests.begin());
        it != m_pendingRequests.end();
        ++it) {
        ClientRequest *request = *it;
        MORDOR_ASSERT(request->m_requestNumber != 0);
        if (lastRequestNumber == 0) {
            lastRequestNumber = request->m_requestNumber;
        } else {
            MORDOR_ASSERT(++lastRequestNumber == request->m_requestNumber);
        }
        MORDOR_ASSERT(request->m_requestState < ClientRequest::COMPLETE ||
            request->m_responseState < ClientRequest::COMPLETE ||
            request->m_responseState == ClientRequest::CANCELED);
        // NOTE: it is allowed to have a response complete before the request
        // completes, BUT you can't have a response start before the request
        // starts
        if (request->m_responseState > ClientRequest::WAITING)
            MORDOR_ASSERT(request->m_requestState > ClientRequest::WAITING);
        if (!seenFirstUnrequested) {
            if (request->m_requestState < ClientRequest::COMPLETE) {
                seenFirstUnrequested = true;
                MORDOR_ASSERT(request->m_requestState >
                    ClientRequest::WAITING);
                MORDOR_ASSERT(m_currentRequest == it);
            }
        } else {
            MORDOR_ASSERT(request->m_requestState == ClientRequest::WAITING);
        }
        if (it != m_pendingRequests.begin())
            MORDOR_ASSERT(request->m_responseState <= ClientRequest::WAITING);
    }
    if (!seenFirstUnrequested)
        MORDOR_ASSERT(m_currentRequest == m_pendingRequests.end());
    std::list<ClientRequest *>::const_iterator end = m_currentRequest;
    if (end != m_pendingRequests.end())
        ++end;
    for (std::set<ClientRequest *>::const_iterator it(m_waitingResponses.begin());
        it != m_waitingResponses.end();
        ++it) {
        ClientRequest *request = *it;
        MORDOR_ASSERT(request);
        MORDOR_ASSERT(request->m_responseState == ClientRequest::WAITING);

        MORDOR_ASSERT(std::find<std::list<ClientRequest *>::const_iterator>
            (m_pendingRequests.begin(), end, request) != end);
    }
#endif
}


ClientRequest::ClientRequest(ClientConnection::ptr conn, const Request &request)
: m_conn(conn),
  m_requestNumber(0),
  m_scheduler(NULL),
  m_request(request),
  m_requestState(WAITING),
  m_responseState(PENDING),
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

bool
ClientRequest::hasRequestBody() const
{
    return Connection::hasMessageBody(m_request.general,
        m_request.entity, m_request.requestLine.method, INVALID, false);
}

Stream::ptr
ClientRequest::requestStream()
{
    if (m_requestStream)
        return m_requestStream;
    MORDOR_ASSERT(!m_requestMultipart);
    MORDOR_ASSERT(m_request.entity.contentType.type != "multipart");
    if (!hasRequestBody()) {
        m_requestStream = Stream::ptr(&NullStream::get(), &nop<Stream *>);
        m_requestStream.reset(new LimitedStream(m_requestStream, 0));
        return m_requestStream;
    }
    MORDOR_ASSERT(m_requestState == BODY);
    return m_requestStream = m_conn->getStream(m_request.general, m_request.entity,
        m_request.requestLine.method, INVALID,
        boost::bind(&ClientRequest::requestDone, this),
        boost::bind(&ClientRequest::requestFailed, this), false);
}

Multipart::ptr
ClientRequest::requestMultipart()
{
    if (m_requestMultipart)
        return m_requestMultipart;
    MORDOR_ASSERT(m_request.entity.contentType.type == "multipart");
    MORDOR_ASSERT(!m_requestStream);
    MORDOR_ASSERT(m_requestState == BODY);
    StringMap::const_iterator it = m_request.entity.contentType.parameters.find("boundary");
    if (it == m_request.entity.contentType.parameters.end()) {
        MORDOR_THROW_EXCEPTION(MissingMultipartBoundaryException());
    }
    m_requestStream = m_conn->getStream(m_request.general, m_request.entity,
        m_request.requestLine.method, INVALID,
        boost::bind(&ClientRequest::requestDone, this),
        boost::bind(&ClientRequest::requestFailed, this), false);
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
    if (result || m_hasResponseBody) {
        MORDOR_ASSERT(result);
        return result;
    }
    ensureResponse();
    if (m_responseState >= COMPLETE) {
        m_hasResponseBody = true;
        result.reset(&NullStream::get(), &nop<Stream *>);
        m_responseStream = result;
        return result;
    }
    MORDOR_ASSERT(m_responseState == BODY);
    MORDOR_ASSERT(m_response.entity.contentType.type != "multipart");
    result = m_conn->getStream(m_response.general, m_response.entity,
        m_request.requestLine.method, m_response.status.status,
        boost::bind(&ClientRequest::responseDone, shared_from_this()),
        boost::bind(&ClientRequest::cancel, shared_from_this(), true, true), true);
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
    MORDOR_ASSERT(m_responseState >= COMPLETE);
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
    MORDOR_ASSERT(m_responseState == BODY);
    MORDOR_ASSERT(m_response.entity.contentType.type == "multipart");
    StringMap::const_iterator it = m_response.entity.contentType.parameters.find("boundary");
    if (it == m_response.entity.contentType.parameters.end()) {
        MORDOR_THROW_EXCEPTION(MissingMultipartBoundaryException());
    }
    Stream::ptr stream = m_conn->getStream(m_response.general, m_response.entity,
        m_request.requestLine.method, m_response.status.status,
        NULL,
        boost::bind(&ClientRequest::cancel, shared_from_this(), true, true), true);
    m_responseStream = stream;
    result.reset(new Multipart(stream, it->second));
    result->multipartFinished = boost::bind(&ClientRequest::responseDone, shared_from_this());
    m_responseMultipart = result;
    return result;
}

void
ClientRequest::cancel(bool abort, bool error)
{
    if (m_requestState >= COMPLETE && m_responseState >= COMPLETE)
        return;
    MORDOR_LOG_TRACE(g_log) << m_conn << "-" << m_requestNumber
        << (abort ? " aborting" : " cancelling");
    if (!abort && m_requestState == WAITING && m_responseState <= WAITING) {
        // Just abandon it
        m_requestState = CANCELED;
        m_responseState = CANCELED;
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        std::list<ClientRequest *>::iterator it =
            std::find(m_conn->m_pendingRequests.begin(),
            m_conn->m_pendingRequests.end(), this);
        MORDOR_ASSERT(it != m_conn->m_pendingRequests.end());
        m_conn->m_pendingRequests.erase(it);
        if (m_responseState == WAITING) {
            std::set<ClientRequest *>::iterator waitIt =
                m_conn->m_waitingResponses.find(this);
            MORDOR_ASSERT(waitIt != m_conn->m_waitingResponses.end());
            m_conn->m_waitingResponses.erase(waitIt);
            MORDOR_LOG_TRACE(g_log) << m_conn << "-" << m_requestNumber
                << " scheduling response";
            m_scheduler->schedule(m_fiber);
            m_scheduler = NULL;
            m_fiber.reset();
        }
        return;
    }
    if (m_requestStream) {
        FilterStream *filter = static_cast<FilterStream *>(m_requestStream.get());
        if (filter->parent().get() != &NullStream::get()) {
            // Break the circular reference
            NotifyStream::ptr notify =
                boost::dynamic_pointer_cast<NotifyStream>(m_requestStream);
            MORDOR_ASSERT(notify);
            notify->notifyOnClose = NULL;
            notify->notifyOnEof = NULL;
            notify->notifyOnException = NULL;
            notify->parent(Stream::ptr(new Stream()));
        }
    }
    Stream::ptr responseStream = m_responseStream.lock();
    ClientRequest::ptr self;
    if (responseStream) {
        // notify may be holding the last reference to this, so keep ourself in scope
        self = shared_from_this();
        NotifyStream::ptr notify =
            boost::dynamic_pointer_cast<NotifyStream>(responseStream);
        MORDOR_ASSERT(notify);
        notify->notifyOnClose = NULL;
        notify->notifyOnEof = NULL;
        notify->notifyOnException = NULL;
    }
    bool close = false, waiting = m_responseState == WAITING;
    {
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        m_conn->m_priorResponseFailed = m_requestNumber;
        if (m_requestState < COMPLETE)
            m_requestState = error ? ERROR : CANCELED;
        if (m_responseState < COMPLETE)
            m_responseState = error ? ERROR : CANCELED;

        std::list<ClientRequest *>::iterator it =
            std::find(m_conn->m_pendingRequests.begin(),
            m_conn->m_pendingRequests.end(), this);
        MORDOR_ASSERT(it != m_conn->m_pendingRequests.end());
        close = it == m_conn->m_pendingRequests.begin();
        if (it == m_conn->m_currentRequest)
            m_conn->m_currentRequest = m_conn->m_pendingRequests.erase(it);
        else
            m_conn->m_pendingRequests.erase(it);
        if (waiting) {
            std::set<ClientRequest *>::iterator waitIt =
                m_conn->m_waitingResponses.find(this);
            MORDOR_ASSERT(waitIt != m_conn->m_waitingResponses.end());
            m_conn->m_waitingResponses.erase(waitIt);
            MORDOR_LOG_TRACE(g_log) << m_conn << "-" << m_requestNumber
                << " scheduling response";
            m_scheduler->schedule(m_fiber);
            m_scheduler = NULL;
            m_fiber.reset();
        }
        m_conn->scheduleAllWaitingRequests();
        m_conn->scheduleAllWaitingResponses();
    }
    if (close)
        m_conn->m_stream->cancelRead();
    m_conn->m_stream->cancelWrite();
}

void
ClientRequest::finish()
{
    if (m_requestState != COMPLETE) {
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
        if (!m_conn->m_allowNewRequests || m_conn->m_priorResponseClosed != ~0ull) {
            m_requestState = m_responseState = ERROR;
            MORDOR_THROW_EXCEPTION(ConnectionVoluntarilyClosedException());
        }
        if (m_conn->m_priorRequestFailed || m_conn->m_priorResponseFailed != ~0ull) {
            m_requestState = m_responseState = ERROR;
            MORDOR_THROW_EXCEPTION(PriorRequestFailedException());
        }
        if (m_conn->m_idleTimer) {
            m_conn->m_idleTimer->cancel();
            m_conn->m_idleTimer.reset();
        }
        firstRequest = m_conn->m_currentRequest == m_conn->m_pendingRequests.end();
        m_requestNumber = ++m_conn->m_requestCount;
        m_conn->m_pendingRequests.push_back(this);
        if (firstRequest) {
            m_conn->m_currentRequest = m_conn->m_pendingRequests.end();
            --m_conn->m_currentRequest;
            m_requestState = HEADERS;
            // Disable read timeouts while a request is in progress
            if (m_conn->m_timeoutStream)
                m_conn->m_timeoutStream->readTimeout(~0ull);
            MORDOR_LOG_TRACE(g_log) << m_conn << "-" << m_requestNumber << " requesting";
        } else {
            m_scheduler = Scheduler::getThis();
            m_fiber = Fiber::getThis();
            MORDOR_ASSERT(m_scheduler);
            MORDOR_ASSERT(m_fiber);
            MORDOR_LOG_TRACE(g_log) << m_conn << "-" << m_requestNumber << " waiting to request";
        }
        if (close)
            m_conn->m_allowNewRequests = false;
    }
    // If we weren't the first request in the queue, we have to wait for
    // another request to schedule us
    if (!firstRequest) {
        Scheduler::yieldTo();
        MORDOR_LOG_TRACE(g_log) << m_conn << "-" << m_requestNumber << " requesting";
        // Check for problems that occurred while we were waiting
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        if (m_conn->m_priorResponseClosed != ~0ull ||
            m_conn->m_priorRequestFailed ||
            m_conn->m_priorResponseFailed != ~0ull) {
            if (m_requestState == HEADERS) {
                MORDOR_ASSERT(m_conn->m_currentRequest !=
                    m_conn->m_pendingRequests.end());
                MORDOR_ASSERT(*m_conn->m_currentRequest == this);
                m_conn->m_currentRequest =
                    m_conn->m_pendingRequests.erase(m_conn->m_currentRequest);
                MORDOR_ASSERT(m_conn->m_currentRequest ==
                    m_conn->m_pendingRequests.end());
            }
            m_requestState = m_responseState = ERROR;
            if (m_conn->m_priorResponseClosed != ~0ull)
                MORDOR_THROW_EXCEPTION(ConnectionVoluntarilyClosedException());
            else
                MORDOR_THROW_EXCEPTION(PriorRequestFailedException());
        }
    }
    MORDOR_ASSERT(m_requestState == HEADERS);

    try {
        // Do the request
        std::ostringstream os;
        os << m_request;
        std::string str = os.str();
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
            MORDOR_LOG_DEBUG(g_log) << m_conn << "-" << m_requestNumber<< " " << m_request;
            if (!webAuth.empty())
                m_request.request.authorization.base64 = webAuth;
            if (!proxyAuth.empty())
                m_request.request.proxyAuthorization.base64 = proxyAuth;
        } else {
            MORDOR_LOG_VERBOSE(g_log) << m_conn << "-" << m_requestNumber << " " << m_request.requestLine;
        }
        m_conn->m_stream->write(str.c_str(), str.size());
        m_requestState = BODY;

        if (!Connection::hasMessageBody(m_request.general, m_request.entity, requestLine.method, INVALID, false)) {
            MORDOR_LOG_TRACE(g_log) << m_conn << "-" << m_requestNumber << " no request body";
            m_conn->scheduleNextRequest(this);
        }
    } catch(...) {
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        m_requestState = ERROR;
        m_responseState = CANCELED;
        MORDOR_ASSERT(!m_conn->m_pendingRequests.empty());
        MORDOR_ASSERT(m_conn->m_currentRequest != m_conn->m_pendingRequests.end());
        m_conn->m_priorRequestFailed = true;
        m_conn->m_currentRequest = m_conn->m_pendingRequests.erase(m_conn->m_currentRequest);
        m_conn->scheduleAllWaitingRequests();
        // Throw an HTTP exception if we can
        if (m_conn->m_priorResponseClosed <= m_requestNumber)
            MORDOR_THROW_EXCEPTION(ConnectionVoluntarilyClosedException());
        if (m_conn->m_priorResponseFailed <= m_requestNumber)
            MORDOR_THROW_EXCEPTION(PriorRequestFailedException());
        throw;
    }
}

void
ClientRequest::ensureResponse()
{
    if (m_priorResponseException)
        ::Mordor::rethrow_exception(m_priorResponseException);
    if (m_responseState == BODY || m_responseState >= COMPLETE)
        return;
    try {
        bool wait = false;
        MORDOR_ASSERT(m_responseState == PENDING);
        {
            boost::mutex::scoped_lock lock(m_conn->m_mutex);
            m_conn->invariant();
            if (m_conn->m_priorResponseFailed <= m_requestNumber ||
                m_conn->m_priorResponseClosed <= m_requestNumber) {
                std::list<ClientRequest *>::iterator it;
                it = std::find(m_conn->m_pendingRequests.begin(),
                    m_conn->m_pendingRequests.end(), this);
                MORDOR_ASSERT(it != m_conn->m_pendingRequests.end());
                m_conn->m_pendingRequests.erase(it);
                m_responseState = ERROR;
                if (m_conn->m_priorResponseClosed <= m_requestNumber)
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
                m_responseState = WAITING;
                MORDOR_LOG_TRACE(g_log) << m_conn << "-" << m_requestNumber<< " waiting for response";
            } else {
                m_responseState = HEADERS;
                MORDOR_LOG_TRACE(g_log) << m_conn << "-" << m_requestNumber << " reading response";
            }
        }

        // If we weren't the first response in the queue, wait for someone
        // else to schedule us
        if (wait) {
            Scheduler::yieldTo();
            MORDOR_LOG_TRACE(g_log) << m_conn << "-" << m_requestNumber << " reading response";
            // Check for problems that occurred while we were waiting
            boost::mutex::scoped_lock lock(m_conn->m_mutex);
            m_conn->invariant();
            if (m_responseState == CANCELED)
                MORDOR_THROW_EXCEPTION(OperationAbortedException());
            if (m_responseState == ERROR) {
                if (m_conn->m_priorResponseClosed <= m_requestNumber)
                    MORDOR_THROW_EXCEPTION(ConnectionVoluntarilyClosedException());
                else
                    MORDOR_THROW_EXCEPTION(PriorRequestFailedException());
            }
            // Probably means that the Scheduler exited in the above yieldTo,
            // and returned to us, because there is no other work to be done
            try {
                MORDOR_ASSERT(!m_conn->m_pendingRequests.empty());
                MORDOR_ASSERT(m_conn->m_pendingRequests.front() == this);
            } catch(...) {
                m_responseState = PENDING;
                std::set<ClientRequest *>::iterator it = m_conn->m_waitingResponses.find(this);
                if (it != m_conn->m_waitingResponses.end())
                    m_conn->m_waitingResponses.erase(it);
                throw;
            }
        }

        try {
            MORDOR_ASSERT(m_responseState == HEADERS);
            // Read and parse headers
            ResponseParser parser(m_response);
            unsigned long long read = parser.run(m_conn->m_stream);
            if (read == 0ull)
                MORDOR_THROW_EXCEPTION(UnexpectedEofException());
            if (parser.error())
                MORDOR_THROW_EXCEPTION(BadMessageHeaderException());
            if (!parser.complete())
                MORDOR_THROW_EXCEPTION(IncompleteMessageHeaderException());
            if (g_log->enabled(Log::DEBUG)) {
                MORDOR_LOG_DEBUG(g_log) << m_conn << "-" << m_requestNumber << " " << m_response;
            } else {
                MORDOR_LOG_VERBOSE(g_log) << m_conn << "-" << m_requestNumber << " " << m_response.status;
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
                m_request.requestLine.method, m_response.status.status, false);
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
                m_conn->m_priorResponseClosed = m_requestNumber;
                MORDOR_ASSERT(!m_conn->m_pendingRequests.empty());
                MORDOR_ASSERT(m_conn->m_pendingRequests.front() == this);
                if (!hasBody && m_requestState >= COMPLETE)
                    m_conn->m_pendingRequests.pop_front();
                m_responseState = hasBody ? BODY : COMPLETE;
                m_conn->scheduleAllWaitingRequests();
                m_conn->scheduleAllWaitingResponses();
            } else {
                m_responseState = connect ? COMPLETE : BODY;
            }

            if (!hasBody && !connect) {
                MORDOR_LOG_TRACE(g_log) << m_conn << "-" << m_requestNumber << " no response body";
                if (close) {
                    if (m_conn->m_stream->supportsHalfClose())
                        m_conn->m_stream->close(Stream::READ);
                } else {
                    m_conn->scheduleNextResponse(this);
                }
            }
        } catch (...) {
            boost::mutex::scoped_lock lock(m_conn->m_mutex);
            m_conn->invariant();
            bool firstFailure = m_conn->m_priorResponseFailed == ~0ull;
            if (firstFailure)
                m_conn->m_priorResponseFailed = m_requestNumber;
            m_responseState = ERROR;
            if (!m_conn->m_pendingRequests.empty() &&
                m_conn->m_pendingRequests.front() == this) {
                if (m_requestState >= COMPLETE)
                    m_conn->m_pendingRequests.pop_front();
                m_conn->scheduleAllWaitingRequests();
                m_conn->scheduleAllWaitingResponses();
            }
            if (m_conn->m_priorResponseClosed < m_requestNumber)
                MORDOR_THROW_EXCEPTION(ConnectionVoluntarilyClosedException());
            if (!firstFailure && m_conn->m_priorResponseFailed < m_requestNumber)
                MORDOR_THROW_EXCEPTION(PriorRequestFailedException());
            throw;
        }
    } catch (...) {
        m_priorResponseException = boost::current_exception();
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
    MORDOR_ASSERT(m_requestState == BODY);
    MORDOR_ASSERT(m_requestStream);
    MORDOR_LOG_TRACE(g_log) << m_conn << "-" << m_requestNumber << " request complete";
    // Break the circular reference
    NotifyStream::ptr notify =
        boost::dynamic_pointer_cast<NotifyStream>(m_requestStream);
    MORDOR_ASSERT(notify);
    notify->notifyOnClose = NULL;
    notify->notifyOnEof = NULL;
    notify->notifyOnException = NULL;
    if (m_requestStream->supportsSize() && m_requestStream->supportsTell())
        MORDOR_ASSERT(m_requestStream->size() == m_requestStream->tell());
    if (!m_request.general.transferEncoding.empty()) {
        std::ostringstream os;
        os << m_requestTrailer << "\r\n";
        std::string str = os.str();
        MORDOR_LOG_DEBUG(g_log) << m_conn << "-" << m_requestNumber << " " << str;
        m_conn->m_stream->write(str.c_str(), str.size());
    }
    m_conn->scheduleNextRequest(this);
}

void
ClientRequest::requestFailed()
{
    MORDOR_ASSERT(m_requestState == BODY);
    MORDOR_LOG_TRACE(g_log) << m_conn << "-" << m_requestNumber << " request failed";
    if (m_requestStream) {
        // Break the circular reference
        NotifyStream::ptr notify =
            boost::dynamic_pointer_cast<NotifyStream>(m_requestStream);
        MORDOR_ASSERT(notify);
        notify->notifyOnClose = NULL;
        notify->notifyOnEof = NULL;
        notify->notifyOnException = NULL;
    }
    boost::mutex::scoped_lock lock(m_conn->m_mutex);
    m_conn->invariant();
    MORDOR_ASSERT(!m_conn->m_pendingRequests.empty());
    MORDOR_ASSERT(this == *m_conn->m_currentRequest);
    m_conn->m_priorRequestFailed = true;
    m_requestState = ERROR;
    if (m_responseState >= COMPLETE) {
        MORDOR_ASSERT(this == m_conn->m_pendingRequests.front());
        m_conn->m_pendingRequests.pop_front();
        m_conn->m_currentRequest = m_conn->m_pendingRequests.end();
    } else {
        ++m_conn->m_currentRequest;
    }
    m_conn->scheduleAllWaitingRequests();
    // Throw an HTTP exception if we can
    if (m_conn->m_priorResponseClosed <= m_requestNumber)
        MORDOR_THROW_EXCEPTION(ConnectionVoluntarilyClosedException());
    if (m_conn->m_priorResponseFailed <= m_requestNumber)
        MORDOR_THROW_EXCEPTION(PriorRequestFailedException());
}

void
ClientRequest::responseDone()
{
    MORDOR_ASSERT(m_responseState == BODY);
    MORDOR_LOG_TRACE(g_log) << m_conn << "-" << m_requestNumber << " response complete";
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
    // Make sure every stream in the stack gets a proper EOF
    FilterStream::ptr filter =
        boost::dynamic_pointer_cast<FilterStream>(notify->parent());
    Stream::ptr chunked, limited;
    while (filter && !chunked && !limited) {
        chunked = boost::dynamic_pointer_cast<ChunkedStream>(filter);
        limited = boost::dynamic_pointer_cast<LimitedStream>(filter);
        transferStream(filter, NullStream::get());
        filter = boost::dynamic_pointer_cast<FilterStream>(filter->parent());
    }
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
        MORDOR_LOG_DEBUG(g_log) << m_conn << "-" << m_requestNumber << " " << m_responseTrailer;
    }
    m_conn->scheduleNextResponse(this);
}

void
ClientRequest::responseFailed()
{
    cancel(true, true);
}

}}
