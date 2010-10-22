// Copyright (c) 2010 - Decho Corporation

#include "singleplex.h"

#include "mordor/assert.h"

namespace Mordor {

SingleplexStream::SingleplexStream(Stream::ptr parent, Type type, bool own)
    : FilterStream(parent, own),
      m_type(type)
{
    MORDOR_ASSERT(type == READ || type == WRITE);
    if (type == READ) MORDOR_ASSERT(parent->supportsRead());
    if (type == WRITE) MORDOR_ASSERT(parent->supportsWrite());
}

void
SingleplexStream::close(CloseType type)
{
    if (ownsParent()) {
        if (m_type == READ && (type & Stream::READ)) {
            parent()->close(parent()->supportsHalfClose() ?
                Stream::READ : BOTH);
        } else if (m_type == WRITE && (type & Stream::WRITE)) {
            parent()->close(parent()->supportsHalfClose() ?
                Stream::WRITE : BOTH);
        }
    }
}

size_t
SingleplexStream::read(Buffer &buffer, size_t length)
{
    MORDOR_ASSERT(m_type == READ);
    return parent()->read(buffer, length);
}

size_t
SingleplexStream::read(void * buffer, size_t length)
{
    MORDOR_ASSERT(m_type == READ);
    return parent()->read(buffer, length);
}

size_t
SingleplexStream::write(const Buffer &buffer, size_t length)
{
    MORDOR_ASSERT(m_type == WRITE);
    return parent()->write(buffer, length);
}

size_t
SingleplexStream::write(const void *buffer, size_t length)
{
    MORDOR_ASSERT(m_type == WRITE);
    return parent()->write(buffer, length);
}

void
SingleplexStream::truncate(long long size)
{
    MORDOR_ASSERT(m_type == WRITE);
    return parent()->truncate(size);
}

void
SingleplexStream::flush(bool flushParent)
{
    if (m_type == READ)
        return;
    return parent()->flush(flushParent);
}

ptrdiff_t
SingleplexStream::find(char delimiter, size_t sanitySize,
        bool throwIfNotFound)
{
    MORDOR_ASSERT(m_type == READ);
    return parent()->find(delimiter, sanitySize, throwIfNotFound);
}

ptrdiff_t
SingleplexStream::find(const std::string &delimiter, size_t sanitySize,
    bool throwIfNotFound)
{
    MORDOR_ASSERT(m_type == READ);
    return parent()->find(delimiter, sanitySize, throwIfNotFound);
}

void
SingleplexStream::unread(const Buffer &buffer, size_t length)
{
    MORDOR_ASSERT(m_type == READ);
    return parent()->unread(buffer, length);
}

}
