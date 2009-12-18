// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "cat.h"

namespace Mordor {

CatStream::CatStream(const std::vector<Stream::ptr> &streams)
: m_streams(streams),
  m_seekable(true),
  m_size(0ll),
  m_pos(0ll)
{
    m_it = m_streams.begin();
    for (std::vector<Stream::ptr>::iterator it = m_streams.begin();
        it != m_streams.end();
        ++it) {
        if (!(*it)->supportsSeek()) {
            m_seekable = false;
            if (m_size == -1ll)
                break;
        }
        if (!(*it)->supportsSize()) {
            m_seekable = false;
            m_size = -1ll;
            break;
        } else {
            m_size += (*it)->size();
        }
    }
}

size_t
CatStream::read(Buffer &buffer, size_t length)
{
    MORDOR_ASSERT(length != 0);
    if (m_it == m_streams.end())
        return 0;
    while (true) {
        size_t result = (*m_it)->read(buffer, length);
        m_pos += result;
        if (result == 0) {
            if (++m_it == m_streams.end())
                return 0;
            if ((*m_it)->supportsSeek())
                (*m_it)->seek(0);
            continue;
        }
        return result;
    }
}

long long
CatStream::seek(long long offset, Anchor anchor)
{
    if (offset == 0 && anchor == CURRENT)
        return m_pos;
    MORDOR_ASSERT(m_seekable);
    std::vector<Stream::ptr>::iterator it = m_it;
    long long itOffset = 0;
    switch (anchor) {
        case BEGIN:
            it = m_streams.begin();
            if (it != m_streams.end())
                itOffset = (*it)->tell();
            break;
        case CURRENT:
            break;
        case END:
            it = m_streams.end();
            break;
        default:
            MORDOR_NOTREACHED();
    }
    long long pos = m_pos;
    while (offset != 0) {
        if (offset < 0) {
            if (itOffset == 0) {
                if (it == m_streams.begin())
                    throw std::invalid_argument("Can't seek below 0");
                --it;
                itOffset = (*it)->size();
            }
            long long toChange = std::min(-offset, itOffset);
            itOffset -= toChange;
            pos -= toChange;
            offset += toChange;
        } else {
            long long toChange = offset;
            if (it != m_streams.end())
                toChange = std::min(offset, (*it)->size() - itOffset);
            itOffset += toChange;
            pos += toChange;
            offset -= toChange;
            if (it != m_streams.end() && itOffset == (*it)->size()) {
                ++it;
                itOffset = 0;
            }
        }
    }
    if (it != m_streams.end() && it != m_it)
        (*it)->seek(itOffset);
    m_it = it;
    if (it == m_streams.end())
        pos = m_size + itOffset;
    return m_pos = pos;
}

long long
CatStream::size()
{
    MORDOR_ASSERT(m_size != -1ll);
    return m_size;
}

}
