#ifndef __MORDOR_LIMITED_STREAM_H__
#define __MORDOR_LIMITED_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "filter.h"

namespace Mordor {

class LimitedStream : public MutatingFilterStream
{
public:
    LimitedStream(Stream::ptr parent, long long size, bool own = true);

    bool supportsTell() { return true; }
    bool supportsSize() { return true; }
    bool supportsTruncate() { return false; }

    size_t read(Buffer &b, size_t len);
    size_t write(const Buffer &b, size_t len);
    long long seek(long long offset, Anchor anchor = BEGIN);
    long long size();
    void truncate(long long size) { MORDOR_NOTREACHED(); }
    void unread(const Buffer &b, size_t len);

private:
    long long m_pos, m_size;
};

}

#endif
