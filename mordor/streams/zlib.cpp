// Copyright (c) 2009 - Mozy, Inc.

#include "zlib.h"

#include "mordor/assert.h"
#include "mordor/exception.h"
#include "mordor/log.h"

#ifdef MSVC
#pragma comment(lib, "zdll")
#endif

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:streams:zlib");

ZlibStream::ZlibStream(Stream::ptr parent, bool own, Type type, int level,
    int windowBits, int memlevel, Strategy strategy, bool invert)
    : MutatingFilterStream(parent, own),
      m_closed(true)
{
   init(type, level, windowBits, memlevel, strategy, invert);
}

void
ZlibStream::init(Type type, int level, int windowBits, int memlevel, Strategy strategy, bool invert)
{
    MORDOR_ASSERT(supportsRead() || supportsWrite());
    MORDOR_ASSERT(!(supportsRead() && supportsWrite()));
    MORDOR_ASSERT((level >= 0 && level <= 9) || level == Z_DEFAULT_COMPRESSION);
    MORDOR_ASSERT(windowBits >= 8 && windowBits <= 15);
    MORDOR_ASSERT(memlevel >= 1 && memlevel <= 9);
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
            MORDOR_ASSERT(false);
    }
    m_windowBits = windowBits;
    m_level = level;
    m_memlevel = memlevel;
    m_strategy = strategy;
    m_doInflate = (!invert && supportsRead()) || (invert && supportsWrite());
    reset();
}

void
ZlibStream::reset()
{
    m_inBuffer.clear();
    m_outBuffer.clear(false);
    if (!m_closed) {
        if (m_doInflate) {
            inflateEnd(&m_strm);
        } else {
            deflateEnd(&m_strm);
        }
        m_closed = true;
    }
    int rc;
    memset(&m_strm, 0, sizeof(z_stream));
    if (m_doInflate) {
        rc = inflateInit2(&m_strm, m_windowBits);
    } else {
        rc = deflateInit2(&m_strm, m_level, Z_DEFLATED, m_windowBits, m_memlevel,
            (int)m_strategy);
    }
    switch (rc) {
        case Z_OK:
            m_closed = false;
            break;
        case Z_MEM_ERROR:
            throw std::bad_alloc();
        case Z_STREAM_ERROR:
        {
            std::string message(m_strm.msg ? m_strm.msg : "");
            if (m_doInflate) {
                inflateEnd(&m_strm);
            } else {
                deflateEnd(&m_strm);
            }
            throw std::runtime_error(message);
        }
        default:
            MORDOR_NOTREACHED();
    }
}

ZlibStream::ZlibStream(Stream::ptr parent, int level, int windowBits, int memlevel, Strategy strategy,
    bool own, bool invert)
    : MutatingFilterStream(parent, own),
      m_closed(true)
{
    init(ZLIB, level, windowBits, memlevel, strategy, invert);
}

ZlibStream::ZlibStream(Stream::ptr parent, bool own, bool invert)
    : MutatingFilterStream(parent, own),
      m_closed(true)
{
    init(ZLIB, Z_DEFAULT_COMPRESSION, 15, 8, DEFAULT, invert);
}

ZlibStream::~ZlibStream()
{
    if (!m_closed) {
        if (m_doInflate) {
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
        if (ownsParent())
            parent()->close(type);
        return;
    }
    if (supportsWrite())
        flush(Z_FINISH);
    if (m_doInflate)
        inflateEnd(&m_strm);
    else
        deflateEnd(&m_strm);
    m_closed = true;
    if (ownsParent())
        parent()->close(type);
}

size_t ZlibStream::doInflateForRead(Buffer &buffer, size_t length)
{
    if (m_closed)
        return 0;
    struct iovec outbuf = buffer.writeBuffer(length, false);
    m_strm.next_out = (Bytef*)outbuf.iov_base;
    m_strm.avail_out = outbuf.iov_len;

    while (true) {
        std::vector<iovec> inbufs = m_inBuffer.readBuffers();
        if (!inbufs.empty()) {
            m_strm.next_in = (Bytef*)inbufs[0].iov_base;
            m_strm.avail_in = inbufs[0].iov_len;
        } else {
            m_strm.next_in = NULL;
            m_strm.avail_in = 0;
        }
        int rc = inflate(&m_strm, Z_NO_FLUSH);
        MORDOR_LOG_DEBUG(g_log) << this << " inflate(("
            << (inbufs.empty() ? 0 : inbufs[0].iov_len) << ", "
            << outbuf.iov_len << ")): " << rc << " (" << m_strm.avail_in
            << ", " << m_strm.avail_out << ")";
        if (!inbufs.empty())
            m_inBuffer.consume(inbufs[0].iov_len - m_strm.avail_in);
        size_t result;
        switch (rc) {
            case Z_STREAM_END:
                // May have still produced output
                result = outbuf.iov_len - m_strm.avail_out;
                buffer.produce(result);
                inflateEnd(&m_strm);
                m_closed = true;
                return result;
            case Z_OK:
                result = outbuf.iov_len - m_strm.avail_out;
                // It consumed input, but produced no output... DON'T return eof
                if (result == 0)
                    continue;
                buffer.produce(result);
                return result;
            case Z_BUF_ERROR:
                // no progress... we need to provide more input (since we're
                // guaranteed to provide output)
                MORDOR_ASSERT(m_strm.avail_in == 0);
                MORDOR_ASSERT(inbufs.empty());
                result = parent()->read(m_inBuffer, m_bufferSize);
                if (result == 0)
                    MORDOR_THROW_EXCEPTION(UnexpectedEofException());
                break;
            case Z_MEM_ERROR:
                MORDOR_THROW_EXCEPTION(std::bad_alloc());
            case Z_NEED_DICT:
                MORDOR_THROW_EXCEPTION(NeedPresetDictionaryException());
            case Z_DATA_ERROR:
                MORDOR_THROW_EXCEPTION(CorruptedZlibStreamException());
            default:
                MORDOR_NOTREACHED();
        }
    }
}

size_t ZlibStream::doDeflateForRead(Buffer &buffer, size_t length)
{
    if (m_closed)
        return 0;
    struct iovec outbuf = buffer.writeBuffer(length, false);
    m_strm.next_out = (Bytef*)outbuf.iov_base;
    m_strm.avail_out = outbuf.iov_len;

    while (true) {
        if (m_inBuffer.readAvailable() == 0)
            parent()->read(m_inBuffer, m_bufferSize);
        struct iovec inbuf = m_inBuffer.readBuffer((size_t)~0, true);
        m_strm.next_in = (Bytef*)inbuf.iov_base;
        m_strm.avail_in = inbuf.iov_len;
        int rc = deflate(&m_strm, Z_FINISH);
        MORDOR_LOG_DEBUG(g_log) << this << " deflate((" << inbuf.iov_len << ", "
            << outbuf.iov_len << "), Z_FINISH): " << rc << " ("
            << m_strm.avail_in << ", " << m_strm.avail_out << ")";
        // We are always providing both input and output
        MORDOR_ASSERT(rc != Z_BUF_ERROR);
        m_inBuffer.consume(inbuf.iov_len - m_strm.avail_in);
        size_t result;
        switch(rc) {
            case Z_STREAM_END:
                // Pending output is flushed and there is enough output space
                result = outbuf.iov_len - m_strm.avail_out;
                buffer.produce(result);
                return result;
            case Z_OK:
                //if deflate returns with Z_OK, this function must be called again with Z_FINISH
                //and more output space (updated avail_out) but no more input data, until it returns
                //with Z_STREAM_END or an error.
                result = outbuf.iov_len - m_strm.avail_out;
                if (result == 0)
                    continue;
                buffer.produce(result);
                return result;
            default:
                MORDOR_NOTREACHED();
        }
    }
}

size_t ZlibStream::doInflateForWrite(const Buffer &buffer, size_t length)
{
    MORDOR_ASSERT(!m_closed);
    flushBuffer();
    while (true) {
        if (m_outBuffer.writeAvailable() == 0)
            m_outBuffer.reserve(m_bufferSize);
        struct iovec outbuf = m_outBuffer.writeBuffer(~0u, false);
        size_t len = (std::min)(length, buffer.readAvailable());
        struct iovec inbuf = buffer.readBuffer(len, true);
        m_strm.next_in = (Bytef*)inbuf.iov_base;
        m_strm.avail_in = inbuf.iov_len;
        m_strm.next_out = (Bytef*)outbuf.iov_base;
        m_strm.avail_out = outbuf.iov_len;
        int rc = inflate(&m_strm, Z_NO_FLUSH);
        MORDOR_LOG_DEBUG(g_log) << this << " inflate(("
            << inbuf.iov_len << ", " << outbuf.iov_len << ")): " << rc
            << " (" << m_strm.avail_in << ", " << m_strm.avail_out << ")";
        // We are always providing both input and output
        MORDOR_ASSERT(rc != Z_BUF_ERROR);
        size_t result;
        switch (rc) {
            case Z_STREAM_END:
                // May have still produced output
                result = inbuf.iov_len - m_strm.avail_in;
                m_outBuffer.produce(outbuf.iov_len - m_strm.avail_out);
                m_closed = true;
                inflateEnd(&m_strm);
                try {
                    flushBuffer();
                } catch (std::runtime_error) {
                    // Swallow it
                }
                return result;
            case Z_OK:
                // some progress has been made (more input processed or more output produced)
                result = inbuf.iov_len - m_strm.avail_in;
                if (result == 0)
                    continue;
                m_outBuffer.produce(outbuf.iov_len - m_strm.avail_out);
                flushBuffer();
                return result;
            case Z_MEM_ERROR:
                MORDOR_THROW_EXCEPTION(std::bad_alloc());
            case Z_NEED_DICT:
                MORDOR_THROW_EXCEPTION(NeedPresetDictionaryException());
            case Z_DATA_ERROR:
                MORDOR_THROW_EXCEPTION(CorruptedZlibStreamException());
            default:
                MORDOR_NOTREACHED();
        }
    }
}

size_t ZlibStream::doDeflateForWrite(const Buffer &buffer, size_t length)
{
    MORDOR_ASSERT(!m_closed);
    flushBuffer();
    while (true) {
        if (m_outBuffer.writeAvailable() == 0)
            m_outBuffer.reserve(m_bufferSize);
        struct iovec inbuf = buffer.readBuffer(length, false);
        struct iovec outbuf = m_outBuffer.writeBuffer(~0u, false);
        m_strm.next_in = (Bytef*)inbuf.iov_base;
        m_strm.avail_in = inbuf.iov_len;
        m_strm.next_out = (Bytef*)outbuf.iov_base;
        m_strm.avail_out = outbuf.iov_len;
        int rc = deflate(&m_strm, Z_NO_FLUSH);
        MORDOR_LOG_DEBUG(g_log) << this << " deflate((" << inbuf.iov_len << ", "
            << outbuf.iov_len << "), Z_NO_FLUSH): " << rc << " ("
            << m_strm.avail_in << ", " << m_strm.avail_out << ")";
        // We are always providing both input and output
        MORDOR_ASSERT(rc != Z_BUF_ERROR);
        // We're not doing Z_FINISH, so we shouldn't get EOF
        MORDOR_ASSERT(rc != Z_STREAM_END);
        size_t result;
        switch(rc) {
            case Z_OK:
                result = inbuf.iov_len - m_strm.avail_in;
                if (result == 0)
                    continue;
                m_outBuffer.produce(outbuf.iov_len - m_strm.avail_out);
                try {
                    flushBuffer();
                } catch (std::runtime_error) {
                    // Swallow it
                }
                return result;
            default:
                MORDOR_NOTREACHED();
        }
    }
}

size_t
ZlibStream::read(Buffer &buffer, size_t length)
{
    if (m_doInflate)
        return doInflateForRead(buffer, length);
    else
        return doDeflateForRead(buffer, length);
}

size_t
ZlibStream::write(const Buffer &buffer, size_t length)
{
    if (m_doInflate)
        return doInflateForWrite(buffer, length);
    else
        return doDeflateForWrite(buffer, length);
}

void
ZlibStream::flush(bool flushParent)
{
    flush(Z_SYNC_FLUSH);
    if (flushParent)
        parent()->flush();
}

void
ZlibStream::flush(int flush)
{
    flushBuffer();
    while (true) {
        if (m_outBuffer.writeAvailable() == 0)
            m_outBuffer.reserve(m_bufferSize);
        struct iovec outbuf = m_outBuffer.writeBuffer(~0u, false);
        MORDOR_ASSERT(m_strm.avail_in == 0);
        m_strm.next_out = (Bytef*)outbuf.iov_base;
        m_strm.avail_out = outbuf.iov_len;
        int rc;
        if (m_doInflate) {
            rc = inflate(&m_strm, flush);
            MORDOR_LOG_DEBUG(g_log) << this << " inflate((0, " << outbuf.iov_len
                << "), " << flush << "): " << rc << " (0, " << m_strm.avail_out
                << ")";
        } else {
            rc = deflate(&m_strm, flush);
            MORDOR_LOG_DEBUG(g_log) << this << " deflate((0, " << outbuf.iov_len
                << "), " << flush << "): " << rc << " (0, " << m_strm.avail_out
                << ")";
        }
        MORDOR_ASSERT(m_strm.avail_in == 0);
        m_outBuffer.produce(outbuf.iov_len - m_strm.avail_out);
        MORDOR_ASSERT(flush == Z_FINISH || rc != Z_STREAM_END);
        switch (rc) {
            case Z_STREAM_END:
                m_closed = true;
                if (m_doInflate)
                    inflateEnd(&m_strm);
                else
                    deflateEnd(&m_strm);
                flushBuffer();
                return;
            case Z_OK:
                break;
            case Z_BUF_ERROR:
                flushBuffer();
                return;
            default:
                MORDOR_NOTREACHED();
        }
    }
}

void
ZlibStream::flushBuffer()
{
    while (m_outBuffer.readAvailable() > 0)
        m_outBuffer.consume(parent()->write(m_outBuffer,
            m_outBuffer.readAvailable()));
}

}
