#ifndef __MORDOR_NULL_STREAM_H__
#define __MORDOR_NULL_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "stream.h"

namespace Mordor {

class NullStream : public Stream
{
private:
    NullStream() {}

public:
    static NullStream &get() { return s_nullStream; }

    bool supportsRead() { return true; }
    bool supportsWrite() { return true; }
    bool supportsSize() { return true; }

    size_t read(Buffer &b, size_t len) { return 0; }
    size_t write(const Buffer &b, size_t len) { return len; }
    long long size() { return 0; }

private:
    static NullStream s_nullStream;
};

}

#endif
