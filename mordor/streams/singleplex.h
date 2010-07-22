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

    SingleplexStream(Stream::ptr parent, Type type, bool own = true);

    bool supportsRead() { return m_type == READ; }
    bool supportsWrite() { return m_type == WRITE; }
    bool supportsTruncate()
    { return m_type == WRITE && parent()->supportsTruncate(); }
    bool supportsFind()
    { return m_type == READ && parent()->supportsFind(); }
    bool supportsUnread()
    { return m_type == READ && parent()->supportsUnread(); }

    void close(CloseType type = BOTH);

    size_t read(Buffer &buffer, size_t length);
    size_t read(void *buffer, size_t length);
    size_t write(const Buffer &buffer, size_t length);
    size_t write(const void *buffer, size_t length);
    void truncate(long long size);
    void flush(bool flushParent = true);
    ptrdiff_t find(char delimiter, size_t sanitySize = ~0,
        bool throwIfNotFound = true);
    ptrdiff_t find(const std::string &delimiter,
        size_t sanitySize = ~0, bool throwIfNotFound = true);
    void unread(const Buffer &buffer, size_t length);

private:
    Type m_type;
};

}

#endif
