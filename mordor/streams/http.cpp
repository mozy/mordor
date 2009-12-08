// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "http.h"

#include "null.h"
#include "transfer.h"

using namespace Mordor::HTTP;

namespace Mordor {

HTTPStream::HTTPStream(const URI &uri, RequestBroker::ptr requestBroker)
: FilterStream(Stream::ptr(), false),
  m_uri(uri),
  m_requestBroker(requestBroker),
  m_pos(0),
  m_size(-1)
{
    MORDOR_ASSERT(uri.authority.hostDefined());
    MORDOR_ASSERT(uri.path.type == URI::Path::ABSOLUTE);
}

size_t
HTTPStream::read(Buffer &buffer, size_t length)
{
    if (m_size >= 0 && m_pos >= m_size)
        return 0;

    while (true) {
        if (!parent()) {
            while (true) {
                Request requestHeaders;
                requestHeaders.requestLine.uri = m_uri;
                if (!eTag.unspecified) {
                    if (m_pos != 0)
                        requestHeaders.request.ifRange = eTag;
                    else
                        requestHeaders.request.ifMatch.insert(eTag);
                }
                if (m_pos != 0)
                    requestHeaders.request.range.push_back(
                        std::make_pair((unsigned long long)m_pos, ~0ull));
                ClientRequest::ptr request;
                try {
                    request = m_requestBroker->request(requestHeaders);
                    // URI could have been changed by a permanent redirect
                    m_uri = requestHeaders.requestLine.uri;
                } catch (...) {
                    m_uri = requestHeaders.requestLine.uri;
                    throw;
                }
                const HTTP::Response response = request->response();
                Stream::ptr responseStream;
                switch (response.status.status) {
                    case OK:
                    case PARTIAL_CONTENT:
                        if (response.entity.contentRange.instance != ~0ull)
                            m_size = (long long)response.entity.contentRange.instance;
                        else if (response.entity.contentLength != ~0ull)
                            m_size = (long long)response.entity.contentLength;
                        else
                            m_size = -2;
                        if (eTag.unspecified &&
                            !response.response.eTag.unspecified) {
                            eTag = response.response.eTag;
                        } else if (!eTag.unspecified &&
                            !response.response.eTag.unspecified &&
                            response.response.eTag != eTag) {
                            eTag = response.response.eTag;
                            if (m_pos != 0 && response.status.status ==
                                PARTIAL_CONTENT) {
                                // Server doesn't support If-Range
                                request->cancel(true);
                                parent(Stream::ptr());
                            } else {
                                parent(request->responseStream());
                            }
                            MORDOR_THROW_EXCEPTION(EntityChangedException());
                        }
                        responseStream = request->responseStream();
                        if (m_pos != 0 && response.status.status == OK)
                            transferStream(responseStream,
                                NullStream::get(), m_pos);
                        parent(responseStream);
                        break;
                    default:
                        MORDOR_THROW_EXCEPTION(InvalidResponseException(request));
                }
                MORDOR_NOTREACHED();
            }
        }

        MORDOR_ASSERT(parent());
        try {
            size_t result = parent()->read(buffer, length);
            m_pos += result;
            return result;
        } catch (SocketException &) {
            parent(Stream::ptr());
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
            ensureSize();
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
    parent(Stream::ptr());
    return m_pos = offset;
}

bool
HTTPStream::supportsSize()
{
    ensureSize();
    MORDOR_ASSERT(m_size != -1);
    MORDOR_ASSERT(m_size >= -2);
    if (m_size == -2)
        return false;
    else
        return true;
}

void
HTTPStream::ensureSize()
{
    if (m_size == -1) {
        Request requestHeaders;
        requestHeaders.requestLine.method = HEAD;
        requestHeaders.requestLine.uri = m_uri;
        ClientRequest::ptr request;
        try {
            request = m_requestBroker->request(requestHeaders);
            // URI could have been changed by a permanent redirect
            m_uri = requestHeaders.requestLine.uri;
        } catch (...) {
            m_uri = requestHeaders.requestLine.uri;
            throw;
        }
        const HTTP::Response response = request->response();
        switch (response.status.status) {
            case OK:
                if (response.entity.contentLength != ~0ull)
                    m_size = (long long)response.entity.contentLength;
                else
                    m_size = -2;
                if (eTag.unspecified &&
                    !response.response.eTag.unspecified) {
                    eTag = response.response.eTag;
                } else if (!eTag.unspecified &&
                    response.response.eTag != eTag) {
                    eTag = response.response.eTag;
                    parent(Stream::ptr());
                    MORDOR_THROW_EXCEPTION(EntityChangedException());
                }
                break;
            default:
                MORDOR_THROW_EXCEPTION(InvalidResponseException(request));
        }
    }
}

}
