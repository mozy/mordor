// Copyright (c) 2009 - Decho Corp.

#include "zlib.h"

#include "mordor/common/assert.h"
#include "mordor/common/exception.h"

#ifdef MSVC
#pragma comment(lib, "zdll")
#endif

ZlibStream::ZlibStream(Stream::ptr parent, bool own, Type type, int level,
    int windowBits, int memlevel, Strategy strategy)
    : MutatingFilterStream(parent, own),
      m_closed(false)
{
   init(type, level, windowBits, memlevel, strategy);
}

void
ZlibStream::init(Type type, int level, int windowBits, int memlevel, Strategy strategy)
{
    ASSERT(supportsRead() || supportsWrite());
    ASSERT(!(supportsRead() && supportsWrite()));
    ASSERT((level >= 0 && level <= 9) || level == Z_DEFAULT_COMPRESSION);
    ASSERT(windowBits >= 8 && windowBits <= 15);
    ASSERT(memlevel >= 1 && memlevel <= 9);
    switch (type) {
        case ZLIB:
            break;
        case DEFLATE:
            windowBits = -windowBits;
            break;
        case GZIP:
            windowBits += 16;
            break;
        default:
            ASSERT(false);
    }
    int rc;
    memset(&m_strm, 0, sizeof(z_stream));
    if (supportsRead()) {
        rc = inflateInit2(&m_strm, windowBits);
    } else {
        rc = deflateInit2(&m_strm, level, Z_DEFLATED, windowBits, memlevel, (int)strategy);
    }
    switch (rc) {
        case Z_OK:
            break;
        case Z_MEM_ERROR:
            throw std::bad_alloc();
        case Z_STREAM_ERROR:
        {
            std::string message(m_strm.msg);
            if (supportsRead()) {
                inflateEnd(&m_strm);
            } else {
                deflateEnd(&m_strm);
            }
            throw std::runtime_error(message);
        }
        default:
            ASSERT(false);
    }
}

ZlibStream::ZlibStream(Stream::ptr parent, int level, int windowBits, int memlevel, Strategy strategy,
    bool own)
    : MutatingFilterStream(parent, own),
      m_closed(false)
{
    init(ZLIB, level, windowBits, memlevel, strategy);
}

ZlibStream::ZlibStream(Stream::ptr parent, bool own)
    : MutatingFilterStream(parent, own),
      m_closed(false)
{
    init(ZLIB);
}

ZlibStream::~ZlibStream()
{
    if (!m_closed) {
        if (supportsRead()) {
            inflateEnd(&m_strm);
        } else {
            deflateEnd(&m_strm);
        }
    }
}

void
ZlibStream::close(CloseType type)
{
    if ((type == READ && supportsWrite()) ||
        (type == WRITE && supportsRead()) ||
        m_closed) {
        MutatingFilterStream::close(type);
        return;
    }

    if (supportsRead()) {
        inflateEnd(&m_strm);
    } else {
        flush(Z_FINISH);
        deflateEnd(&m_strm);
    }
    m_closed = true;
    MutatingFilterStream::close(type);
}

size_t
ZlibStream::read(Buffer &b, size_t len)
{
    if (m_closed)
        return 0;
    b.reserve(len);
    struct iovec outbuf = b.writeBufs(len)[0];
    m_strm.next_out = (Bytef*)outbuf.iov_base;
    m_strm.avail_out = outbuf.iov_len;

    while (true) {
        std::vector<iovec> inbufs = m_inBuffer.readBufs();
        size_t avail_in;
        if (!inbufs.empty()) {
            m_strm.next_in = (Bytef*)inbufs[0].iov_base;
            avail_in = inbufs[0].iov_len;
            m_strm.avail_in = inbufs[0].iov_len;
        } else {
            m_strm.next_in = NULL;
            m_strm.avail_in = 0;
        }
        int rc = inflate(&m_strm, Z_NO_FLUSH);
        if (!inbufs.empty()) {
            m_inBuffer.consume(inbufs[0].iov_len - m_strm.avail_in);
        }
        size_t result;
        switch (rc) {
            case Z_STREAM_END:
                // May have still produced output
                result = outbuf.iov_len - m_strm.avail_out;
                b.produce(result);
                inflateEnd(&m_strm);
                m_closed = true;
                return result;
            case Z_OK:
                result = outbuf.iov_len - m_strm.avail_out;
                // It consumed input, but produced no output... DON'T return eof
                if (result == 0)
                    continue;
                b.produce(result);
                return result;
            case Z_BUF_ERROR:
                // no progress... we need to provide more input (since we're
                // guaranteed to provide output)
                ASSERT(m_strm.avail_in == 0);
                ASSERT(inbufs.empty());
                result = MutatingFilterStream::read(m_inBuffer, m_bufferSize);
                if (result == 0)
                    throw UnexpectedEofError();
                break;
            default:
                throw std::runtime_error("zlib error");
        }
    }
}

size_t
ZlibStream::write(const Buffer &b, size_t len)
{
    ASSERT(!m_closed);
    flushBuffer();
    while (true) {
        if (m_outBuffer.writeAvailable() == 0)
            m_outBuffer.reserve(m_bufferSize);
        struct iovec inbuf = b.readBufs()[0];
        struct iovec outbuf = m_outBuffer.writeBufs()[0];
        m_strm.next_in = (Bytef*)inbuf.iov_base;
        m_strm.avail_in = inbuf.iov_len;
        m_strm.next_out = (Bytef*)outbuf.iov_base;
        m_strm.avail_out = outbuf.iov_len;
        int rc = deflate(&m_strm, Z_NO_FLUSH);
        // We are always providing both input and output
        ASSERT(rc != Z_BUF_ERROR);
        // We're not doing Z_FINISH, so we shouldn't get EOF
        ASSERT(rc != Z_STREAM_END);
        size_t result;
        switch(rc) {
            case Z_OK:
                result = inbuf.iov_len - m_strm.avail_in;
                if (result == 0)
                    continue;
                m_outBuffer.produce(result);
                try {
                    flushBuffer();
                } catch (std::runtime_error) {
                    // Swallow it
                }
                return result;
            default:
                throw std::runtime_error("zlib error");
        }
    }
}

void
ZlibStream::flush()
{
    flush(Z_SYNC_FLUSH);
    MutatingFilterStream::flush();
}

void
ZlibStream::flush(int flush)
{
    flushBuffer();
    while (true) {
        if (m_outBuffer.writeAvailable() == 0)
            m_outBuffer.reserve(m_bufferSize);
        struct iovec outbuf = m_outBuffer.writeBufs()[0];
        m_strm.next_out = (Bytef*)outbuf.iov_base;
        m_strm.avail_out = outbuf.iov_len;
        int rc = deflate(&m_strm, flush);
        m_outBuffer.produce(outbuf.iov_len - m_strm.avail_out);
        ASSERT(flush == Z_FINISH || rc != Z_STREAM_END);
        switch (rc) {
            case Z_STREAM_END:
                m_closed = true;
                deflateEnd(&m_strm);
                flushBuffer();
                return;
            case Z_OK:
                break;
            case Z_BUF_ERROR:
                flushBuffer();
                return;
            default:
                throw std::runtime_error("zlib error");
        }
    }
}

void
ZlibStream::flushBuffer()
{
    while (m_outBuffer.readAvailable() > 0) {
        m_outBuffer.consume(MutatingFilterStream::write(m_outBuffer, m_outBuffer.readAvailable()));
    }
}
