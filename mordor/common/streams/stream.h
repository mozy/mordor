#ifndef __STREAM_H__
#define __STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include <cassert>
#include <string>

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include "buffer.h"

class Stream : boost::noncopyable
{
public:
    typedef boost::shared_ptr<Stream> ptr;

    enum CloseType {
        NONE  = 0x00,
        READ  = 0x01,
        WRITE = 0x02,
        BOTH  = 0x03
    };

    enum Anchor {
        BEGIN,
        CURRENT,
        END
    };

    virtual ~Stream() {}

    virtual bool supportsRead() { return false; }
    virtual bool supportsWrite() { return false; }
    virtual bool supportsSeek() { return false; }
    virtual bool supportsSize() { return false; }
    virtual bool supportsTruncate() { return false; }
    virtual bool supportsFind() { return false; }
    virtual bool supportsUnread() { return false; }

    virtual void close(CloseType type = BOTH) {}
    virtual size_t read(Buffer &b, size_t len) { assert(supportsRead()); return 0; }
    virtual size_t write(const Buffer &b, size_t len) { assert(supportsWrite()); return len; }
    virtual long long seek(long long offset, Anchor anchor) { assert(supportsSeek()); return 0ll; }
    virtual long long size() { assert(supportsSize()); return 0ll; }
    virtual void truncate(long long size) { assert(supportsTruncate()); }
    virtual void flush() {}
    virtual size_t find(char delim) { assert(false); return 0; }
    virtual size_t find(const std::string &str, size_t sanitySize = ~0, bool throwIfNotFound = true) { assert(false); return ~0; }
    virtual void unread(const Buffer &b, size_t len) { assert(false); }

    // Convenience functions - do *not* implement in FilterStream, so that
    // filters do not need to implement these
    virtual size_t write(const void *b, size_t len);
    size_t write(const char *sz);

    std::string getDelimited(char delim = '\n');
};

#endif
