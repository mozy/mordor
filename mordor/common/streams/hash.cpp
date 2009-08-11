// Copyright (c) 2009 - Decho Corp.

#include "hash.h"

#include <boost/bind.hpp>

size_t
HashStream::read(Buffer &b, size_t len)
{
    Buffer temp;
    size_t result = FilterStream::read(temp, len);
    updateHash(temp, result);
    b.copyIn(temp);
    return result;
}

size_t
HashStream::write(const Buffer &b, size_t len)
{
    size_t result = FilterStream::write(b, len);
    updateHash(b, result);
    return result;
}

SHA0Stream::SHA0Stream(Stream::ptr parent, bool own)
: HashStream(parent, own)
{
    SHA_Init(&m_ctx);
}

std::vector<unsigned char>
SHA0Stream::hash() const
{
    SHA_CTX copy(m_ctx);
    std::vector<unsigned char> result;
    result.resize(SHA_DIGEST_LENGTH);
    SHA_Final(&result[0], &copy);
    return result;
}

void
SHA0Stream::updateHash(const Buffer &b, size_t len)
{
    b.visit(boost::bind(&SHA_Update, &m_ctx, _1, _2), len);
}

SHA1Stream::SHA1Stream(Stream::ptr parent, bool own)
: HashStream(parent, own)
{
    SHA1_Init(&m_ctx);
}

std::vector<unsigned char>
SHA1Stream::hash() const
{
    SHA_CTX copy(m_ctx);
    std::vector<unsigned char> result;
    result.resize(SHA_DIGEST_LENGTH);
    SHA1_Final(&result[0], &copy);
    return result;
}

void
SHA1Stream::updateHash(const Buffer &b, size_t len)
{
    b.visit(boost::bind(&SHA1_Update, &m_ctx, _1, _2), len);
}

MD5Stream::MD5Stream(Stream::ptr parent, bool own)
: HashStream(parent, own)
{
    MD5_Init(&m_ctx);
}

std::vector<unsigned char>
MD5Stream::hash() const
{
    MD5_CTX copy(m_ctx);
    std::vector<unsigned char> result;
    result.resize(MD5_DIGEST_LENGTH);
    MD5_Final(&result[0], &copy);
    return result;
}

void
MD5Stream::updateHash(const Buffer &b, size_t len)
{
    b.visit(boost::bind(&MD5_Update, &m_ctx, _1, _2), len);
}
