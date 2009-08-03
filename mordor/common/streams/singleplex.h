#ifndef __SINGLEPLEX_STREAM_H__
#define __SINGLEPLEX_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "filter.h"

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
        ASSERT(type == READ || type == WRITE);
        if (type == READ) ASSERT(parent->supportsRead());
        if (type == WRITE) ASSERT(parent->supportsWrite());
    }

    bool supportsRead() { return m_type == READ; }
    bool supportsWrite() { return m_type == WRITE; }
    bool supportsTruncate() { return m_type == WRITE && FilterStream::supportsTruncate(); }
    bool supportsFind() { return m_type == READ && FilterStream::supportsFind(); }
    bool supportsUnread() { return m_type == READ && FilterStream::supportsUnread(); }

    void close(CloseType type = BOTH)
    {
        if (m_type == READ && type & Stream::READ) {
            FilterStream::close(Stream::READ);
        } else if (m_type == WRITE && type & Stream::WRITE) {
            FilterStream::close(Stream::WRITE);
        }
    }

    size_t read(Buffer &b, size_t len)
    {
        ASSERT(m_type == READ);
        return FilterStream::read(b, len);
    }
    size_t write(const Buffer &b, size_t len)
    {
        ASSERT(m_type == WRITE);
        return FilterStream::write(b, len);
    }
    void truncate(long long size)
    {
        ASSERT(m_type == WRITE);
        return FilterStream::truncate(size);
    }
    void flush()
    {
        if (m_type == READ)
            return;
        return FilterStream::flush();
    }
    size_t find(char delim)
    {
        ASSERT(m_type == READ);
        return FilterStream::find(delim);
    }
    size_t find(const std::string &str, size_t sanitySize = ~0, bool throwIfNotFound = true)
    {
        ASSERT(m_type == READ);
        return FilterStream::find(str, sanitySize, throwIfNotFound);
    }
    void unread(const Buffer &b, size_t len)
    {
        ASSERT(m_type == READ);
        return FilterStream::unread(b, len);
    }

private:
    Type m_type;
};

#endif
