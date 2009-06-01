#ifndef __FILTER_STREAM_H__
#define __FILTER_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "stream.h"

class FilterStream : public Stream
{
protected:
    FilterStream(Stream *parent, bool own = true)
        : m_parent(parent), m_own(own)
    {}
public:
    ~FilterStream()
    {
        if (m_own) {
            delete m_parent;
        }
    }

    Stream *parent() { return m_parent; }
    bool ownsParent() { return m_own; }

    bool supportsRead() { return m_parent->supportsRead(); }
    bool supportsWrite() { return m_parent->supportsWrite(); }
    bool supportsSeek() { return m_parent->supportsSeek(); }
    bool supportsSize() { return m_parent->supportsSize(); }
    bool supportsTruncate() { return m_parent->supportsTruncate(); }
    bool supportsFindDelimited() { return m_parent->supportsFindDelimited(); }

    void close(CloseType type = BOTH)
    {
        if (m_own) {
            m_parent->close(type);
        }
    }
    size_t read(Buffer *b, size_t len) { return m_parent->read(b, len); }
    size_t write(const Buffer *b, size_t len) { return m_parent->write(b, len); }
    long long seek(long long offset, Anchor anchor) { return m_parent->seek(offset, anchor); }
    long long size() { return m_parent->size(); }
    void truncate(long long size) { m_parent->truncate(size); }
    void flush() { m_parent->flush(); }
    size_t findDelimited(char delim) { return m_parent->findDelimited(delim); }

protected:
    void parent(Stream *parent) { m_parent = parent; }
    void ownsParent(bool own) { m_own = own; }

private:
    Stream *m_parent;
    bool m_own;
};

class MutatingFilterStream : public FilterStream
{
protected:
    MutatingFilterStream(Stream *parent, bool owns = true)
        : FilterStream(parent, owns)
    {}

    bool supportsFindDelimited() { return false; }

public:
    size_t findDelimited(char delim) { assert(false); return 0; }
};

#endif
