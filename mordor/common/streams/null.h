#ifndef __NULL_STREAM_H__
#define __NULL_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "stream.h"

class NullStream : public Stream
{
private:
    NullStream() {}

public:
    static NullStream &get() { return s_nullStream; }

    bool supportsRead() { return true; }
    bool supportsWrite() { return true; }

    size_t read(Buffer &b, size_t len) { return 0; }
    size_t write(const Buffer &b, size_t len) { return len; }

private:
    static NullStream s_nullStream;
};

#endif
