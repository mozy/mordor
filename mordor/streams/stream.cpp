// Copyright (c) 2009 - Mozy, Inc.

#include "stream.h"

#include <boost/scoped_array.hpp>

#include <string.h>

#include "buffer.h"
#include "mordor/assert.h"

namespace Mordor {

size_t
Stream::read(void *buffer, size_t length)
{
    MORDOR_ASSERT(supportsRead());
    Buffer internalBuffer;
    internalBuffer.adopt(buffer, length);
    size_t result = read(internalBuffer, length);
    MORDOR_ASSERT(result <= length);
    MORDOR_ASSERT(internalBuffer.readAvailable() == result);
    if (result == 0u)
        return 0;
    std::vector<iovec> iovs = internalBuffer.readBuffers(result);
    MORDOR_ASSERT(!iovs.empty());
    // It wrote directly into our buffer
    if (iovs.front().iov_base == buffer && iovs.front().iov_len == result)
        return result;
    bool overlapping = false;
    for (std::vector<iovec>::iterator it = iovs.begin();
        it != iovs.end();
        ++it) {
        if (it->iov_base >= buffer || it->iov_base <=
            (unsigned char *)buffer + length) {
            overlapping = true;
            break;
        }
    }
    // It didn't touch our buffer at all; it's safe to just copyOut
    if (!overlapping) {
        internalBuffer.copyOut(buffer, result);
        return result;
    }
    // We have to allocate *another* buffer so we don't destroy any data while
    // copying to our buffer
    boost::scoped_array<unsigned char> extraBuffer(new unsigned char[result]);
    internalBuffer.copyOut(extraBuffer.get(), result);
    memcpy(buffer, extraBuffer.get(), result);
    return result;
}

size_t
Stream::read(Buffer &buffer, size_t length)
{
    return read(buffer, length, false);
}

size_t
Stream::read(Buffer &buffer, size_t length, bool coalesce)
{
    MORDOR_ASSERT(supportsRead());
    iovec iov = buffer.writeBuffer(length, coalesce);
    size_t result = read(iov.iov_base, iov.iov_len);
    buffer.produce(result);
    return result;
}

size_t
Stream::write(const char *string)
{
    return write(string, strlen(string));
}

size_t
Stream::write(const void *buffer, size_t length)
{
    MORDOR_ASSERT(supportsWrite());
    Buffer internalBuffer;
    internalBuffer.copyIn(buffer, length);
    return write(internalBuffer, length);
}

size_t
Stream::write(const Buffer &buffer, size_t length)
{
    return write(buffer, length, false);
}

size_t
Stream::write(const Buffer &buffer, size_t length, bool coalesce)
{
    MORDOR_ASSERT(supportsWrite());
    const iovec iov = buffer.readBuffer(length, coalesce);
    return write(iov.iov_base, iov.iov_len);
}

long long
Stream::seek(long long offset, Anchor anchor)
{
    MORDOR_NOTREACHED();
}

long long
Stream::size()
{
    MORDOR_NOTREACHED();
}

void
Stream::truncate(long long size)
{
    MORDOR_NOTREACHED();
}

ptrdiff_t
Stream::find(char delimiter, size_t sanitySize, bool throwIfNotFound)
{
    MORDOR_NOTREACHED();
}

ptrdiff_t
Stream::find(const std::string &delimiter, size_t sanitySize,
    bool throwIfNotFound)
{
    MORDOR_NOTREACHED();
}

std::string
Stream::getDelimited(char delim, bool eofIsDelimiter, bool includeDelimiter)
{
    ptrdiff_t offset = find(delim, ~0, !eofIsDelimiter);
    eofIsDelimiter = offset < 0;
    if (offset < 0)
        offset = -offset - 1;
    std::string result;
    result.resize(offset + (eofIsDelimiter ? 0 : 1));
#ifdef DEBUG
    size_t readResult =
#endif
    read((char *)result.c_str(), result.size());
    MORDOR_ASSERT(readResult == result.size());
    if (!eofIsDelimiter && !includeDelimiter)
        result.resize(result.size() - 1);
    return result;
}

std::string
Stream::getDelimited(const std::string &delim, bool eofIsDelimiter,
    bool includeDelimiter)
{
    ptrdiff_t offset = find(delim, ~0, !eofIsDelimiter);
    eofIsDelimiter = offset < 0;
    if (offset < 0)
        offset = -offset - delim.size();
    std::string result;
    result.resize(offset + (eofIsDelimiter ? 0 : delim.size()));
#ifdef DEBUG
    size_t readResult =
#endif
    read((char *)result.c_str(), result.size());
    MORDOR_ASSERT(readResult == result.size());
    if (!eofIsDelimiter && !includeDelimiter)
        result.resize(result.size() - delim.size());
    return result;
}

void
Stream::unread(const Buffer &buffer, size_t length)
{
    MORDOR_NOTREACHED();
}

}
