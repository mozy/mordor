// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "memory.h"

#include <stdexcept>

namespace Mordor {

MemoryStream::MemoryStream()
: m_offset(0)
{}

MemoryStream::MemoryStream(const Buffer &b)
: m_read(b),
  m_original(b),
  m_offset(0)
{}

size_t
MemoryStream::read(Buffer &buffer, size_t length)
{
    return readInternal(buffer, length);
}

size_t
MemoryStream::read(void *buffer, size_t length)
{
    return readInternal(buffer, length);
}

template <class T>
size_t
MemoryStream::readInternal(T &buffer, size_t length)
{
    size_t todo = std::min(length, m_read.readAvailable());
    m_read.copyOut(buffer, todo);
    m_read.consume(todo);
    m_offset += todo;
    return todo;
}

size_t
MemoryStream::write(const Buffer &buffer, size_t length)
{
    return writeInternal(buffer, length);
}

size_t
MemoryStream::write(const void *buffer, size_t length)
{
    return writeInternal(buffer, length);
}

template <class T>
size_t
MemoryStream::writeInternal(const T &buffer, size_t length)
{
    size_t size = m_original.readAvailable();
    if (m_offset == size) {
        m_original.copyIn(buffer, length);
        m_offset += length;
    } else if (m_offset > size) {
        // extend the stream, then write
        truncate(m_offset);
        m_original.copyIn(buffer, length);
        m_offset += length;
    } else {
        // write at offset
        Buffer original(m_original);
        // Truncate original to all data before the write
        m_original.truncate(m_offset);
        original.consume(m_offset);
        // copy in the write, advancing the stream pointer
        m_original.copyIn(buffer, length);
        m_offset += length;
        if (m_offset < size) {
            original.consume(length);
            // Copy in any remaining data beyond the write
            m_original.copyIn(original);
            // Reset our read buffer to the current stream pos
            m_read.clear();
            m_read.copyIn(original);
        }
    }
    return length;
}

long long
MemoryStream::seek(long long offset, Anchor anchor)
{
    size_t size = m_original.readAvailable();

    switch (anchor) {
        case BEGIN:
            if (offset < 0)
                MORDOR_THROW_EXCEPTION(std::invalid_argument("resulting offset is negative"));
            if ((unsigned long long)offset > (size_t)~0) {
                MORDOR_THROW_EXCEPTION(std::invalid_argument(
                    "Memory stream position cannot exceed virtual address space."));
            }
            m_read.clear();
            m_read.copyIn(m_original, m_original.readAvailable());
            m_offset = (size_t)offset;
            m_read.consume(std::min((size_t)offset, size));
            return (long long)m_offset;
        case CURRENT:
            if (offset < 0) {
                return seek(m_offset + offset, BEGIN);
            } else {
                // Optimized forward seek
                if (m_offset + offset > (size_t)~0) {
                    MORDOR_THROW_EXCEPTION(std::invalid_argument(
                        "Memory stream position cannot exceed virtual address space."));
                }
                if (m_offset <= size) {
                    m_read.consume(std::min((size_t)offset, size - m_offset));
                    m_offset += (size_t)offset;
                }
                return (long long)m_offset;
            }
        case END:
            // Change this into a CURRENT to try and catch an optimized forward
            // seek
            return seek(size + offset - m_offset, CURRENT);
        default:
            MORDOR_ASSERT(false);
            return 0;
    }
}

long long
MemoryStream::size()
{
    return (long long)m_original.readAvailable();
}

void
MemoryStream::truncate(long long size)
{
    MORDOR_ASSERT(size >= 0);
    if ((unsigned long long)size > (size_t)~0) {
        MORDOR_THROW_EXCEPTION(std::invalid_argument(
            "Memory stream size cannot exceed virtual address space."));
    }
    size_t currentSize = m_original.readAvailable();

    if (currentSize == (size_t)size) {
    } else if (currentSize > (size_t)size) {
        m_original.truncate((size_t)size);
        if (m_offset > (size_t)size)
            m_read.clear();
        else
            m_read.truncate((size_t)size - m_offset);
    } else {
        size_t needed = (size_t)size - currentSize;
        m_original.reserve(needed);
        std::vector<iovec> iovs = m_original.writeBufs(needed);
        for (std::vector<iovec>::iterator it(iovs.begin());
            it != iovs.end();
            ++it) {
            memset(it->iov_base, 0, it->iov_len);
        }
        m_original.produce(needed);
        // Reset the read buf so we're referencing the same memory
        m_read.clear();
        m_read.copyIn(m_original);
        m_read.consume(std::min(m_offset, (size_t)size));
    }

    MORDOR_ASSERT(m_original.readAvailable() == (size_t)size);
}

ptrdiff_t
MemoryStream::find(char delim, size_t sanitySize, bool throwIfNotFound)
{
    ptrdiff_t result = m_read.find(delim, std::min(sanitySize, m_read.readAvailable()));
    if (result != -1)
        return result;
    if (throwIfNotFound)
        MORDOR_THROW_EXCEPTION(UnexpectedEofException());
    return -(ptrdiff_t)m_read.readAvailable() - 1;
}

ptrdiff_t
MemoryStream::find(const std::string &str, size_t sanitySize, bool throwIfNotFound)
{
    ptrdiff_t result = m_read.find(str, std::min(sanitySize, m_read.readAvailable()));
    if (result != -1)
        return result;
    if (throwIfNotFound)
        MORDOR_THROW_EXCEPTION(UnexpectedEofException());
    return -(ptrdiff_t)m_read.readAvailable() - 1;
}

}
