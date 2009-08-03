// Copyright (c) 2009 - Decho Corp.

#include "connection.h"

#include "chunked.h"
#include "mordor/common/streams/buffered.h"
#include "mordor/common/streams/gzip.h"
#include "mordor/common/streams/limited.h"
#include "mordor/common/streams/notify.h"
#include "mordor/common/streams/singleplex.h"
#include "mordor/common/streams/zlib.h"

HTTP::Connection::Connection(Stream::ptr stream)
: m_stream(stream)
{
    ASSERT(stream);
    ASSERT(stream->supportsRead());
    ASSERT(stream->supportsWrite());
    if (!stream->supportsUnread() || !stream->supportsFind()) {
        BufferedStream *buffered = new BufferedStream(stream);
        buffered->allowPartialReads(true);
        m_stream.reset(buffered);
    }
}

bool
HTTP::Connection::hasMessageBody(const GeneralHeaders &general,
                                 const EntityHeaders &entity,
                                 Method method,
                                 Status status)
{
    if (status == INVALID) {
        // Request
        switch (method) {
            case GET:
            case HEAD:
            case TRACE:
                return false;
            case CONNECT:
                return true;
            default:
                break;
        }
        if (entity.contentLength != ~0ull && entity.contentLength != 0)
            return true;
        for (ParameterizedList::const_iterator it(general.transferEncoding.begin());
            it != general.transferEncoding.end();
            ++it) {
            if (stricmp(it->value.c_str(), "identity") != 0)
                return true;
        }
        if (entity.contentType.type == "multipart")
            return true;
        throw std::runtime_error("Requests must have some way to determine when they end!");
    } else {
        // Response
        switch (method) {
            case HEAD:
            case TRACE:
                return false;
            default:
                break;
        }
        if (((int)status >= 100 && status <= 199) ||
            (int)status == 204 ||
            (int)status == 304 ||
            method == HEAD)
            return false;
        for (ParameterizedList::const_iterator it(general.transferEncoding.begin());
            it != general.transferEncoding.end();
            ++it) {
            if (stricmp(it->value.c_str(), "identity") != 0)
                return true;
        }
        if (entity.contentLength == 0)
            return false;
        if (entity.contentType.type == "multipart")
            return true;
        return true;
    }
}

Stream::ptr
HTTP::Connection::getStream(const GeneralHeaders &general,
                            const EntityHeaders &entity,
                            Method method,
                            Status status,
                            boost::function<void()> notifyOnEof,
                            boost::function<void()> notifyOnException,
                            bool forRead)
{
    ASSERT(hasMessageBody(general, entity, method, status));
    Stream::ptr stream;
    if (forRead) {
        stream.reset(new SingleplexStream(m_stream, SingleplexStream::READ, false));
    } else {
        stream.reset(new SingleplexStream(m_stream, SingleplexStream::WRITE, false));
    }
    Stream::ptr baseStream(stream);
    bool notifyOnClose = false;
    for (ParameterizedList::const_reverse_iterator it(general.transferEncoding.rbegin());
        it != general.transferEncoding.rend();
        ++it) {
        if (stricmp(it->value.c_str(), "chunked") == 0) {
            stream.reset(new ChunkedStream(stream));
        } else if (stricmp(it->value.c_str(), "deflate") == 0) {
            stream.reset(new ZlibStream(stream));
        } else if (stricmp(it->value.c_str(), "gzip") == 0 ||
            stricmp(it->value.c_str(), "x-gzip") == 0) {
            stream.reset(new GzipStream(stream));
        } else if (stricmp(it->value.c_str(), "compress") == 0 ||
            stricmp(it->value.c_str(), "x-compress") == 0) {
            ASSERT(false);
        } else if (stricmp(it->value.c_str(), "identity") == 0) {
            ASSERT(false);
        } else {
            ASSERT(false);
        }
    }
    if (stream != baseStream) {
    } else if (entity.contentLength != ~0ull) {
        stream.reset(new LimitedStream(stream, entity.contentLength));
    } else if (entity.contentType.type == "multipart") {
        // Getting stream to pass to multipart; self-delimiting
    } else {
        // Delimited by closing the connection
    }
    NotifyStream *notify = new NotifyStream(stream);
    stream.reset(notify);
    notify->notifyOnClose = notifyOnEof;
    notify->notifyOnEof = notifyOnEof;
    notify->notifyOnException = notifyOnException;
    return stream;
}
