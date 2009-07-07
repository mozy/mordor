#ifndef __MEMORY_STREAM_H__
#define __MEMORY_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "stream.h"

class MemoryStream : public Stream
{
public:
    MemoryStream();

    bool supportsRead() { return true; }
    bool supportsWrite() { return true; }
    bool supportsSeek() { return true; }
    bool supportsSize() { return true; }
    bool supportsTruncate() { return true; }
    bool supportsFind() { return true; }

    size_t read(Buffer &b, size_t len);
    size_t write(const Buffer &b, size_t len);
    size_t write(const void *b, size_t len);
    long long seek(long long offset, Anchor anchor);
    long long size();
    void truncate(long long size);
    size_t find(char delim);
    size_t find(const std::string &str, size_t sanitySize = ~0, bool throwIfNotFound = true);

    // Direct access to memory
    const Buffer &buffer() const { return m_original; }
    const Buffer &readBuffer() const { return m_read; }

private:
    Buffer m_read;
    Buffer m_original;
    size_t m_offset;

    template <class T> size_t writeInternal(T b, size_t len);
};

#endif
