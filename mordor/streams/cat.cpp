// Copyright (c) 2009 - Mozy, Inc.

#include "cat.h"

#include "mordor/assert.h"

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
    long long newPos = 0;
    switch (anchor) {
        case BEGIN:
            newPos = offset;
            break;
        case CURRENT:
            newPos = m_pos + offset;
            break;
        case END:
            newPos = m_size + offset;
            break;
        default:
            MORDOR_NOTREACHED();
    }
    if (newPos < 0) {
        MORDOR_THROW_EXCEPTION(std::invalid_argument("Can't seek below 0"));
    } else if (newPos >= m_size) {
        MORDOR_THROW_EXCEPTION(std::invalid_argument("Can't seek above max size"));
    }

    long long cursor = 0;
    for (std::vector<Stream::ptr>::iterator it = m_streams.begin();
        it != m_streams.end();
        ++it) {
        long long size = (*it)->size();
        if (newPos < cursor + size) {
            m_it = it;
            (*m_it)->seek(newPos - cursor);
            break;
        }
        cursor += size;
    }
    m_pos = newPos;
    return m_pos;
}

long long
CatStream::size()
{
    MORDOR_ASSERT(m_size != -1ll);
    return m_size;
}

}
