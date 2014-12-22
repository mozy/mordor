#ifndef __MORDOR_ZLIB_STREAM_H__
#define __MORDOR_ZLIB_STREAM_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <zlib.h>

#include "buffer.h"
#include "filter.h"
#include "mordor/exception.h"

namespace Mordor {

struct ZlibException : virtual Exception
{
public:
    ZlibException() : m_rc(0) {}
    ZlibException(int rc) :
      m_rc(rc)
    {}

    int rc() const { return m_rc; }

private:
    int m_rc;
};

struct NeedPresetDictionaryException : virtual ZlibException
{
public:
    NeedPresetDictionaryException() : ZlibException(Z_NEED_DICT)
    {}
};

struct CorruptedZlibStreamException : public ZlibException
{
public:
    CorruptedZlibStreamException() : ZlibException(Z_DATA_ERROR)
    {}
};

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
    // Inverse means compress on read, decompress on write;
    // Non-inverse means decompress on read, compress on write, the default way.
    ZlibStream(Stream::ptr parent, bool own, Type type, int level = Z_DEFAULT_COMPRESSION,
        int windowBits = 15, int memlevel = 8, Strategy strategy = DEFAULT, bool invert = false);

private:
    void init(Type type, int level = Z_DEFAULT_COMPRESSION, int windowBits = 15,
        int memlevel = 8, Strategy strategy = DEFAULT, bool invert = false);

public:
    ZlibStream(Stream::ptr parent, int level, int windowBits, int memlevel, Strategy strategy,
        bool own = true, bool invert = false);
    ZlibStream(Stream::ptr parent, bool own = true, bool invert = false);
    ~ZlibStream();

    void reset();

    bool supportsSeek() { return false; }
    bool supportsSize() { return false; }
    bool supportsTruncate() { return false; }

    void close(CloseType type = BOTH);
    using MutatingFilterStream::read;
    size_t read(Buffer &b, size_t len);
    using MutatingFilterStream::write;
    size_t write(const Buffer &b, size_t len);
    void flush(bool flushParent = true);

private:
    void flush(int flush);
    void flushBuffer();
    size_t doInflateForRead(Buffer &b, size_t len);
    size_t doDeflateForRead(Buffer &b, size_t len);
    size_t doInflateForWrite(const Buffer &b, size_t len);
    size_t doDeflateForWrite(const Buffer &b, size_t len);

private:
    static const size_t m_bufferSize = 64 * 1024;
    int m_level, m_windowBits, m_memlevel;
    Strategy m_strategy;
    Buffer m_inBuffer, m_outBuffer;
    z_stream m_strm;
    bool m_closed;
    bool m_doInflate;  //m_doInflate determines the stream to do inflate or deflate.
};

}

#endif
