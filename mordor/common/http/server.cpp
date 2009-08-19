// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "server.h"

#include <boost/bind.hpp>

#include "mordor/common/exception.h"
#include "mordor/common/log.h"
#include "mordor/common/streams/null.h"
#include "mordor/common/streams/transfer.h"
#include "parser.h"

static Logger::ptr g_log = Log::lookup("mordor.common.http.server");

HTTP::ServerConnection::ServerConnection(Stream::ptr stream, boost::function<void (ServerRequest::ptr)> dg)
: Connection(stream),
  m_dg(dg),
  m_exception("")
{
    ASSERT(m_dg);
}

void
HTTP::ServerConnection::processRequests()
{
    scheduleNextRequest(ServerRequest::ptr((HTTP::ServerRequest *)NULL));
}

void
HTTP::ServerConnection::scheduleNextRequest(ServerRequest::ptr request)
{
    bool close = false;
    {
        boost::mutex::scoped_lock lock(m_mutex);
        invariant();
        if (request.get()) {
            ASSERT(!m_pendingRequests.empty());
            ASSERT(request == m_pendingRequests.back());
            ASSERT(!request->m_requestDone);
            request->m_requestDone = true;
            if (request->m_responseDone) {
                m_pendingRequests.pop_back();
            }
            close = request->m_willClose;
            request.reset();
        }
        if (!close) {
            request.reset(new ServerRequest(shared_from_this()));
            m_pendingRequests.push_back(request);
        }
    }
    if (!close) {
        Fiber::ptr requestFiber(new Fiber(boost::bind(&ServerRequest::doRequest, request), 4096 * 2));
        Scheduler::getThis()->schedule(requestFiber);
    } else {
        m_stream->close(Stream::READ);
    }
}

void
HTTP::ServerConnection::scheduleNextResponse(ServerRequest::ptr request)
{
    {
        boost::mutex::scoped_lock lock(m_mutex);
        invariant();
        ASSERT(!request->m_responseDone);
        ASSERT(request->m_responseInFlight);
        std::list<ServerRequest::ptr>::iterator it =
            std::find(m_pendingRequests.begin(), m_pendingRequests.end(), request);
        ASSERT(it != m_pendingRequests.end());
        std::list<ServerRequest::ptr>::iterator next(it);
        ++next;
        if (next != m_pendingRequests.end()) {
            std::set<ServerRequest::ptr>::iterator waitIt(m_waitingResponses.find(*next));
            if (waitIt != m_waitingResponses.end()) {
                request->m_responseInFlight = false;
                request->m_responseDone = true;
                if (request->m_requestDone)
                    m_pendingRequests.erase(it);
                m_waitingResponses.erase(waitIt);
                request = *next;
                request->m_responseInFlight = true;
                request->m_scheduler->schedule(request->m_fiber);
                return;
            }
        }
    }
    m_stream->flush();
    boost::mutex::scoped_lock lock(m_mutex);
    invariant();
    ASSERT(request == m_pendingRequests.front());
    request->m_responseInFlight = false;
    request->m_responseDone = true;
    if (request->m_willClose) {
        m_stream->close();
    }
    std::list<ServerRequest::ptr>::iterator it =
            std::find(m_pendingRequests.begin(), m_pendingRequests.end(), request);
    ASSERT(it != m_pendingRequests.end());
    if (request->m_requestDone) {
        it = m_pendingRequests.erase(it);
    } else {
        ++it;
    }
    // Someone else may have queued up while we were flushing
    if (it != m_pendingRequests.end()) {
        std::set<ServerRequest::ptr>::iterator waitIt(m_waitingResponses.find(*it));
        if (waitIt != m_waitingResponses.end()) {
            request = *waitIt;
            m_waitingResponses.erase(waitIt);
            request->m_responseInFlight = true;
            request->m_scheduler->schedule(request->m_fiber);
            return;
        }
    }
}

void
HTTP::ServerConnection::scheduleAllWaitingResponses()
{
    ASSERT(*m_exception.what());
    // ASSERT(m_mutex.locked());
    for (std::list<ServerRequest::ptr>::iterator it(m_pendingRequests.begin());
        it != m_pendingRequests.end();
        ++it) {
        std::set<ServerRequest::ptr>::iterator waiting = m_waitingResponses.find(*it);
        if (waiting != m_waitingResponses.end()) {
            (*it)->m_scheduler->schedule((*it)->m_fiber);            
            it = m_pendingRequests.erase(it);
            --it;
            m_waitingResponses.erase(waiting);
        }
    }
}

void
HTTP::ServerConnection::invariant() const
{
    // ASSERT(m_mutex.locked());
    bool seenResponseNotDone = false;
    for (std::list<ServerRequest::ptr>::const_iterator it(m_pendingRequests.begin());
        it != m_pendingRequests.end();
        ++it) {
        ServerRequest::ptr request = *it;
        ASSERT(!(request->m_requestDone && request->m_responseDone));
        if (seenResponseNotDone) {
            ASSERT(!request->m_responseDone);
        } else {
            seenResponseNotDone = !request->m_responseDone;
        }
        if (!request->m_requestDone) {
            ++it;
            ASSERT(it == m_pendingRequests.end());
            break;
        }
    }
    for (std::set<ServerRequest::ptr>::const_iterator it(m_waitingResponses.begin());
        it != m_waitingResponses.end();
        ++it) {
        ServerRequest::ptr request = *it;
        ASSERT(!request->m_responseDone);
        ASSERT(!request->m_responseInFlight);
    }
}


HTTP::ServerRequest::ServerRequest(ServerConnection::ptr conn)
: m_conn(conn),
  m_scheduler(NULL),
  m_requestDone(false),
  m_committed(false),
  m_responseDone(false),
  m_responseInFlight(false),
  m_aborted(false),
  m_willClose(false)
{}

const HTTP::Request &
HTTP::ServerRequest::request()
{
    return m_request;
}

bool
HTTP::ServerRequest::hasRequestBody()
{
    if (m_requestStream)
        return true;
    return Connection::hasMessageBody(m_request.general,
        m_request.entity,
        m_request.requestLine.method,
        INVALID);
}

Stream::ptr
HTTP::ServerRequest::requestStream()
{
    if (m_requestStream)
        return m_requestStream;
    ASSERT(!m_requestMultipart);
    ASSERT(m_request.entity.contentType.type != "multipart");
    return m_requestStream = m_conn->getStream(m_request.general, m_request.entity,
        m_request.requestLine.method, INVALID,
        boost::bind(&ServerRequest::requestDone, this),
        boost::bind(&ServerRequest::cancel, this), true);
}

Multipart::ptr
HTTP::ServerRequest::requestMultipart()
{
    if (m_requestMultipart)
        return m_requestMultipart;
    ASSERT(m_request.entity.contentType.type == "multipart");
    ASSERT(!m_requestStream);
    HTTP::StringMap::const_iterator it = m_request.entity.contentType.parameters.find("boundary");
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

const HTTP::EntityHeaders &
HTTP::ServerRequest::requestTrailer() const
{
    // If transferEncoding is not empty, it must include chunked,
    // and it must include chunked in order to have a trailer
    ASSERT(!m_request.general.transferEncoding.empty());
    ASSERT(m_requestDone);
    return m_requestTrailer;
}

HTTP::Response &
HTTP::ServerRequest::response()
{
    return m_response;
}

Stream::ptr
HTTP::ServerRequest::responseStream()
{
    if (m_responseStream)
        return m_responseStream;
    ASSERT(!m_responseMultipart);
    ASSERT(m_response.entity.contentType.type != "multipart");
    commit();    
    return m_responseStream = m_conn->getStream(m_response.general, m_response.entity,
        m_request.requestLine.method, m_response.status.status,
        boost::bind(&ServerRequest::responseDone, this),
        boost::bind(&ServerRequest::cancel, this), false);
}

Multipart::ptr
HTTP::ServerRequest::responseMultipart()
{
    if (m_responseMultipart)
        return m_responseMultipart;
    ASSERT(m_response.entity.contentType.type == "multipart");
    ASSERT(!m_responseStream);
    HTTP::StringMap::const_iterator it = m_response.entity.contentType.parameters.find("boundary");
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

HTTP::EntityHeaders &
HTTP::ServerRequest::responseTrailer()
{
    ASSERT(!m_response.general.transferEncoding.empty());
    return m_responseTrailer;
}

void
HTTP::ServerRequest::cancel()
{
    if (m_aborted)
        return;
    m_aborted = true;
    boost::mutex::scoped_lock lock(m_conn->m_mutex);
    m_conn->invariant();
    m_conn->m_exception =
        std::runtime_error("No more requests are possible because a prior request failed");
    m_conn->scheduleAllWaitingResponses();
    m_conn->m_stream->close(Stream::BOTH);
}

void
HTTP::ServerRequest::finish()
{
    if (m_responseDone) {
        return;
    }
    if (committed() && Connection::hasMessageBody(m_response.general, m_response.entity,
        m_request.requestLine.method, m_response.status.status)) {
        cancel();
        return;
    }
    commit();
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
            ASSERT(m_requestStream);
            transferStream(m_requestStream, NullStream::get());
        }
    }
}

void
HTTP::ServerRequest::doRequest()
{
    ASSERT(!m_requestDone);

    try {
        // Read and parse headers
        RequestParser parser(m_request);
        unsigned long long consumed = parser.run(m_conn->m_stream);
        if (consumed == 0 && !parser.error() && !parser.complete()) {
            // EOF; finish up as a dummy response
            m_willClose = true;
            m_responseInFlight = true;
            m_conn->scheduleNextResponse(shared_from_this());
            return;
        }
        if (parser.error() || !parser.complete()) {
            respondError(shared_from_this(), BAD_REQUEST, "Unable to parse request.", true);
            return;
        }
        LOG_TRACE(g_log) << m_request;

        if (m_request.requestLine.ver.major != 1) {
            respondError(shared_from_this(), HTTP_VERSION_NOT_SUPPORTED, "", true);
            return;
        }
        StringSet &connection = m_request.general.connection;
        if (m_request.requestLine.ver == Version(1, 0) &&
            connection.find("Keep-Alive") == connection.end()) {
            m_willClose = true;
        }
        if (connection.find("close") != connection.end()) {
            m_willClose = true;
        }

        // Host header required with HTTP/1.1
        if (m_request.requestLine.ver >= Version(1, 1) && m_request.request.host.empty()) {
            respondError(shared_from_this(), BAD_REQUEST, "Host header is required with HTTP/1.1", false);
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
                respondError(shared_from_this(), BAD_REQUEST, "The last transfer-coding is not chunked.", true);
            }
            if (!transferEncoding.back().parameters.empty()) {
                respondError(shared_from_this(), NOT_IMPLEMENTED, "Unknown parameter to chunked transfer-coding.", true);
            }
            for (ParameterizedList::const_iterator it(transferEncoding.begin());
                it + 1 != transferEncoding.end();
                ++it) {
                if (stricmp(it->value.c_str(), "chunked") == 0) {
                    respondError(shared_from_this(), BAD_REQUEST, "chunked transfer-coding applied multiple times.", true);
                    return;
                } else if (stricmp(it->value.c_str(), "deflate") == 0 ||
                    stricmp(it->value.c_str(), "gzip") == 0 ||
                    stricmp(it->value.c_str(), "x-gzip") == 0) {
                    // Supported transfer-codings
                } else if (stricmp(it->value.c_str(), "compress") == 0 ||
                    stricmp(it->value.c_str(), "x-compress") == 0) {
                    respondError(shared_from_this(), NOT_IMPLEMENTED, "compress transfer-coding is not supported", false);
                    return;
                } else {
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
                    respondError(shared_from_this(), EXPECTATION_FAILED, "Unrecognized parameters to 100-continue expectation", false);
                }
            } else {
                respondError(shared_from_this(), EXPECTATION_FAILED, "Unrecognized expectation: " + it->key, false);
            }
        }

        // TE is a connection-specific header
        if (!m_request.request.te.empty())
            m_request.general.connection.insert("TE");

        if (!Connection::hasMessageBody(m_request.general, m_request.entity,
            m_request.requestLine.method, INVALID)) {
            m_conn->scheduleNextRequest(shared_from_this());
        }
        m_conn->m_dg(shared_from_this());
    } catch (OperationAbortedException) {
        // Do nothing (this occurs when a pipelined request fails because a prior request closed the connection
    } catch (std::exception &ex) {
        if (!m_responseDone) {
            try {
                if (!committed()) {
                    size_t whatLen = strlen(ex.what());
                    m_response.status.status = INTERNAL_SERVER_ERROR;
                    m_response.general.connection.clear();
                    m_response.general.connection.insert("close");
                    m_response.general.transferEncoding.clear();
                    m_response.entity.contentLength = whatLen;
                    m_response.entity.contentType.type = "text";
                    m_response.entity.contentType.subtype = "plain";
                    m_response.entity.contentType.parameters.clear();
                    if (!m_responseStream) {
                        m_responseStream = responseStream();
                    }
                    ASSERT(m_responseStream);
                    m_responseStream->write(ex.what(), whatLen);
                    m_responseStream->close();
                } else {
                    finish();
                }
            } catch(...) {
                // Swallow any exceptions that happen while trying to report the error
            }
            boost::mutex::scoped_lock lock(m_conn->m_mutex);
            m_conn->invariant();
            m_conn->m_exception = std::runtime_error("No more requests are possible because a prior request failed");
            m_conn->scheduleAllWaitingResponses();
        }
    }
}


void
HTTP::ServerRequest::commit()
{
    if (m_committed)
        return;
    // TODO: need to queue up other people waiting for this response if m_responseInFlight
    m_committed = true;

    if (m_response.general.connection.find("close") != m_response.general.connection.end())
        m_willClose = true;

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
    if (m_response.status.status == UNAUTHORIZED) {
        ASSERT(!m_response.response.wwwAuthenticate.empty());
    } else if (m_response.status.status == PROXY_AUTHENTICATION_REQUIRED) {
        ASSERT(!m_response.response.proxyAuthenticate.empty());
    }

    bool wait = false;
    {
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        if (*m_conn->m_exception.what()) {
            std::list<ServerRequest::ptr>::iterator it;
            it = std::find(m_conn->m_pendingRequests.begin(), m_conn->m_pendingRequests.end(),
                shared_from_this());
            ASSERT(it != m_conn->m_pendingRequests.end());             
            m_conn->m_pendingRequests.erase(it);
            throw m_conn->m_exception;
        }
        ASSERT(!m_conn->m_pendingRequests.empty());
        ServerRequest::ptr request = m_conn->m_pendingRequests.front();
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
        m_scheduler = Scheduler::getThis();
        m_fiber = Fiber::getThis();
        Scheduler::getThis()->yieldTo();
        m_scheduler = NULL;
        m_fiber.reset();
        // Check for problems that occurred while we were waiting
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        if (*m_conn->m_exception.what()) {
            throw m_conn->m_exception;
        }
        m_responseInFlight = true;
    }

    if (m_response.status.ver == Version()) {
        m_response.status.ver = m_request.requestLine.ver;
        if (m_response.status.ver == Version())
            m_response.status.ver = Version(1, 1);
    }
    ASSERT(m_response.status.ver == Version(1, 0) ||
           m_response.status.ver == Version(1, 1));

    if (m_willClose) {
        m_response.general.connection.insert("close");
    } else if (m_response.status.ver == Version(1, 0)) {
        m_response.general.connection.insert("Keep-Alive");
    }

    if (m_response.status.reason.empty()) {
        m_response.status.reason = reason(m_response.status.status);
    }

    try {
        // Write the headers
        std::ostringstream os;
        os << m_response;
        std::string str = os.str();
        LOG_TRACE(g_log) << str;
        m_conn->m_stream->write(str.c_str(), str.size());

        if (!Connection::hasMessageBody(m_response.general, m_response.entity, m_request.requestLine.method, m_response.status.status)) {
            responseDone();
        }
    } catch(...) {
        boost::mutex::scoped_lock lock(m_conn->m_mutex);
        m_conn->invariant();
        m_conn->m_exception = std::runtime_error("No more requests are possible because a prior request failed");
        m_conn->scheduleAllWaitingResponses();        
        throw;
    }
}


void
HTTP::ServerRequest::requestDone()
{
    m_requestStream.reset();
    if (!m_request.general.transferEncoding.empty()) {
        // Read and parse the trailer
        TrailerParser parser(m_requestTrailer);
        parser.run(m_conn->m_stream);
        if (parser.error()) {
            cancel();
            throw std::runtime_error("Error parsing trailer");
        }
        ASSERT(parser.complete());
        LOG_TRACE(g_log) << m_requestTrailer;
    }
    m_conn->scheduleNextRequest(shared_from_this());
}

void
HTTP::ServerRequest::responseMultipartDone()
{
    m_responseStream->close();
}

void
HTTP::ServerRequest::responseDone()
{
    // TODO: ASSERT they wrote enough
    m_responseStream.reset();
    if (!m_response.general.transferEncoding.empty()) {
        std::ostringstream os;
        os << m_responseTrailer << "\r\n";
        std::string str = os.str();
        LOG_TRACE(g_log) << str;
        m_conn->m_stream->write(str.c_str(), str.size());         
    }
    LOG_INFO(g_log) << m_request.requestLine << " " << m_response.status.status;
    m_conn->scheduleNextResponse(shared_from_this());
    if (!m_requestDone && hasRequestBody() && !m_willClose) {
        if (!m_requestStream) {
            m_requestStream = requestStream();
        }
        ASSERT(m_requestStream);
        transferStream(m_requestStream, NullStream::get());
    }
}


void
HTTP::respondError(ServerRequest::ptr request, Status status, const std::string &message, bool closeConnection)
{
    ASSERT(!request->committed());
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
HTTP::respondStream(ServerRequest::ptr request, Stream::ptr response)
{
    ASSERT(!request->committed());
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
                } catch (UnexpectedEofError) {
                    respondError(request, REQUESTED_RANGE_NOT_SATISFIABLE);
                    return;
                }
            } else {
                try {
                    transferStream(response, NullStream::get(), cr.first);
                } catch (UnexpectedEofError) {
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
            AcceptList available;
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
