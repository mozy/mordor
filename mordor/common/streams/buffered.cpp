// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "buffered.h"

#include "mordor/common/exception.h"

BufferedStream::BufferedStream(Stream::ptr parent, bool own)
: FilterStream(parent, own)
{
    m_bufferSize = 65536;
    m_allowPartialReads = false;
}

void
BufferedStream::close(CloseType type)
{
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
BufferedStream::read(Buffer &b, size_t len)
{
    ASSERT(!m_writeBuffer.readAvailable() || !supportsSeek());
    size_t remaining = len;

    size_t buffered = std::min(m_readBuffer.readAvailable(), remaining);
    b.copyIn(m_readBuffer, buffered);
    m_readBuffer.consume(buffered);
    remaining -= buffered;

    if (remaining == 0) {
        return len;
    }

    if (buffered == 0 || !m_allowPartialReads) {
        size_t result;
        do {
            // Read enough to satisfy this request, plus up to a multiple of
            // the buffer size
            size_t todo = ((remaining - 1) / m_bufferSize + 1) * m_bufferSize;
            try {
                result = parent()->read(m_readBuffer, todo);
            } catch (...) {
                if (remaining == len) {
                    throw;
                } else {
                    // Swallow the exception
                    return len - remaining;
                }
            }

            buffered = std::min(m_readBuffer.readAvailable(), remaining);
            b.copyIn(m_readBuffer, buffered);
            m_readBuffer.consume(buffered);
            remaining -= buffered;
        } while (remaining > 0 && !m_allowPartialReads && result != 0);
    }

    return len - remaining;
}

size_t
BufferedStream::write(const Buffer &b, size_t len)
{
    ASSERT(!m_readBuffer.readAvailable() || !supportsSeek());
    m_writeBuffer.copyIn(b, len);
    size_t result = flushWrite(len);
    // Partial writes not allowed
    ASSERT(result == len);
    return result;
}

size_t
BufferedStream::write(const void *b, size_t len)
{
    ASSERT(!m_readBuffer.readAvailable() || !supportsSeek());
    m_writeBuffer.reserve(std::max(m_bufferSize, len));
    m_writeBuffer.copyIn(b, len);
    size_t result = flushWrite(len);
    // Partial writes not allowed
    ASSERT(result == len);
    return result;
}

size_t
BufferedStream::flushWrite(size_t len)
{
    while (m_writeBuffer.readAvailable() >= m_bufferSize)
    {
        size_t result;
        try {
            result = parent()->write(m_writeBuffer, m_writeBuffer.readAvailable());
        } catch (...) {
            // If this entire write is still in our buffer,
            // back it out and report the error
            if (m_writeBuffer.readAvailable() >= len) {
                Buffer tempBuffer;
                tempBuffer.copyIn(m_writeBuffer, m_writeBuffer.readAvailable() - len);
                m_writeBuffer.clear();
                m_writeBuffer.copyIn(tempBuffer);
                throw;
            } else {
                // Otherwise we have to say we succeeded,
                // because we're not allowed to have a partial
                // write, and we can't report an error because
                // the caller will think he needs to repeat
                // the entire write
                return len;
            }
        }
        m_writeBuffer.consume(result);
    }
    return len;
}

long long
BufferedStream::seek(long long offset, Anchor anchor)
{
    // My head asplodes!  Deal with it when someone actually does random access
    // read and writes;
    ASSERT(!(m_readBuffer.readAvailable() && m_writeBuffer.readAvailable()));
    if (offset == 0 && anchor == CURRENT) {
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
    ASSERT(!m_readBuffer.readAvailable() || !supportsSeek());
    flush();
    // TODO: truncate m_readBuffer at the end
    parent()->truncate(size);
}

void
BufferedStream::flush()
{
    while (m_writeBuffer.readAvailable()) {
        size_t result = parent()->write(m_writeBuffer, m_writeBuffer.readAvailable());
        ASSERT(result > 0);
        m_writeBuffer.consume(result);
    }
    parent()->flush();
}

size_t
BufferedStream::find(char delim, size_t sanitySize, bool throwIfNotFound)
{
    ASSERT(!m_writeBuffer.readAvailable() || !supportsSeek());
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
                throw BufferOverflowError();
            return ~0;
        }

        size_t result = parent()->read(m_readBuffer, m_bufferSize);
        if (result == 0) {
            // EOF
            if (throwIfNotFound)
                throw UnexpectedEofError();
            return ~0;
        }
    }
}

size_t
BufferedStream::find(const std::string &str, size_t sanitySize, bool throwIfNotFound)
{
    ASSERT(!m_writeBuffer.readAvailable() || !supportsSeek());
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
                throw BufferOverflowError();
            return ~0;
        }

        size_t result = parent()->read(m_readBuffer, m_bufferSize);
        if (result == 0) {
            // EOF
            if (throwIfNotFound)
                throw UnexpectedEofError();
            return ~0;
        }
    }
}

void
BufferedStream::unread(const Buffer &b, size_t len)
{
    ASSERT(!m_writeBuffer.readAvailable() || !supportsSeek());
    Buffer tempBuffer;
    tempBuffer.copyIn(b, len);
    tempBuffer.copyIn(m_readBuffer);
    m_readBuffer.clear();
    m_readBuffer.copyIn(tempBuffer);
}
