// Copyright (c) 2009 - Mozy, Inc.

#include "hash.h"

#include <boost/bind.hpp>

#include "buffer.h"
#include "mordor/assert.h"

namespace Mordor {

size_t
HashStream::read(Buffer &buffer, size_t length)
{
    Buffer temp;
    size_t result = parent()->read(temp, length);
    updateHash(temp, result);
    buffer.copyIn(temp);
    return result;
}

size_t
HashStream::read(void *buffer, size_t length)
{
    size_t result = parent()->read(buffer, length);
    updateHash(buffer, result);
    return result;
}

size_t
HashStream::write(const Buffer &buffer, size_t length)
{
    size_t result = parent()->write(buffer, length);
    updateHash(buffer, result);
    return result;
}

size_t
HashStream::write(const void *buffer, size_t length)
{
    size_t result = parent()->write(buffer, length);
    updateHash(buffer, result);
    return result;
}

long long
HashStream::seek(long long offset, Anchor anchor)
{
    MORDOR_NOTREACHED();
}

SHA0Stream::SHA0Stream(Stream::ptr parent, bool own)
: HashStream(parent, own)
{
    SHA_Init(&m_ctx);
}

std::string
SHA0Stream::hash() const
{
    SHA_CTX copy(m_ctx);
    std::string result;
    result.resize(SHA_DIGEST_LENGTH);
    SHA_Final((unsigned char *)&result[0], &copy);
    return result;
}

void
SHA0Stream::updateHash(const Buffer &buffer, size_t length)
{
    buffer.visit(boost::bind(&SHA_Update, &m_ctx, _1, _2), length);
}

void
SHA0Stream::updateHash(const void *buffer, size_t length)
{
    SHA_Update(&m_ctx, buffer, length);
}

SHA1Stream::SHA1Stream(Stream::ptr parent, bool own)
: HashStream(parent, own)
{
    SHA1_Init(&m_ctx);
}

std::string
SHA1Stream::hash() const
{
    SHA_CTX copy(m_ctx);
    std::string result;
    result.resize(SHA_DIGEST_LENGTH);
    SHA1_Final((unsigned char *)&result[0], &copy);
    return result;
}

void
SHA1Stream::updateHash(const Buffer &buffer, size_t length)
{
    buffer.visit(boost::bind(&SHA1_Update, &m_ctx, _1, _2), length);
}

void
SHA1Stream::updateHash(const void *buffer, size_t length)
{
    SHA1_Update(&m_ctx, buffer, length);
}

MD5Stream::MD5Stream(Stream::ptr parent, bool own)
: HashStream(parent, own)
{
    MD5_Init(&m_ctx);
}

std::string
MD5Stream::hash() const
{
    MD5_CTX copy(m_ctx);
    std::string result;
    result.resize(MD5_DIGEST_LENGTH);
    MD5_Final((unsigned char *)&result[0], &copy);
    return result;
}

void
MD5Stream::updateHash(const Buffer &buffer, size_t length)
{
    buffer.visit(boost::bind(&MD5_Update, &m_ctx, _1, _2), length);
}

void
MD5Stream::updateHash(const void *buffer, size_t length)
{
    MD5_Update(&m_ctx, buffer, length);
}

}
