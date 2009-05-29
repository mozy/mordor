#ifndef __BUFFERED_STREAM_H__
#define __BUFFERED_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "filter.h"

class BufferedStream : public FilterStream
{
public:
    BufferedStream(Stream *parent, bool own = true);

    size_t bufferSize() { return m_bufferSize; }
    void bufferSize(size_t bufferSize) { m_bufferSize = bufferSize; }

    bool allowPartialReads() { return m_allowPartialReads; }
    void allowPartialReads(bool allowPartialReads) { m_allowPartialReads = allowPartialReads; }

    size_t read(Buffer *b, size_t len);
    size_t write(const Buffer *b, size_t len);
    size_t write(const void *b, size_t len);
    long long seek(long long offset, Anchor anchor);
    long long size();
    void truncate(long long size);
    void flush();
    size_t findDelimited(char delim);

    void unread(Buffer *b, size_t len);

private:
    size_t flushWrite(size_t len);

private:
    size_t m_bufferSize;
    bool m_allowPartialReads;
    Buffer m_readBuffer, m_writeBuffer;
};

#endif
