#ifndef __ZLIB_STREAM_H__
#define __ZLIB_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include <zlib.h>

#include "filter.h"

class ZlibStream : public MutatingFilterStream
{
public:
    enum Strategy {
        DEFAULT = Z_DEFAULT_STRATEGY,
        FILTERED = Z_FILTERED,
        HUFFMAN = Z_HUFFMAN_ONLY,
        FIXED = Z_FIXED,
        RLE = Z_RLE
    };

protected:
    enum Type {
        ZLIB,
        DEFLATE,
        GZIP
    };

    ZlibStream(Stream::ptr parent, bool own, Type type, int level = Z_DEFAULT_COMPRESSION,
        int windowBits = 15, int memlevel = 8, Strategy strategy = DEFAULT);

private:
    void init(Type type, int level = Z_DEFAULT_COMPRESSION, int windowBits = 15,
        int memlevel = 8, Strategy strategy = DEFAULT);

public:
    ZlibStream(Stream::ptr parent, int level, int windowBits, int memlevel, Strategy strategy,
        bool own = true);
    ZlibStream(Stream::ptr parent, bool own = true);
    ~ZlibStream();

    bool supportsSeek() const { return false; }
    bool supportsSize() const { return false; }
    bool supportsTruncate() const { return false; }

    void close(CloseType type = BOTH);
    size_t read(Buffer &b, size_t len);
    size_t write(const Buffer &b, size_t len);
    void flush();

private:
    void flush(int flush);
    void flushBuffer();

private:
    size_t m_bufferSize;
    Buffer m_inBuffer, m_outBuffer;
    z_stream m_strm;
    bool m_closed;
};

#endif
