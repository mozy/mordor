#ifndef __HASH_STREAM_H__
#define __HASH_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include <vector>

#include <openssl/sha.h>
#include <openssl/md5.h>

#include "assert.h"
#include "filter.h"

class HashStream : public FilterStream
{
public:
    HashStream(Stream::ptr parent, bool own = true)
        : FilterStream(parent, own)
    {}

    bool supportsSeek() { return false; }
    bool supportsTruncate() { return false; }
    bool supportsUnread() { return false; }

    size_t read(Buffer &b, size_t len);
    size_t write(const Buffer &b, size_t len);
    long long seek(long long offset, Anchor anchor) { NOTREACHED(); }

    // Returns the hash in *binary*
    virtual std::string hash() const = 0;

protected:
    virtual void updateHash(const Buffer &b, size_t len) = 0;
};

class SHA0Stream : public HashStream
{
public:
    SHA0Stream(Stream::ptr parent, bool own = true);

    std::string hash() const;

protected:
    void updateHash(const Buffer &b, size_t len);

private:
    SHA_CTX m_ctx;
};

class SHA1Stream : public HashStream
{
public:
    SHA1Stream(Stream::ptr parent, bool own = true);

    std::string hash() const;

protected:
    void updateHash(const Buffer &b, size_t len);

private:
    SHA_CTX m_ctx;
};

class MD5Stream : public HashStream
{
public:
    MD5Stream(Stream::ptr parent, bool own = true);

    std::string hash() const;

protected:
    void updateHash(const Buffer &b, size_t len);

private:
    MD5_CTX m_ctx;
};

#endif
