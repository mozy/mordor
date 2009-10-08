#ifndef __MORDOR_BUFFERED_STREAM_H__
#define __MORDOR_BUFFERED_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "filter.h"

namespace Mordor {

class BufferedStream : public FilterStream
{
public:
    typedef boost::shared_ptr<BufferedStream> ptr;

    BufferedStream(Stream::ptr parent, bool own = true);

    size_t bufferSize() { return m_bufferSize; }
    void bufferSize(size_t bufferSize) { m_bufferSize = bufferSize; }

    bool allowPartialReads() { return m_allowPartialReads; }
    void allowPartialReads(bool allowPartialReads) { m_allowPartialReads = allowPartialReads; }

    bool supportsFind() { return true; }
    bool supportsUnread() { return true; }

    void close(CloseType type = BOTH);
    size_t read(Buffer &b, size_t len);
    size_t write(const Buffer &b, size_t len);
    size_t write(const void *b, size_t len);
    long long seek(long long offset, Anchor anchor);
    long long size();
    void truncate(long long size);
    void flush();
    ptrdiff_t find(char delim, size_t sanitySize = ~0, bool throwIfNotFound = true);
    ptrdiff_t find(const std::string &str, size_t sanitySize = ~0, bool throwIfNotFound = true);
    void unread(const Buffer &b, size_t len = ~0);

private:
    size_t flushWrite(size_t len);

private:
    size_t m_bufferSize;
    bool m_allowPartialReads;
    Buffer m_readBuffer, m_writeBuffer;
};

}

#endif
