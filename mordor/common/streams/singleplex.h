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

    SingleplexStream(Stream *parent, Type type, bool own = true)
        : FilterStream(parent, own),
          m_type(type)
    {
        assert(type == READ || type == WRITE);
        if (type == READ) assert(parent->supportsRead());
        if (type == WRITE) assert(parent->supportsWrite());
    }

    bool supportsRead() { return m_type == READ; }
    bool supportsWrite() { return m_type == WRITE; }
    bool supportsTruncate() { return m_type == WRITE && FilterStream::supportsTruncate(); }
    bool supportsFindDelimited() { return m_type == READ && FilterStream::supportsFindDelimited(); }

    size_t read(Buffer *b, size_t len)
    {
        assert(m_type == READ);
        return FilterStream::read(b, len);
    }
    size_t write(const Buffer *b, size_t len)
    {
        assert(m_type == WRITE);
        return FilterStream::write(b, len);
    }
    void truncate(long long size)
    {
        assert(m_type == WRITE);
        return FilterStream::truncate(size);
    }
    void flush()
    {
        if (m_type == READ)
            return;
        return FilterStream::flush();
    }
    size_t findDelimited(char delim)
    {
        assert(m_type == READ);
        return FilterStream::findDelimited(delim);
    }

private:
    Type m_type;
};

#endif
