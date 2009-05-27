#ifndef __STREAM_H__
#define __STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include <cassert>
#include <string>

#include <boost/noncopyable.hpp>

#include "buffer.h"

class Stream : boost::noncopyable
{
public:
    enum CloseType {
        READ,
        WRITE,
        BOTH
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

    virtual void close(CloseType type = BOTH) {}
    virtual size_t read(Buffer *b, size_t len) { assert(false); return 0; }
    virtual size_t write(const Buffer *b, size_t len) { assert(false); return 0; }
    virtual long long seek(long long offset, Anchor anchor) { assert(false); return 0ll; }
    virtual long long size() { assert(false); return 0ll; }
    virtual void truncate(long long size) { assert(false); }
    virtual void flush() {}
    virtual size_t findDelimited(char delim) { assert(false); return 0; }

    // Convenience functions - do *not* implement in FilterStream, so that
    // filters do not need to implement these
    virtual size_t write(const void *b, size_t len);
    size_t write(const char *sz);

    std::string getDelimited(char delim = '\n');
};

#endif
