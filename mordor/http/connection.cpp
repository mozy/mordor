// Copyright (c) 2009 - Mozy, Inc.

#include "connection.h"

#include "chunked.h"
#include "mordor/streams/buffered.h"
#include "mordor/streams/gzip.h"
#include "mordor/streams/limited.h"
#include "mordor/streams/notify.h"
#include "mordor/streams/singleplex.h"
#include "mordor/streams/zlib.h"

namespace Mordor {
namespace HTTP {

Connection::Connection(Stream::ptr stream)
: m_stream(stream)
{
    MORDOR_ASSERT(stream);
    MORDOR_ASSERT(stream->supportsRead());
    MORDOR_ASSERT(stream->supportsWrite());
    if (!stream->supportsUnread() || !stream->supportsFind()) {
        BufferedStream *buffered = new BufferedStream(stream);
        buffered->allowPartialReads(true);
        m_stream.reset(buffered);
    }
}

bool
Connection::hasMessageBody(const GeneralHeaders &general,
    const EntityHeaders &entity, const std::string &method, Status status,
    bool includeEmpty)
{
    // Connect escapes HTTP
    if (method == CONNECT && (status == OK || status == INVALID))
        return false;
    // http://www.w3.org/Protocols/rfc2616/rfc2616-sec9.html#sec9.8
    // A TRACE request MUST NOT include an entity.
    if (status == INVALID && method == TRACE)
        return false;
    // http://www.w3.org/Protocols/rfc2616/rfc2616-sec9.html#sec9.4
    // the server MUST NOT return a message-body in the response
    if (status != INVALID && method == HEAD)
        return false;
    // http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html#sec10.1
    // This class of status code indicates a provisional response, consisting only of the Status-Line and optional headers
    if (((int)status >= 100 && status <= 199) ||
        // http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html#sec10.2.5
        // The 204 response MUST NOT include a message-body
        (int)status == 204 ||
        // http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html#sec10.3.5
        // The 304 response MUST NOT contain a message-body
        (int)status == 304)
        return false;
    // http://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.4
    // 2.
    for (ParameterizedList::const_iterator it(general.transferEncoding.begin());
        it != general.transferEncoding.end();
        ++it) {
        if (stricmp(it->value.c_str(), "identity") != 0)
            return true;
    }
    // 3.
    if (entity.contentLength == 0)
        return includeEmpty;
    if (entity.contentLength != ~0ull)
        return true;
    // 4.
    if (entity.contentType.type == "multipart")
        return true;
    // 5. By the server closing the connection.
    // http://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.3
    // (by default, requests don't have a message body, because you can't
    // tell where it would end, without precluding the possibility of a
    // response)
    return status != INVALID;
}

Stream::ptr
Connection::getStream(const GeneralHeaders &general,
    const EntityHeaders &entity, const std::string &method, Status status,
    boost::function<void()> notifyOnEof,
    boost::function<void()> notifyOnException, bool forRead)
{
    MORDOR_ASSERT(hasMessageBody(general, entity, method, status));
    Stream::ptr stream;
    if (forRead) {
        stream.reset(new SingleplexStream(m_stream, SingleplexStream::READ, false));
    } else {
        stream.reset(new SingleplexStream(m_stream, SingleplexStream::WRITE, false));
    }
    Stream::ptr baseStream(stream);
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
            MORDOR_ASSERT(false);
        } else if (stricmp(it->value.c_str(), "identity") == 0) {
            MORDOR_ASSERT(false);
        } else {
            MORDOR_ASSERT(false);
        }
    }
    if (stream != baseStream) {
    } else if (entity.contentLength != ~0ull) {
        LimitedStream::ptr limited(new LimitedStream(stream, entity.contentLength));
        limited->strict(true);
        stream = limited;
    } else if (entity.contentType.type == "multipart") {
        // Getting stream to pass to multipart; self-delimiting
    } else {
        // Delimited by closing the connection
    }
    NotifyStream::ptr notify(new NotifyStream(stream));
    stream = notify;
    notify->notifyOnClose(notifyOnEof);
    notify->notifyOnEof = notifyOnEof;
    notify->notifyOnException = notifyOnException;
    return stream;
}

}}
