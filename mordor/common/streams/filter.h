#ifndef __FILTER_STREAM_H__
#define __FILTER_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "stream.h"

// When inheriting from FilterStream, use parent()->xxx to call
// method xxx on the parent stream.
class FilterStream : public Stream
{
public:
    typedef boost::shared_ptr<FilterStream> ptr;

public:
    FilterStream(Stream::ptr parent, bool own = true)
        : m_parent(parent), m_own(own)
    {
        ASSERT(parent);
    }

    Stream::ptr parent() { return m_parent; }
    bool ownsParent() { return m_own; }

    bool supportsRead() { return m_parent->supportsRead(); }
    bool supportsWrite() { return m_parent->supportsWrite(); }
    bool supportsSeek() { return m_parent->supportsSeek(); }
    bool supportsSize() { return m_parent->supportsSize(); }
    bool supportsTruncate() { return m_parent->supportsTruncate(); }
    bool supportsFind() { return m_parent->supportsFind(); }
    bool supportsUnread() { return m_parent->supportsUnread(); }

    void close(CloseType type = BOTH)
    {
        if (m_own) {
            m_parent->close(type);
        }
    }
    size_t read(Buffer &b, size_t len) { return m_parent->read(b, len); }
    size_t write(const Buffer &b, size_t len) { return m_parent->write(b, len); }
    long long seek(long long offset, Anchor anchor) { return m_parent->seek(offset, anchor); }
    long long size() { return m_parent->size(); }
    void truncate(long long size) { m_parent->truncate(size); }
    void flush() { m_parent->flush(); }
    size_t find(char delim, size_t sanitySize = ~0, bool throwIfNotFound = true)
    { return m_parent->find(delim, sanitySize, throwIfNotFound); }
    size_t find(const std::string &str, size_t sanitySize = ~0, bool throwIfNotFound = true)
    { return m_parent->find(str, sanitySize, throwIfNotFound); }
    void unread(const Buffer &b, size_t len) { return m_parent->unread(b, len); }

protected:
    void parent(Stream::ptr parent) { m_parent = parent; }
    void ownsParent(bool own) { m_own = own; }

private:
    Stream::ptr m_parent;
    bool m_own;
};

// A mutating filter stream is one that declares that it changes the data
// as it flows through it.  It implicitly turns off and asserts features
// that would need to be implemented by the inheritor, instead of defaulting
// to the parent streams implementation.
class MutatingFilterStream : public FilterStream
{
protected:
    MutatingFilterStream(Stream::ptr parent, bool owns = true)
        : FilterStream(parent, owns)
    {}

    bool supportsSeek() { return false; }
    bool supportsSize() { return false; }
    bool supportsTruncate() { return false; }
    bool supportsFind() { return false; }
    bool supportsUnread() { return false; }

public:
    long long seek(long long offset, Anchor anchor) { NOTREACHED(); }
    long long size() { NOTREACHED(); }
    void truncate(long long size) { NOTREACHED(); }
    size_t find(char delim, size_t sanitySize = ~0, bool throwIfNotFound = true) { NOTREACHED(); }
    size_t find(const std::string &str, size_t sanitySize = ~0, bool throwIfNotFound = true) { NOTREACHED(); }
    void unread(const Buffer &b, size_t len) { NOTREACHED(); }
};

#endif
