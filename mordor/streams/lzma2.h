#ifndef __MORDOR_LZMA_STREAM_H__
#define __MORDOR_LZMA_STREAM_H__
// Copyright (c) 2012 - VMware, Inc.

#ifdef MSVC
#define LZMA_API_STATIC // static-link with liblzma
#endif

#include <lzma.h>

#include "buffer.h"
#include "filter.h"

namespace Mordor {

struct LZMAException : virtual Exception
{
    LZMAException(lzma_ret rc) : m_rc(rc) {}
    lzma_ret rc() const {
        return m_rc;
    }
private:
    lzma_ret m_rc;
};

struct UnknownLZMAFormatException : LZMAException
{
    UnknownLZMAFormatException() : LZMAException(LZMA_FORMAT_ERROR)
    {}
};

struct CorruptedLZMAStreamException : LZMAException
{
    CorruptedLZMAStreamException() : LZMAException(LZMA_DATA_ERROR)
    {}
};

struct UnsupportedOptionsException : LZMAException
{
    UnsupportedOptionsException() : LZMAException(LZMA_OPTIONS_ERROR)
    {}
};

/// Stream for compressing or decompressing streams using the lzma2 algorithm
///
/// Lzma2 is an extension on top of the original lzma but features support for
/// some advanced features, flushing the encoder and improves support for
/// multithreading, among other things.
/// Although lzma2 encoder can be highly customized, we just hardwire two filters,
/// LZMA_FILTER_${ARCH} and LZMA_FILTER_LZMA2, into the encoder in hope that it
/// can be optimal under most circumstances.
/// @note lzma2 is @em not compatible with plain lzma format.
/// As a side note, XZ is a container format of one or more lzma2 streams.
class LZMAStream : public MutatingFilterStream
{
public:
    LZMAStream(Stream::ptr parent, uint32_t preset = LZMA_PRESET_DEFAULT,
             lzma_check check = LZMA_CHECK_CRC64, bool own = true);
    ~LZMAStream();
    void close(CloseType type = BOTH);
    using MutatingFilterStream::read;
    size_t read(Buffer &b, size_t len);
    using MutatingFilterStream::write;
    size_t write(const Buffer &b, size_t len);
    void flush(bool flushParent = true);

private:
    void flushBuffer();
    void finish();

private:
    static const size_t BUFFER_SIZE = 64 * 1024;
    lzma_stream m_strm;
    Buffer m_inBuffer, m_outBuffer; // only used when compressing
    bool m_closed;
};

}

#endif
