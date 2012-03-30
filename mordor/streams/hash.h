#ifndef __MORDOR_HASH_STREAM_H__
#define __MORDOR_HASH_STREAM_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <vector>

#include <openssl/sha.h>
#include <openssl/md5.h>

#include "assert.h"
#include "filter.h"
#include "buffer.h"

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
    virtual std::string hash() const;
    virtual size_t hashSize() const = 0;
    virtual void hash(void *result, size_t length) const = 0;
    /// dump variant Hash context into buffer for later resuming
    virtual Buffer dumpContext() const = 0;

    virtual void reset() = 0;

protected:
    virtual void updateHash(const void *buffer, size_t length) = 0;
};

enum HASH_TYPE
{
    MD5    = 0,
#ifndef OPENSSL_NO_SHA0
    SHA0   = 1,
#endif
#ifndef OPENSSL_NO_SHA1
    SHA1   = 2,
#endif
#ifndef OPENSSL_NO_SHA256
    SHA224 = 3,
    SHA256 = 4,
#endif
    SHA384 = 5,
#ifndef OPENSSL_NO_SHA512
    SHA512 = 6,
#endif
};

namespace {
// templated hash stream in anonymous namespace
template<HASH_TYPE H> struct HashOps {};

#ifndef OPENSSL_NO_SHA0
template<>
struct HashOps<SHA0>
{
    typedef SHA_CTX ctx_type;

    static int init(ctx_type *ctx)
    { return SHA_Init(ctx); }
    static int update(ctx_type *ctx, const void *data, size_t len)
    { return SHA_Update(ctx, data, len); }
    static int final(unsigned char *md, ctx_type *ctx)
    { return SHA_Final(md, ctx); }
    static size_t digestLength() { return SHA_DIGEST_LENGTH; }
};
#endif

#ifndef OPENSSL_NO_SHA1
template<>
struct HashOps<SHA1>
{
    typedef SHA_CTX ctx_type;

    static int init(ctx_type *ctx)
    { return SHA1_Init(ctx); }
    static int update(ctx_type *ctx, const void *data, size_t len)
    { return SHA1_Update(ctx, data, len); }
    static int final(unsigned char *md, ctx_type *ctx)
    { return SHA1_Final(md, ctx); }
    static size_t digestLength() { return SHA_DIGEST_LENGTH; }
};
#endif

#ifndef OPENSSL_NO_SHA256
template<>
struct HashOps<SHA224>
{
    typedef SHA256_CTX ctx_type;

    static int init(ctx_type *ctx)
    { return SHA224_Init(ctx); }
    static int update(ctx_type *ctx, const void *data, size_t len)
    { return SHA224_Update(ctx, data, len); }
    static int final(unsigned char *md, ctx_type *ctx)
    { return SHA224_Final(md, ctx); }
    static size_t digestLength() { return SHA224_DIGEST_LENGTH; }
};

template<>
struct HashOps<SHA256>
{
    typedef SHA256_CTX ctx_type;

    static int init(ctx_type *ctx)
    { return SHA256_Init(ctx); }
    static int update(ctx_type *ctx, const void *data, size_t len)
    { return SHA256_Update(ctx, data, len); }
    static int final(unsigned char *md, ctx_type *ctx)
    { return SHA256_Final(md, ctx); }
    static size_t digestLength() { return SHA256_DIGEST_LENGTH; }
};
#endif

template<>
struct HashOps<MD5>
{
    typedef MD5_CTX ctx_type;

    static int init(ctx_type *ctx)
    { return MD5_Init(ctx); }
    static int update(ctx_type *ctx, const void *data, size_t len)
    { return MD5_Update(ctx, data, len); }
    static int final(unsigned char *md, ctx_type *ctx)
    { return MD5_Final(md, ctx); }
    static size_t digestLength() { return SHA256_DIGEST_LENGTH; }
};

template<HASH_TYPE H>
class _HashStream : public HashStream
{
public:
    typedef typename HashOps<H>::ctx_type ctx_type;
    typedef boost::shared_ptr<_HashStream<H> > ptr;

private:
    typedef HashOps<H> hash_ops;

public:
    _HashStream(Stream::ptr parent, bool own = true)
        : HashStream(parent, own)
    { hash_ops::init(&m_ctx); }

    _HashStream(Stream::ptr parent, const ctx_type &ctx, bool own = true)
        : HashStream(parent, own)
        , m_ctx(ctx)
    {}

    _HashStream(Stream::ptr parent, const Buffer &buffer, bool own = true)
        : HashStream(parent, own)
    { buffer.copyOut(&m_ctx, sizeof(ctx_type)); }

    size_t hashSize() const { return hash_ops::digestLength(); }
    void reset() { hash_ops::init(&m_ctx); }
    ctx_type ctx() const { return m_ctx; }

    using HashStream::hash;
    void hash(void *result, size_t length) const
    {
        ctx_type copy(m_ctx);
        hash_ops::final((unsigned char *)result, &copy);
    }

    Buffer dumpContext() const
    {
        Buffer buffer;
        buffer.copyIn(&m_ctx, sizeof(ctx_type));
        return buffer;
    }

private:
    void updateHash(const void *buffer, size_t length)
    { hash_ops::update(&m_ctx, buffer, length); }

protected:
    ctx_type m_ctx;
};

} // end of anonymous namespace

#ifndef OPENSSL_NO_SHA0
typedef _HashStream<SHA0>   SHA0Stream;
#endif
#ifndef OPENSSL_NO_SHA1
typedef _HashStream<SHA1>   SHA1Stream;
#endif
#ifndef OPENSSL_NO_SHA256
typedef _HashStream<SHA224> SHA224Stream;
typedef _HashStream<SHA256> SHA256Stream;
#endif
typedef _HashStream<MD5>    MD5Stream;

class CRC32Stream : public HashStream
{
public:
    /// Using a well-known polynomial will automatically select a precomputed table
    enum WellknownPolynomial
    {
        /// CRC-32-IEEE, used in V.42, Ethernet, MPEG-2, PNG, POSIX cksum, etc.
        IEEE = 0x04C11DB7,
        /// CRC-32C used in iSCSI and SCTP
        CASTAGNOLI = 0x1EDC6F41,
        /// CRC-32K
        KOOPMAN = 0x741B8CD7
    };
public:
    CRC32Stream(Stream::ptr parent, unsigned int polynomial = IEEE,
        bool own = true);
    CRC32Stream(Stream::ptr parent, const unsigned int *precomputedTable,
        bool own = true);

    static std::vector<unsigned int> precomputeTable(unsigned int polynomial);

    size_t hashSize() const;
    using HashStream::hash;
    void hash(void *result, size_t length) const;
    void reset();
    Buffer dumpContext() const { return Buffer(); }

protected:
    void updateHash(const void *buffer, size_t length);

private:
    unsigned int m_crc;
    const std::vector<unsigned int> m_tableStorage;
    const unsigned int *m_table;
};

}

#endif
