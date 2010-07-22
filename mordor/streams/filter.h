#ifndef __MORDOR_FILTER_STREAM_H__
#define __MORDOR_FILTER_STREAM_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "stream.h"

namespace Mordor {

// When inheriting from FilterStream, use parent()->xxx to call
// method xxx on the parent stream.
// FilterStreams *must* implement one of the possible overloads for read()
// and write(), even if it's just calling the parent version.  Otherwise
// the adapter functions in the base Stream will just call each other
// and blow the stack
class FilterStream : public Stream
{
public:
    typedef boost::shared_ptr<FilterStream> ptr;

public:
    FilterStream(Stream::ptr parent, bool own = true)
        : m_parent(parent), m_own(own)
    {}

    Stream::ptr parent() { return m_parent; }
    void parent(Stream::ptr parent) { m_parent = parent; }
    bool ownsParent() { return m_own; }
    void ownsParent(bool own) { m_own = own; }

    bool supportsHalfClose() { return m_parent->supportsHalfClose(); }
    bool supportsRead() { return m_parent->supportsRead(); }
    bool supportsWrite() { return m_parent->supportsWrite(); }
    bool supportsSeek() { return m_parent->supportsSeek(); }
    bool supportsTell() { return m_parent->supportsTell(); }
    bool supportsSize() { return m_parent->supportsSize(); }
    bool supportsTruncate() { return m_parent->supportsTruncate(); }
    bool supportsFind() { return m_parent->supportsFind(); }
    bool supportsUnread() { return m_parent->supportsUnread(); }

    void close(CloseType type = BOTH) { if (m_own) m_parent->close(type); }
    void cancelRead() { m_parent->cancelRead(); }
    void cancelWrite() { m_parent->cancelWrite(); }
    long long seek(long long offset, Anchor anchor = BEGIN)
    { return m_parent->seek(offset, anchor); }
    long long size() { return m_parent->size(); }
    void truncate(long long size) { m_parent->truncate(size); }
    void flush(bool flushParent = true)
    { if (flushParent) m_parent->flush(true); }
    ptrdiff_t find(char delim, size_t sanitySize = ~0,
        bool throwIfNotFound = true)
    { return m_parent->find(delim, sanitySize, throwIfNotFound); }
    ptrdiff_t find(const std::string &str, size_t sanitySize = ~0,
        bool throwIfNotFound = true)
    { return m_parent->find(str, sanitySize, throwIfNotFound); }
    void unread(const Buffer &b, size_t len)
    { return m_parent->unread(b, len); }

private:
    Stream::ptr m_parent;
    bool m_own;
};

/// @details
/// A mutating filter stream is one that declares that it changes the data
/// as it flows through it.  It implicitly turns off and asserts features
/// that would need to be implemented by the inheritor, instead of defaulting
/// to the parent streams implementation.
class MutatingFilterStream : public FilterStream
{
protected:
    MutatingFilterStream(Stream::ptr parent, bool owns = true)
        : FilterStream(parent, owns)
    {}

public:
    bool supportsSeek() { return false; }
    bool supportsTell() { return supportsSeek(); }
    bool supportsSize() { return false; }
    bool supportsTruncate() { return false; }
    bool supportsFind() { return false; }
    bool supportsUnread() { return false; }

    long long seek(long long offset, Anchor anchor = BEGIN);
    long long size();
    void truncate(long long size);
    ptrdiff_t find(char delim, size_t sanitySize = ~0,
        bool throwIfNotFound = true);
    ptrdiff_t find(const std::string &str, size_t sanitySize = ~0,
        bool throwIfNotFound = true);
    void unread(const Buffer &b, size_t len);
};

}

#endif
