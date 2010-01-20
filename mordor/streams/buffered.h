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

    bool flushMultiplesOfBuffer() { return m_flushMultiplesOfBuffer; }
    void flushMultiplesOfBuffer(bool flushMultiplesOfBuffer ) { m_flushMultiplesOfBuffer = flushMultiplesOfBuffer; }

    bool supportsFind() { return supportsRead(); }
    bool supportsUnread() { return supportsRead() && (!supportsWrite() || !supportsSeek()); }

    void close(CloseType type = BOTH);
    size_t read(Buffer &buffer, size_t length);
    size_t read(void *buffer, size_t length);
    size_t write(const Buffer &buffer, size_t length);
    size_t write(const void *buffer, size_t length);
    long long seek(long long offset, Anchor anchor = BEGIN);
    long long size();
    void truncate(long long size);
    void flush(bool flushParent = true);
    ptrdiff_t find(char delim, size_t sanitySize = ~0, bool throwIfNotFound = true);
    ptrdiff_t find(const std::string &str, size_t sanitySize = ~0, bool throwIfNotFound = true);
    void unread(const Buffer &b, size_t len = ~0);

private:
    template <class T> size_t readInternal(T &buffer, size_t length);
    size_t flushWrite(size_t length);

private:
    size_t m_bufferSize;
    bool m_allowPartialReads, m_flushMultiplesOfBuffer;
    Buffer m_readBuffer, m_writeBuffer;
};

}

#endif
