#ifndef __MORDOR_DEFLATE_STREAM_H__
#define __MORDOR_DEFLATE_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "zlib.h"

namespace Mordor {

class DeflateStream : public ZlibStream
{
public:
    DeflateStream(Stream::ptr parent, int level, int windowBits, int memlevel, Strategy strategy, bool own = true)
        : ZlibStream(parent, own, DEFLATE, level, windowBits, memlevel, strategy)
    {}

    DeflateStream(Stream::ptr parent, bool own = true)
        : ZlibStream(parent, own, DEFLATE)
    {}
};

}

#endif
