#ifndef __MORDOR_GZIP_STREAM_H__
#define __MORDOR_GZIP_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "zlib.h"

namespace Mordor {

class GzipStream : public ZlibStream
{
public:
    GzipStream(Stream::ptr parent, int level, int windowBits, int memlevel, Strategy strategy, bool own = true)
        : ZlibStream(parent, own, GZIP, level, windowBits, memlevel, strategy)
    {}

    GzipStream(Stream::ptr parent, bool own = true)
        : ZlibStream(parent, own, GZIP)
    {}
};

}

#endif
