#ifndef __MORDOR_ZERO_STREAM_H__
#define __MORDOR_ZERO_STREAM_H__
// Copyright (c) 2012 - VMware, Inc.

#include "stream.h"
#include "mordor/util.h"

class ZeroStream : public Mordor::Stream
{
private:
    ZeroStream() {}

public:
    static Stream::ptr get_ptr() {
        return Mordor::unmanagedPtr(s_zeroStream);
    }
    virtual bool supportsRead() { return true; }
    virtual size_t read(void *buffer, size_t length);
    using Mordor::Stream::read;

    virtual bool supportsSeek() { return true; }
    long long seek(long long offset, Anchor anchor = BEGIN);

private:
    static ZeroStream s_zeroStream;
};

#endif
