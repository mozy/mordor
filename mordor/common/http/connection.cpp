// Copyright (c) 2009 - Decho Corp.

#include "connection.h"

#include "chunked.h"
#include "common/streams/buffered.h"
#include "common/streams/limited.h"
#include "common/streams/notify.h"
#include "common/streams/singleplex.h"

HTTP::Connection::Connection(Stream *stream, bool own)
: m_stream(stream),
  m_own(own)
{
    assert(stream);
    assert(stream->supportsRead());
    assert(stream->supportsWrite());
    if (!stream->supportsFindDelimited()) {
        try {
            BufferedStream *buffered = new BufferedStream(stream, own);
            buffered->allowPartialReads(true);
            m_stream = buffered;
            m_own = true;
        } catch (...) {
            if (own) {
                delete stream;
            }
            throw;
        }
    }
}

HTTP::Connection::~Connection()
{
    if (m_own) {
        delete m_stream;
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
        }
        if (entity.contentLength != ~0 && entity.contentLength != 0)
            return true;
        for (ParameterizedList::const_iterator it(general.transferEncoding.begin());
            it != general.transferEncoding.end();
            ++it) {
            if (it->value != "identity")
                return true;
        }
        return false;
    } else {
        // Response
        switch (method) {
            case HEAD:
            case TRACE:
                return false;
        }
        if ((int)status >= 100 && status <= 199 ||
            (int)status == 204 ||
            (int)status == 304 ||
            method == HEAD)
            return false;
        for (ParameterizedList::const_iterator it(general.transferEncoding.begin());
            it != general.transferEncoding.end();
            ++it) {
            if (it->value != "identity")
                return true;
        }
        // TODO: if (entity.contentType.major == "multipart") return true;
        if (entity.contentLength == 0)
            return false;
        return true;
    }
}

Stream *
HTTP::Connection::getStream(const GeneralHeaders &general,
                            const EntityHeaders &entity,
                            Method method,
                            Status status,
                            boost::function<void()> notifyOnEof,
                            boost::function<void()> notifyOnException,
                            bool forRead)
{
    assert(hasMessageBody(general, entity, method, status));
    std::auto_ptr<Stream> stream;
    if (forRead) {
        stream.reset(new SingleplexStream(m_stream, SingleplexStream::READ, false));
    } else {
        stream.reset(new SingleplexStream(m_stream, SingleplexStream::WRITE, false));
    }
    Stream *baseStream = stream.get();
    bool notifyOnClose = false;
    for (ParameterizedList::const_iterator it(general.transferEncoding.begin());
        it != general.transferEncoding.end();
        ++it) {
        if (it->value == "chunked") {
            ChunkedStream *chunked = new ChunkedStream(stream.get());
            stream.release();
            stream.reset(chunked);
        } else if (it->value == "deflate") {
            // TODO: ZlibStream
            assert(false);
        } else if (it->value == "gzip" || it->value == "x-gzip") {
            // TODO: GzipStream
            assert(false);
        } else if (it->value == "compress" || it->value == "x-compress") {
            assert(false);
        } else if (it->value == "identity") {
            assert(false);
        } else {
            assert(false);
        }
    }
    if (stream.get() != baseStream) {
    } else if (entity.contentLength != ~0) {
        LimitedStream *limited = new LimitedStream(stream.get(), entity.contentLength);
        stream.release();
        stream.reset(limited);
    } else if (entity.contentType.type == "multipart") {
        // Getting stream to pass to multipart; self-delimiting
    } else {
        // Delimited by closing the connection
    }
    NotifyStream *notify = new NotifyStream(stream.get());
    stream.release();
    stream.reset(notify);
    notify->notifyOnClose = notifyOnEof;
    notify->notifyOnEof = notifyOnEof;
    notify->notifyOnException = notifyOnException;
    return stream.release();
}
