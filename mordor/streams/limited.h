#ifndef __MORDOR_LIMITED_STREAM_H__
#define __MORDOR_LIMITED_STREAM_H__
// Copyright (c) 2009 - Decho Corporation

#include "filter.h"

namespace Mordor {

class LimitedStream : public MutatingFilterStream
{
public:
    typedef boost::shared_ptr<LimitedStream> ptr;

public:
    LimitedStream(Stream::ptr parent, long long size, bool own = true);

    void reset() { m_pos = 0; }
    void reset(long long size) { m_pos = 0; m_size = size; }

    bool strict() { return m_strict; }
    void strict(bool strict) { m_strict = strict; }

    bool supportsSeek() { return parent()->supportsSeek(); }
    bool supportsTell() { return true; }
    bool supportsSize() { return true; }
    bool supportsTruncate() { return false; }

    size_t read(Buffer &b, size_t len);
    size_t write(const Buffer &b, size_t len);
    long long seek(long long offset, Anchor anchor = BEGIN);
    long long size();
    void truncate(long long size);
    void unread(const Buffer &b, size_t len);

private:
    long long m_pos, m_size;
    bool m_strict;
};

}

#endif
