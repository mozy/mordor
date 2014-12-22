#ifndef __MORDOR_DEFLATE_STREAM_H__
#define __MORDOR_DEFLATE_STREAM_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "zlib.h"

namespace Mordor {

class DeflateStream : public ZlibStream
{
public:
    DeflateStream(Stream::ptr parent, int level, int windowBits, int memlevel, Strategy strategy, bool own = true, bool invert = false)
        : ZlibStream(parent, own, DEFLATE, level, windowBits, memlevel, strategy, invert)
    {}

    DeflateStream(Stream::ptr parent, bool own = true, bool invert = false)
        : ZlibStream(parent, own, DEFLATE, Z_DEFAULT_COMPRESSION, 15, 8, DEFAULT, invert)
    {}
};

}

#endif
