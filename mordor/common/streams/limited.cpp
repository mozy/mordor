// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "limited.h"

#include <stdexcept>

LimitedStream::LimitedStream(Stream::ptr parent, long long size, bool own)
: MutatingFilterStream(parent, own),
  m_pos(0),
  m_size(size)
{
    ASSERT(size >= 0);
}

size_t
LimitedStream::read(Buffer &b, size_t len)
{
    if (m_pos >= m_size)
        return 0;

    len = (size_t)std::min<long long>(len, m_size - m_pos);
    size_t result = parent()->read(b, len);
    m_pos += result;
    return result;
}

size_t
LimitedStream::write(const Buffer &b, size_t len)
{
    if (m_pos >= m_size)
        throw std::runtime_error("Write beyond EOF");
    len = (size_t)std::min<long long>(len, m_size - m_pos);
    size_t result = parent()->write(b, len);
    m_pos += result;
    return result;
}

long long
LimitedStream::seek(long long offset, Anchor anchor)
{
    if (anchor == END) {
        offset += size();
        anchor = BEGIN;
    }
    return m_pos = parent()->seek(offset, anchor);
}

long long
LimitedStream::size()
{
    if (!parent()->supportsSize()) {
        return m_size;
    }
    try {
        return std::min(m_size, parent()->size());
    } catch (std::runtime_error) {
        return m_size;
    }
}


void
LimitedStream::unread(const Buffer &b, size_t len)
{
    parent()->unread(b, len);
    m_pos -= len;
}
