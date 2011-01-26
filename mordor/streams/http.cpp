// Copyright (c) 2009 - Mozy, Inc.

#include "http.h"

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
  m_readAdvice(~0ull),
  m_readRequested(0ull),
  m_delayDg(delayDg),
  mp_retries(NULL)
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
  m_readAdvice(~0ull),
  m_readRequested(0ull),
  m_delayDg(delayDg),
  mp_retries(NULL)
{}

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
        parent(Stream::ptr());

    m_requestHeaders.requestLine.method = GET;
    m_requestHeaders.request.ifMatch.clear();
    m_requestHeaders.request.ifRange = ETag();
    m_requestHeaders.request.range.clear();
    m_requestHeaders.request.ifNoneMatch.insert(m_eTag);
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
        start(length);

        MORDOR_ASSERT(parent());
        try {
            size_t result = parent()->read(buffer, length);
            m_pos += result;
            m_readRequested -= result;
            *retries = 0;
            return result;
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
        m_requestHeaders.requestLine.method = HEAD;
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

}
