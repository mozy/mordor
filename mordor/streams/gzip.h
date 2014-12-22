#ifndef __MORDOR_GZIP_STREAM_H__
#define __MORDOR_GZIP_STREAM_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "zlib.h"

namespace Mordor {

class GzipStream : public ZlibStream
{
public:
    GzipStream(Stream::ptr parent, int level, int windowBits, int memlevel, Strategy strategy, bool own = true, bool invert = false)
        : ZlibStream(parent, own, GZIP, level, windowBits, memlevel, strategy, invert)
    {}

    GzipStream(Stream::ptr parent, bool own = true, bool invert = false)
        : ZlibStream(parent, own, GZIP, Z_DEFAULT_COMPRESSION, 15, 8, DEFAULT, invert)
    {}
};

}

#endif
