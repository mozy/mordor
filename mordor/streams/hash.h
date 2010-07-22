#ifndef __MORDOR_HASH_STREAM_H__
#define __MORDOR_HASH_STREAM_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <vector>

#include <openssl/sha.h>
#include <openssl/md5.h>

#include "assert.h"
#include "filter.h"

namespace Mordor {

class HashStream : public FilterStream
{
public:
    typedef boost::shared_ptr<HashStream> ptr;

public:
    HashStream(Stream::ptr parent, bool own = true)
        : FilterStream(parent, own)
    {}

    bool supportsSeek() { return false; }
    bool supportsTruncate() { return false; }
    bool supportsUnread() { return false; }

    size_t read(Buffer &buffer, size_t length);
    size_t read(void *buffer, size_t length);
    size_t write(const Buffer &buffer, size_t length);
    size_t write(const void *buffer, size_t length);
    long long seek(long long offset, Anchor anchor = BEGIN);

    // Returns the hash in *binary*
    virtual std::string hash() const = 0;

protected:
    virtual void updateHash(const Buffer &buffer, size_t length) = 0;
    virtual void updateHash(const void *buffer, size_t length) = 0;
};

class SHA0Stream : public HashStream
{
public:
    SHA0Stream(Stream::ptr parent, bool own = true);

    std::string hash() const;

protected:
    void updateHash(const Buffer &buffer, size_t length);
    void updateHash(const void *buffer, size_t length);

private:
    SHA_CTX m_ctx;
};

class SHA1Stream : public HashStream
{
public:
    SHA1Stream(Stream::ptr parent, bool own = true);

    std::string hash() const;

protected:
    void updateHash(const Buffer &buffer, size_t length);
    void updateHash(const void *buffer, size_t length);

private:
    SHA_CTX m_ctx;
};

class MD5Stream : public HashStream
{
public:
    MD5Stream(Stream::ptr parent, bool own = true);

    std::string hash() const;

protected:
    void updateHash(const Buffer &buffer, size_t length);
    void updateHash(const void *buffer, size_t length);

private:
    MD5_CTX m_ctx;
};

}

#endif
