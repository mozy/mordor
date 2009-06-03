// Copyright (c) 2009 - Decho Corp.

#include "limited.h"

LimitedStream::LimitedStream(Stream::ptr parent, long long size, bool own)
: FilterStream(parent, own),
  m_pos(0),
  m_size(size)
{
    assert(size >= 0);
}

size_t
LimitedStream::read(Buffer &b, size_t len)
{
    if (m_pos >= m_size)
        return 0;

    len = (size_t)std::min<long long>(len, m_size - m_pos);
    size_t result = FilterStream::read(b, len);
    m_pos += result;
    return result;
}

size_t
LimitedStream::write(const Buffer &b, size_t len)
{
    if (m_pos >= m_size)
        throw std::runtime_error("Write beyond EOF");
    len = (size_t)std::min<long long>(len, m_size - m_pos);
    size_t result = FilterStream::write(b, len);
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
    return m_pos = FilterStream::seek(offset, anchor);
}

long long
LimitedStream::size()
{
    if (!FilterStream::supportsSize()) {
        return m_size;
    }
    return std::min(m_size, FilterStream::size());
}
