#ifndef __MORDOR_SINGLEPLEX_STREAM_H__
#define __MORDOR_SINGLEPLEX_STREAM_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "filter.h"

namespace Mordor {

class SingleplexStream : public FilterStream
{
public:
    enum Type {
        READ,
        WRITE
    };

    SingleplexStream(Stream::ptr parent, Type type, bool own = true)
        : FilterStream(parent, own),
          m_type(type)
    {
        MORDOR_ASSERT(type == READ || type == WRITE);
        if (type == READ) MORDOR_ASSERT(parent->supportsRead());
        if (type == WRITE) MORDOR_ASSERT(parent->supportsWrite());
    }

    bool supportsRead() { return m_type == READ; }
    bool supportsWrite() { return m_type == WRITE; }
    bool supportsTruncate() { return m_type == WRITE && parent()->supportsTruncate(); }
    bool supportsFind() { return m_type == READ && parent()->supportsFind(); }
    bool supportsUnread() { return m_type == READ && parent()->supportsUnread(); }

    void close(CloseType type = BOTH)
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

    size_t read(Buffer &b, size_t len)
    {
        MORDOR_ASSERT(m_type == READ);
        return parent()->read(b, len);
    }
    size_t write(const Buffer &b, size_t len)
    {
        MORDOR_ASSERT(m_type == WRITE);
        return parent()->write(b, len);
    }
    void truncate(long long size)
    {
        MORDOR_ASSERT(m_type == WRITE);
        return parent()->truncate(size);
    }
    void flush(bool flushParent = true)
    {
        if (m_type == READ)
            return;
        return parent()->flush(flushParent);
    }
    ptrdiff_t find(char delim)
    {
        MORDOR_ASSERT(m_type == READ);
        return parent()->find(delim);
    }
    ptrdiff_t find(const std::string &str, size_t sanitySize = ~0,
        bool throwIfNotFound = true)
    {
        MORDOR_ASSERT(m_type == READ);
        return parent()->find(str, sanitySize, throwIfNotFound);
    }
    void unread(const Buffer &b, size_t len)
    {
        MORDOR_ASSERT(m_type == READ);
        return parent()->unread(b, len);
    }

private:
    Type m_type;
};

}

#endif
