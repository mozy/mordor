// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "buffered.h"

#include "mordor/config.h"
#include "mordor/exception.h"
#include "mordor/log.h"

namespace Mordor {

static ConfigVar<size_t>::ptr g_defaultBufferSize =
    Config::lookup<size_t>("stream.buffered.defaultbuffersize", 65536,
    "Default buffer size for new BufferedStreams");

static Logger::ptr g_log = Log::lookup("mordor:streams:buffered");

BufferedStream::BufferedStream(Stream::ptr parent, bool own)
: FilterStream(parent, own)
{
    m_bufferSize = g_defaultBufferSize->val();
    m_allowPartialReads = false;
}

void
BufferedStream::close(CloseType type)
{
    MORDOR_LOG_VERBOSE(g_log) << this << " close(" << type << ")";
    if (type & READ)
        m_readBuffer.clear();
    try {
        if ((type & WRITE) && m_writeBuffer.readAvailable())
            flush();
    } catch (...) {
        if (ownsParent())
            parent()->close(type);
        throw;
    }
    if (ownsParent())
        parent()->close(type);
}

size_t
BufferedStream::read(Buffer &buffer, size_t length)
{
    return readInternal(buffer, length);
}

size_t
BufferedStream::read(void *buffer, size_t length)
{
    return readInternal(buffer, length);
}

// Buffer keeps track of its position automatically
static void advance(Buffer &buffer, size_t amount)
{}

// void * does not
static void advance(void *&buffer, size_t amount)
{
    (unsigned char *&)buffer += amount;
}

template <class T>
size_t
BufferedStream::readInternal(T &buffer, size_t length)
{
    MORDOR_ASSERT(!m_writeBuffer.readAvailable() || !supportsSeek());
    size_t remaining = length;

    size_t buffered = std::min(m_readBuffer.readAvailable(), remaining);
    m_readBuffer.copyOut(buffer, buffered);
    m_readBuffer.consume(buffered);
    remaining -= buffered;

    MORDOR_LOG_VERBOSE(g_log) << this << " read(" << length << "): "
        << buffered << " read from buffer";

    if (remaining == 0)
        return length;

    if (buffered == 0 || !m_allowPartialReads) {
        size_t result;
        do {
            // Read enough to satisfy this request, plus up to a multiple of
            // the buffer size
            size_t todo = ((remaining - 1) / m_bufferSize + 1) * m_bufferSize;
            try {
                MORDOR_LOG_TRACE(g_log) << this << " parent()->read(" << todo
                    << ")";
                result = parent()->read(m_readBuffer, todo);
                MORDOR_LOG_DEBUG(g_log) << this << " parent()->read(" << todo
                    << "): " << result;
            } catch (...) {
                if (remaining == length) {
                    MORDOR_LOG_VERBOSE(g_log) << this << " forwarding exception";
                    throw;
                } else {
                    MORDOR_LOG_VERBOSE(g_log) << this << " swallowing exception";
                    // Swallow the exception
                    return length - remaining;
                }
            }

            buffered = std::min(m_readBuffer.readAvailable(), remaining);
            m_readBuffer.copyOut(buffer, buffered);
            m_readBuffer.consume(buffered);
            advance(buffer, buffered);
            remaining -= buffered;
        } while (remaining > 0 && !m_allowPartialReads && result != 0);
    }

    return length - remaining;
}

size_t
BufferedStream::write(const Buffer &buffer, size_t length)
{
    MORDOR_ASSERT(!m_readBuffer.readAvailable() || !supportsSeek());
    m_writeBuffer.copyIn(buffer, length);
    size_t result = flushWrite(length);
    // Partial writes not allowed
    MORDOR_ASSERT(result == length);
    return result;
}

size_t
BufferedStream::write(const void *buffer, size_t length)
{
    MORDOR_ASSERT(!m_readBuffer.readAvailable() || !supportsSeek());
    m_writeBuffer.reserve(std::max(m_bufferSize, length));
    m_writeBuffer.copyIn(buffer, length);
    size_t result = flushWrite(length);
    // Partial writes not allowed
    MORDOR_ASSERT(result == length);
    return result;
}

size_t
BufferedStream::flushWrite(size_t length)
{
    while (m_writeBuffer.readAvailable() >= m_bufferSize)
    {
        size_t result;
        try {
            MORDOR_LOG_TRACE(g_log) << this << " parent()->write("
                << m_writeBuffer.readAvailable() << ")";
            result = parent()->write(m_writeBuffer, m_writeBuffer.readAvailable());
            MORDOR_LOG_DEBUG(g_log) << this << " parent()->write("
                << m_writeBuffer.readAvailable() << "): " << result;
        } catch (...) {
            // If this entire write is still in our buffer,
            // back it out and report the error
            if (m_writeBuffer.readAvailable() >= length) {
                MORDOR_LOG_VERBOSE(g_log) << this << " forwarding exception";
                Buffer tempBuffer;
                tempBuffer.copyIn(m_writeBuffer, m_writeBuffer.readAvailable()
                    - length);
                m_writeBuffer.clear();
                m_writeBuffer.copyIn(tempBuffer);
                throw;
            } else {
                // Otherwise we have to say we succeeded,
                // because we're not allowed to have a partial
                // write, and we can't report an error because
                // the caller will think he needs to repeat
                // the entire write
                MORDOR_LOG_VERBOSE(g_log) << this << " swallowing exception";
                return length;
            }
        }
        m_writeBuffer.consume(result);
    }
    return length;
}

long long
BufferedStream::seek(long long offset, Anchor anchor)
{
    // My head asplodes!  Deal with it when someone actually does random access
    // read and writes;
    MORDOR_ASSERT(!(m_readBuffer.readAvailable() && m_writeBuffer.readAvailable()));
    if (offset == 0 && anchor == CURRENT) {
        MORDOR_LOG_VERBOSE(g_log) << this << " parent()->seek(0, CURRENT) - "
            << m_readBuffer.readAvailable() << " + "
            << m_writeBuffer.readAvailable();
        return parent()->seek(offset, anchor)
            - m_readBuffer.readAvailable()
            + m_writeBuffer.readAvailable();
    }
    flush();
    // TODO: optimized forward seek
    if (anchor == CURRENT) {        
        // adjust the buffer having modified the actual stream position
        offset -= m_readBuffer.readAvailable();
    }
    m_readBuffer.clear();
    return parent()->seek(offset, anchor);
}

long long
BufferedStream::size()
{
    long long size = parent()->size();
    if (supportsSeek()) {
        return std::max(size, seek(0, CURRENT));
    } else {
        // not a seekable stream; we can only write to the end
        size += m_writeBuffer.readAvailable();
    }
    return size;
}

void
BufferedStream::truncate(long long size)
{
    MORDOR_ASSERT(!m_readBuffer.readAvailable() || !supportsSeek());
    flush();
    // TODO: truncate m_readBuffer at the end
    parent()->truncate(size);
}

void
BufferedStream::flush()
{
    while (m_writeBuffer.readAvailable()) {
        MORDOR_LOG_TRACE(g_log) << this << " parent()->write("
            << m_writeBuffer.readAvailable() << ")";
        size_t result = parent()->write(m_writeBuffer, m_writeBuffer.readAvailable());
        MORDOR_LOG_DEBUG(g_log) << this << " parent()->write("
            << m_writeBuffer.readAvailable() << "): " << result;
        MORDOR_ASSERT(result > 0);
        m_writeBuffer.consume(result);
    }
    parent()->flush();
}

ptrdiff_t
BufferedStream::find(char delim, size_t sanitySize, bool throwIfNotFound)
{
    MORDOR_ASSERT(!m_writeBuffer.readAvailable() || !supportsSeek());
    if (sanitySize == (size_t)~0)
        sanitySize = 2 * m_bufferSize;
    ++sanitySize;
    while (true) {
        size_t readAvailable = m_readBuffer.readAvailable();
        if (readAvailable > 0) {
            ptrdiff_t result = m_readBuffer.find(delim, std::min(sanitySize, readAvailable));
            if (result != -1) {
                return result;
            }
        }
        if (readAvailable >= sanitySize) {
            if (throwIfNotFound)
                MORDOR_THROW_EXCEPTION(BufferOverflowException());
            return -(ptrdiff_t)m_readBuffer.readAvailable() - 1;
        }

        MORDOR_LOG_TRACE(g_log) << this << " parent()->read(" << m_bufferSize
            << ")";
        size_t result = parent()->read(m_readBuffer, m_bufferSize);
        MORDOR_LOG_DEBUG(g_log) << this << " parent()->read(" << m_bufferSize
            << "): " << result;
        if (result == 0) {
            // EOF
            if (throwIfNotFound)
                MORDOR_THROW_EXCEPTION(UnexpectedEofException());
            return -(ptrdiff_t)m_readBuffer.readAvailable() - 1;
        }
    }
}

ptrdiff_t
BufferedStream::find(const std::string &str, size_t sanitySize, bool throwIfNotFound)
{
    MORDOR_ASSERT(!m_writeBuffer.readAvailable() || !supportsSeek());
    if (sanitySize == (size_t)~0)
        sanitySize = 2 * m_bufferSize;
    sanitySize += str.size();
    while (true) {
        size_t readAvailable = m_readBuffer.readAvailable();
        if (readAvailable > 0) {
            ptrdiff_t result = m_readBuffer.find(str, std::min(sanitySize, readAvailable));
            if (result != -1) {
                return result;
            }
        }
        if (readAvailable >= sanitySize) {
            if (throwIfNotFound)
                MORDOR_THROW_EXCEPTION(BufferOverflowException());
            return -(ptrdiff_t)m_readBuffer.readAvailable() - 1;
        }

        MORDOR_LOG_TRACE(g_log) << this << " parent()->read(" << m_bufferSize
            << ")";
        size_t result = parent()->read(m_readBuffer, m_bufferSize);
        MORDOR_LOG_DEBUG(g_log) << this << " parent()->read(" << m_bufferSize
            << "): " << result;
        if (result == 0) {
            // EOF
            if (throwIfNotFound)
                MORDOR_THROW_EXCEPTION(UnexpectedEofException());
            return -(ptrdiff_t)m_readBuffer.readAvailable() - 1;
        }
    }
}

void
BufferedStream::unread(const Buffer &b, size_t len)
{
    MORDOR_ASSERT(supportsUnread());
    Buffer tempBuffer;
    tempBuffer.copyIn(b, len);
    tempBuffer.copyIn(m_readBuffer);
    m_readBuffer.clear();
    m_readBuffer.copyIn(tempBuffer);
}

}
