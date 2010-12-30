#ifndef __MORDOR_NULL_STREAM_H__
#define __MORDOR_NULL_STREAM_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "stream.h"
#include "mordor/util.h"

namespace Mordor {

class NullStream : public Stream
{
private:
    NullStream() {}

public:
    static NullStream &get() { return s_nullStream; }
    static Stream::ptr get_ptr() { return Stream::ptr(&s_nullStream, &nop<Stream *>); }

    bool supportsRead() { return true; }
    bool supportsWrite() { return true; }
    bool supportsSeek() { return true; }
    bool supportsSize() { return true; }

    size_t read(Buffer &buffer, size_t length) { return 0; }
    size_t read(void *buffer, size_t length) { return 0; }
    size_t write(const Buffer &buffer, size_t length) { return length; }
    size_t write(const void *buffer, size_t length) { return length; }
    long long seek(long long offset, Anchor anchor = BEGIN) { return 0; }
    long long size() { return 0; }

private:
    static NullStream s_nullStream;
};

}

#endif
