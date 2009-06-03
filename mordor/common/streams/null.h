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
    bool supportsSeek() { return true; }
    bool supportsSize() { return true; }
    bool supportsTruncate() { return true; }

private:
    static NullStream s_nullStream;
};

#endif
