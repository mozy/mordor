// Copyright (c) 2009 - Decho Corp.

#include "buffered.h"

BufferedStream::BufferedStream(Stream::ptr parent, bool own)
: FilterStream(parent, own)
{
    m_bufferSize = 65536;
    m_allowPartialReads = false;
}

size_t
BufferedStream::read(Buffer *b, size_t len)
{
    size_t remaining = len;

    size_t buffered = std::min(m_readBuffer.readAvailable(), remaining);
    b->copyIn(m_readBuffer, buffered);
    m_readBuffer.consume(buffered);
    remaining -= buffered;

    if (remaining == 0) {
        return len;
    }

    if (buffered == 0 || !m_allowPartialReads) {
        do {
            // Read enough to satisfy this request, plus up to a multiple of
            // the buffer size
            size_t todo = ((remaining - 1) / m_bufferSize + 1) * m_bufferSize;
            size_t result;
            try {
                result = FilterStream::read(&m_readBuffer, todo);
            } catch (...) {
                if (remaining == len) {
                    throw;
                } else {
                    // Swallow the exception
                    return len - remaining;
                }
            }

            buffered = std::min(m_readBuffer.readAvailable(), remaining);
            b->copyIn(m_readBuffer, buffered);
            m_readBuffer.consume(buffered);
            remaining -= buffered;
        } while (remaining > 0 && !m_allowPartialReads);
    }

    return len - remaining;
}

size_t
BufferedStream::write(const Buffer *b, size_t len)
{
    m_writeBuffer.copyIn(*b, len);
    size_t result = flushWrite(len);
    // Partial writes not allowed
    assert(result == len);
    return result;
}

size_t
BufferedStream::write(const void *b, size_t len)
{
    m_writeBuffer.reserve(std::max(m_bufferSize, len));
    m_writeBuffer.copyIn(b, len);
    size_t result = flushWrite(len);
    // Partial writes not allowed
    assert(result == len);
    return result;
}

size_t
BufferedStream::flushWrite(size_t len)
{
    while (m_writeBuffer.readAvailable() >= m_bufferSize)
    {
        size_t result;
        try {
            result = FilterStream::write(&m_writeBuffer, m_writeBuffer.readAvailable());
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
    flush();
    // TODO: optimized forward seek
    if (anchor == CURRENT) {
        // adjust the buffer having modified the actual stream position
        offset -= m_readBuffer.readAvailable();
    }
    m_readBuffer.clear();
    return FilterStream::seek(offset, anchor);
}

long long
BufferedStream::size()
{
    long long size = FilterStream::size();
    if (supportsSeek()) {
        long long pos;
        try {
            pos = seek(0, CURRENT);
        } catch(...) {
            return size + m_writeBuffer.readAvailable();
        }
        size = std::max(pos + (long long)m_writeBuffer.readAvailable(), size);
    } else {
        // not a seekable stream; we can only write to the end
        size += m_writeBuffer.readAvailable();
    }
    return size;
}

void
BufferedStream::truncate(long long size)
{
    flush();
    // TODO: truncate m_readBuffer at the end
    FilterStream::truncate(size);
}

void
BufferedStream::flush()
{
    while (m_writeBuffer.readAvailable()) {
        size_t result = FilterStream::write(&m_writeBuffer, m_writeBuffer.readAvailable());
        assert(result > 0);
        m_writeBuffer.consume(result);
    }
    FilterStream::flush();
}

size_t
BufferedStream::findDelimited(char delim)
{
    const size_t sanitySize = 65536;
    while (true) {
        size_t readAvailable = m_readBuffer.readAvailable();
        if (readAvailable >= sanitySize) {
            throw std::runtime_error("Buffer overflow!");
        }
        if (readAvailable > 0) {
            ptrdiff_t result = m_readBuffer.findDelimited(delim);
            if (result != -1) {
                return result;
            }
        }

        size_t result = FilterStream::read(&m_readBuffer, m_bufferSize);
        if (result == 0) {
            // EOF
            throw std::runtime_error("Unexpected EOF");
        }
    }
}

void
BufferedStream::unread(Buffer *b, size_t len)
{
    Buffer tempBuffer;
    tempBuffer.copyIn(*b, len);
    tempBuffer.copyIn(m_readBuffer);
    m_readBuffer.clear();
    m_readBuffer.copyIn(tempBuffer);
}
