#ifndef __MORDOR_DUPLEX_STREAM_H__
#define __MORDOR_DUPLEX_STREAM_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "stream.h"

namespace Mordor {

// A DuplexStream combines a read and a write stream to form a stream that can
// be read and written to.  It *disables* seek(), size(), and truncate(),
// because the parent streams are disparate, and those concepts are supposed
// to be shared
class DuplexStream : public Stream
{
public:
    DuplexStream(Stream::ptr readParent, Stream::ptr writeParent,
        bool own = true)
        : m_readParent(readParent),
          m_writeParent(writeParent),
          m_own(own)
    {
        MORDOR_ASSERT(readParent);
        MORDOR_ASSERT(writeParent);
        MORDOR_ASSERT(readParent->supportsRead());
        MORDOR_ASSERT(writeParent->supportsWrite());
    }

    Stream::ptr readParent() { return m_readParent; }
    Stream::ptr writeParent() { return m_writeParent; }
    bool ownsParents() { return m_own; }

    bool supportsHalfClose() { return true; }
    bool supportsRead() { return true; }
    bool supportsWrite() { return true; }
    bool supportsFind() { return m_readParent && m_readParent->supportsFind(); }
    bool supportsUnread() { return m_readParent && m_readParent->supportsUnread(); }

    void close(CloseType type = BOTH)
    {
        if (m_own) {
            if ((type & READ) && m_readParent)
                m_readParent->close(m_readParent->supportsHalfClose() ? READ : BOTH);
            if ((type & WRITE) && m_writeParent)
                m_writeParent->close(m_writeParent->supportsHalfClose() ? WRITE : BOTH);
        }
        if (type & READ)
            m_readParent.reset();
        if (type & WRITE)
            m_writeParent.reset();
    }
    using Stream::read;
    size_t read(Buffer &b, size_t len)
    {
        if (!m_readParent) MORDOR_THROW_EXCEPTION(BrokenPipeException());
        return m_readParent->read(b, len);
    }
    using Stream::write;
    size_t write(const Buffer &b, size_t len)
    {
        if (!m_writeParent) MORDOR_THROW_EXCEPTION(BrokenPipeException());
        return m_writeParent->write(b, len);
    }
    void flush(bool flushParent = true)
    {
        if (!m_writeParent) MORDOR_THROW_EXCEPTION(BrokenPipeException());
        m_writeParent->flush();
    }
    using Stream::find;
    ptrdiff_t find(char delim)
    {
        if (!m_readParent) MORDOR_THROW_EXCEPTION(BrokenPipeException());
        return m_readParent->find(delim);
    }
    ptrdiff_t find(const std::string &str, size_t sanitySize = ~0, bool throwOnNotFound = true)
    {
        if (!m_readParent) MORDOR_THROW_EXCEPTION(BrokenPipeException());
        return m_readParent->find(str, sanitySize, throwOnNotFound);
    }
    void unread(const Buffer &b, size_t len) {
        if (!m_readParent) MORDOR_THROW_EXCEPTION(BrokenPipeException());
        return m_readParent->unread(b, len);
    }

protected:
    void ownsParents(bool own) { m_own = own; }

private:
    Stream::ptr m_readParent, m_writeParent;
    bool m_own;
};

}

#endif
