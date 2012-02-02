// Copyright (c) 2009 - Mozy, Inc.

#include "http.h"

#include "mordor/fiber.h"
#include "mordor/http/client.h"
#include "mordor/socket.h"
#include "null.h"
#include "transfer.h"

using namespace Mordor::HTTP;

namespace Mordor {

HTTPStream::HTTPStream(const URI &uri, RequestBroker::ptr requestBroker,
    boost::function<bool (size_t)> delayDg)
: FilterStream(Stream::ptr(), false),
  m_requestBroker(requestBroker),
  m_pos(0),
  m_size(-1),
  m_sizeAdvice(-1),
  m_readAdvice(~0ull),
  m_writeAdvice(~0ull),
  m_readRequested(0ull),
  m_writeRequested(0ull),
  m_delayDg(delayDg),
  mp_retries(NULL),
  m_writeInProgress(false),
  m_abortWrite(false)
{
    m_requestHeaders.requestLine.uri = uri;
}

HTTPStream::HTTPStream(const Request &requestHeaders, RequestBroker::ptr requestBroker,
    boost::function<bool (size_t)> delayDg)
: FilterStream(Stream::ptr(), false),
  m_requestHeaders(requestHeaders),
  m_requestBroker(requestBroker),
  m_pos(0),
  m_size(-1),
  m_sizeAdvice(-1),
  m_readAdvice(~0ull),
  m_writeAdvice(~0ull),
  m_readRequested(0ull),
  m_writeRequested(0ull),
  m_delayDg(delayDg),
  mp_retries(NULL),
  m_writeInProgress(false),
  m_abortWrite(false)
{}

HTTPStream::~HTTPStream()
{
    clearParent();
}

ETag
HTTPStream::eTag()
{
    stat();
    return m_eTag;
}

void
HTTPStream::start()
{
    start(m_readAdvice == 0ull ? (size_t)-1 : 0u);
}

void
HTTPStream::start(size_t length)
{
    if (parent() && !parent()->supportsRead())
        clearParent();
    if (!parent() || m_readRequested == 0) {
        if (parent()) {
            // We've read everything, but haven't triggered EOF yet
            MORDOR_ASSERT(m_readRequested == 0);
            try {
                char byte;
                MORDOR_VERIFY(parent()->read(&byte, 1) == 0);
            } catch (...) {
                // Ignore errors finishing the request
            }
        }
        // Don't bother doing a request that will never do anything
        if (m_size >= 0 && m_pos >= m_size) {
            parent(NullStream::get_ptr());
            return;
        }
        m_requestHeaders.requestLine.method = GET;
        m_requestHeaders.request.ifNoneMatch.clear();
        m_requestHeaders.general.transferEncoding.clear();
        m_requestHeaders.entity.contentLength = ~0ull;
        m_requestHeaders.entity.contentRange = ContentRange();
        if (!m_eTag.unspecified) {
            if (m_pos != 0) {
                m_requestHeaders.request.ifRange = m_eTag;
                m_requestHeaders.request.ifMatch.clear();
            } else {
                m_requestHeaders.request.ifMatch.insert(m_eTag);
                m_requestHeaders.request.ifRange = ETag();
            }
        } else {
            m_requestHeaders.request.ifRange = ETag();
            m_requestHeaders.request.ifMatch.clear();
        }
        m_requestHeaders.request.range.clear();
        if (m_pos != 0 || (m_readAdvice != ~0ull && length != (size_t)-1)) {
            if (length == (size_t)-1)
                m_readRequested = ~0ull;
            else
                m_readRequested = (std::max)((unsigned long long)length,
                    m_readAdvice);
            m_requestHeaders.request.range.push_back(
                std::make_pair((unsigned long long)m_pos,
                m_readRequested == ~0ull ? ~0ull : m_pos + m_readRequested - 1));
        } else {
            m_readRequested = ~0ull;
        }
        ClientRequest::ptr request = m_requestBroker->request(m_requestHeaders);
        const Response &response = request->response();
        Stream::ptr responseStream;
        switch (response.status.status) {
            case OK:
            case PARTIAL_CONTENT:
                m_response = response;
                if (response.entity.contentRange.instance != ~0ull)
                    m_size = (long long)response.entity.contentRange.instance;
                else if (response.entity.contentLength != ~0ull)
                    m_size = (long long)response.entity.contentLength;
                else
                    m_size = -2;
                if (m_eTag.unspecified &&
                    !response.response.eTag.unspecified) {
                    m_eTag = response.response.eTag;
                } else if (!m_eTag.unspecified &&
                    !response.response.eTag.unspecified &&
                    response.response.eTag != m_eTag) {
                    m_eTag = response.response.eTag;
                    if (m_pos != 0 && response.status.status ==
                        PARTIAL_CONTENT) {
                        // Server doesn't support If-Range
                        request->cancel(true);
                        parent(Stream::ptr());
                    } else {
                        parent(request->responseStream());
                    }
                    m_pos = 0;
                    MORDOR_THROW_EXCEPTION(EntityChangedException());
                }
                responseStream = request->responseStream();
                // Server doesn't support Range
                if (m_pos != 0 && response.status.status == OK) {
                    m_readRequested = ~0ull;
                    transferStream(responseStream, NullStream::get(), m_pos);
                }
                parent(responseStream);
                break;
            default:
                MORDOR_THROW_EXCEPTION(InvalidResponseException(request));
        }
    }
}

bool
HTTPStream::checkModified()
{
    MORDOR_ASSERT(!m_eTag.unspecified);
    if (parent())
        clearParent();

    m_requestHeaders.requestLine.method = GET;
    m_requestHeaders.request.ifMatch.clear();
    m_requestHeaders.request.ifRange = ETag();
    m_requestHeaders.request.range.clear();
    m_requestHeaders.request.ifNoneMatch.insert(m_eTag);
    m_requestHeaders.general.transferEncoding.clear();
    m_requestHeaders.entity.contentLength = ~0ull;
    m_requestHeaders.entity.contentRange = ContentRange();
    if (m_pos != 0 || m_readAdvice != ~0ull) {
        m_readRequested = m_readAdvice == 0ull ? ~0ull : m_readAdvice;
        m_requestHeaders.request.range.push_back(
            std::make_pair((unsigned long long)m_pos,
            m_readRequested == ~0ull ? ~0ull : m_pos + m_readRequested - 1));
    } else {
        m_readRequested = ~0ull;
    }
    ClientRequest::ptr request = m_requestBroker->request(m_requestHeaders);
    const Response &response = request->response();
    switch (response.status.status) {
        case OK:
        case PARTIAL_CONTENT:
        case NOT_MODIFIED:
            m_response = response;
            if (response.entity.contentRange.instance != ~0ull)
                m_size = (long long)response.entity.contentRange.instance;
            else if (response.entity.contentLength != ~0ull)
                m_size = (long long)response.entity.contentLength;
            else
                m_size = -2;
            break;
        default:
            MORDOR_THROW_EXCEPTION(InvalidResponseException(request));
    }
    if (response.status.status != NOT_MODIFIED)
        m_eTag = response.response.eTag;
    else
        return false;
    Stream::ptr responseStream = request->responseStream();

    // Server doesn't support Range
    if (m_pos != 0 && response.status.status == OK) {
        // We don't really care about any data transfer problems here,
        // since that's not what the caller is asking for
        try {
            m_readRequested = ~0ull;
            transferStream(responseStream, NullStream::get(), m_pos);
        } catch (...) {
            return true;
        }
    }
    parent(responseStream);
    return true;
}

const HTTP::Response &
HTTPStream::response()
{
    stat();
    return m_response;
}

size_t
HTTPStream::read(Buffer &buffer, size_t length)
{
    size_t localRetries = 0;
    size_t *retries = mp_retries ? mp_retries : &localRetries;
    while (true) {
        // remember previous retry count
        size_t prevRetries = *retries;
        bool new_request = !parent();
        start(length);

        MORDOR_ASSERT(parent());
        try {
            try {
                size_t result = parent()->read(buffer, length);
                m_pos += result;
                m_readRequested -= result;
                if (result > 0)
                    *retries = 0;   // only do this if we've made progress
                return result;
            } catch(...) {
                // if this was a new request, RetryRequestBroker reset the retry count to 0
                // undo this, so we don't get stuck in an infinite retry loop if the server
                // consistently hangs up on us immediately after 200 OK
                if (new_request)
                    *retries = prevRetries;
                throw;
            }
        } catch (SocketException &) {
            parent(Stream::ptr());
            if (!m_delayDg || !m_delayDg(++*retries))
                throw;
            continue;
        } catch (UnexpectedEofException &) {
            parent(Stream::ptr());
            if (!m_delayDg || !m_delayDg(++*retries))
                throw;
            continue;
        }
    }
}

void
HTTPStream::doWrite(ClientRequest::ptr request)
{
    parent(request->requestStream());
    m_writeFuture2.reset();
    m_writeFuture.signal();
    m_writeFuture2.wait();
    if (m_abortWrite)
        MORDOR_THROW_EXCEPTION(OperationAbortedException());
    MORDOR_ASSERT(!parent());
}

void
HTTPStream::startWrite()
{
    try {
        m_writeRequest = m_requestBroker->request(m_requestHeaders,
            false, boost::bind(&HTTPStream::doWrite, this, _1));
        m_writeInProgress = false;
        m_writeFuture.signal();
    } catch (OperationAbortedException &) {
        if (!m_abortWrite)
            m_writeException = boost::current_exception();
        m_writeInProgress = false;
        m_writeFuture.signal();
    } catch (...) {
        m_writeException = boost::current_exception();
        m_writeInProgress = false;
        m_writeFuture.signal();
    }
}

size_t
HTTPStream::write(const Buffer &buffer, size_t length)
{
    MORDOR_ASSERT(m_sizeAdvice == -1 ||
        (unsigned long long)m_pos + length <= (unsigned long long)m_sizeAdvice);
    if (parent() && !parent()->supportsWrite())
        clearParent();
    if (!parent()) {
        m_requestHeaders.requestLine.method = PUT;
        m_requestHeaders.request.ifMatch.clear();
        m_requestHeaders.request.ifRange = ETag();
        m_requestHeaders.request.range.clear();
        m_requestHeaders.request.ifNoneMatch.clear();

        if (m_pos != 0 || m_writeAdvice != ~0ull) {
            m_requestHeaders.entity.contentRange.first = m_pos;
            if (m_sizeAdvice != -1) {
                m_writeRequested = (std::max)((unsigned long long)length,
                    m_writeAdvice);
                m_writeRequested = (std::min)(m_pos + m_writeRequested,
                    (unsigned long long)m_sizeAdvice) - m_pos;
                m_requestHeaders.entity.contentLength = m_writeRequested;
                m_requestHeaders.entity.contentRange.last = m_pos +
                    m_writeRequested - 1;
            } else {
                m_requestHeaders.entity.contentLength = ~0ull;
                m_requestHeaders.entity.contentRange.last = ~0ull;
                m_writeRequested = m_writeAdvice;
            }
            m_requestHeaders.entity.contentRange.instance =
                (unsigned long long)m_sizeAdvice;
        } else {
            m_requestHeaders.entity.contentLength =
                (unsigned long long)m_sizeAdvice;
            m_requestHeaders.entity.contentRange = ContentRange();
            m_writeRequested = m_writeAdvice;
        }
        if (m_sizeAdvice == -1) {
            if (m_requestHeaders.general.transferEncoding.empty())
                m_requestHeaders.general.transferEncoding.push_back("chunked");
        } else {
            m_requestHeaders.general.transferEncoding.clear();
        }

        m_writeException = boost::exception_ptr();
        m_writeFuture.reset();
        // Have to schedule this because RequestBroker::request doesn't return
        // until the entire request is complete
        m_writeInProgress = true;
        MORDOR_ASSERT(Scheduler::getThis());
        Scheduler::getThis()->schedule(
            boost::bind(&HTTPStream::startWrite, this));
        m_writeFuture.wait();
        if (m_writeException)
            Mordor::rethrow_exception(m_writeException);
        MORDOR_ASSERT(parent());
    }
    size_t result = parent()->write(buffer,
        (size_t)(std::min)((unsigned long long)length, m_writeRequested));
    m_pos += result;
    m_writeRequested -= result;
    if (m_writeRequested == 0)
        close();
    return result;
}

long long
HTTPStream::seek(long long offset, Anchor anchor)
{
    switch (anchor) {
        case CURRENT:
            offset = m_pos + offset;
            break;
        case END:
            stat();
            MORDOR_ASSERT(m_size >= 0);
            offset = m_size + offset;
            break;
        case BEGIN:
            break;
        default:
            MORDOR_NOTREACHED();
    }
    if (offset < 0)
        MORDOR_THROW_EXCEPTION(std::invalid_argument(
            "resulting offset is before the beginning of the file"));
    if (offset == m_pos)
        return m_pos;
    // Attempt to do an optimized forward seek
    if (offset > m_pos && m_readRequested != ~0ull && parent() &&
        parent()->supportsRead() &&
        m_pos + m_readRequested < (unsigned long long)offset) {
        try {
            transferStream(*this, NullStream::get(), offset - m_pos);
            MORDOR_ASSERT(m_pos == offset);
            return m_pos;
        } catch (...) {
            parent(Stream::ptr());
        }
    } else {
        parent(Stream::ptr());
    }
    return m_pos = offset;
}

bool
HTTPStream::supportsSize()
{
    stat();
    MORDOR_ASSERT(m_size != -1);
    MORDOR_ASSERT(m_size >= -2);
    if (m_size == -2)
        return false;
    else
        return true;
}

void
HTTPStream::stat()
{
    if (m_size == -1) {
        clearParent();
        m_requestHeaders.requestLine.method = HEAD;
        m_requestHeaders.request.ifMatch.clear();
        m_requestHeaders.request.ifRange = ETag();
        m_requestHeaders.request.range.clear();
        m_requestHeaders.request.ifNoneMatch.clear();
        m_requestHeaders.general.transferEncoding.clear();
        m_requestHeaders.entity.contentLength = ~0ull;
        m_requestHeaders.entity.contentRange = ContentRange();
        ClientRequest::ptr request = m_requestBroker->request(m_requestHeaders);
        const Response &response = request->response();
        switch (response.status.status) {
            case OK:
                m_response = response;
                if (response.entity.contentLength != ~0ull)
                    m_size = (long long)response.entity.contentLength;
                else
                    m_size = -2;
                if (m_eTag.unspecified &&
                    !response.response.eTag.unspecified) {
                    m_eTag = response.response.eTag;
                } else if (!m_eTag.unspecified &&
                    response.response.eTag != m_eTag) {
                    m_eTag = response.response.eTag;
                    parent(Stream::ptr());
                    m_pos = 0;
                    MORDOR_THROW_EXCEPTION(EntityChangedException());
                }
                break;
            default:
                MORDOR_THROW_EXCEPTION(InvalidResponseException(request));
        }
    }
}

long long
HTTPStream::size()
{
    stat();
    MORDOR_ASSERT(supportsSize());
    return m_size;
}

void
HTTPStream::truncate(long long size)
{
    if (parent())
        clearParent();
    m_requestHeaders.requestLine.method = PUT;
    m_requestHeaders.request.ifMatch.clear();
    m_requestHeaders.request.ifRange = ETag();
    m_requestHeaders.request.range.clear();
    m_requestHeaders.request.ifNoneMatch.clear();
    m_requestHeaders.general.transferEncoding.clear();
    m_requestHeaders.entity.contentLength = 0;
    m_requestHeaders.entity.contentRange = ContentRange(~0ull, ~0ull, size);
    ClientRequest::ptr request = m_requestBroker->request(m_requestHeaders);
    if (request->response().status.status != OK)
        MORDOR_THROW_EXCEPTION(InvalidResponseException(request));
    request->finish();
}

void
HTTPStream::close(CloseType type)
{
    if ((type & WRITE) && m_sizeAdvice == 0ll) {
        // If we weren't supposed to write anything, we shouldn't have written
        // anything :)
        MORDOR_ASSERT(!parent());
        m_requestHeaders.requestLine.method = PUT;
        m_requestHeaders.request.ifMatch.clear();
        m_requestHeaders.request.ifRange = ETag();
        m_requestHeaders.request.range.clear();
        m_requestHeaders.request.ifNoneMatch.clear();
        m_requestHeaders.general.transferEncoding.clear();
        m_requestHeaders.entity.contentLength = 0;
        m_requestHeaders.entity.contentRange = ContentRange();
        ClientRequest::ptr request = m_requestBroker->request(m_requestHeaders);
        switch (request->response().status.status) {
            case OK:
            case CREATED:
                break;
            default:
                MORDOR_THROW_EXCEPTION(InvalidResponseException(m_writeRequest));
        }
        request->finish();
        m_sizeAdvice = ~0ull;
        return;
    }
    if (parent() && (type & WRITE) && parent()->supportsWrite()) {
        parent()->close();
        parent(Stream::ptr());
        m_writeFuture.reset();
        m_writeFuture2.signal();
        m_writeFuture.wait();
        if (m_writeException)
            Mordor::rethrow_exception(m_writeException);
        MORDOR_ASSERT(m_writeRequest);
        switch ((int)m_writeRequest->response().status.status) {
            case OK:
            case CREATED:
            case 207: // Partial Update OK, from http://www.hpl.hp.com/personal/ange/archives/archives-97/http-wg-archive/2530.html
                m_response = m_writeRequest->response();
                try {
                    m_writeRequest->finish();
                } catch (...) {
                    m_writeRequest.reset();
                    throw;
                }
                m_writeRequest.reset();
                break;
            default:
                MORDOR_THROW_EXCEPTION(InvalidResponseException(m_writeRequest));
        }
    }
}

void
HTTPStream::flush(bool flushParent)
{
    if (parent() && parent()->supportsWrite())
        parent()->flush(flushParent);
}

void
HTTPStream::clearParent()
{
    if (m_writeInProgress) {
        m_abortWrite = true;
        m_writeFuture.reset();
        m_writeFuture2.signal();
        m_writeFuture.wait();
        MORDOR_ASSERT(!m_abortWrite);
    }
    parent(Stream::ptr());
}

}
