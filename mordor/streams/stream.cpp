// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "stream.h"

#include <string.h>

namespace Mordor {

size_t
Stream::read(void *buffer, size_t length)
{
    Buffer internalBuffer;
    internalBuffer.adopt(buffer, length);
    size_t result = read(internalBuffer, length);
    MORDOR_ASSERT(result <= length);
    MORDOR_ASSERT(internalBuffer.readAvailable() == result);
    std::vector<iovec> iovs = internalBuffer.readBufs(result);
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
Stream::write(const char *string)
{
    return write(string, strlen(string));
}

size_t
Stream::write(const void *buffer, size_t length)
{
    Buffer internalBuffer;
    internalBuffer.copyIn(buffer, length);
    return write(internalBuffer, length);
}

std::string
Stream::getDelimited(char delim, bool eofIsDelimiter)
{
    ptrdiff_t offset = find(delim, ~0, !eofIsDelimiter);
    eofIsDelimiter = offset < 0;
    if (offset < 0)
        offset = -offset - 1;
    Buffer buf;
#ifdef DEBUG
    size_t readResult = 
#endif
    read(buf, offset + (eofIsDelimiter ? 0 : 1));
    MORDOR_ASSERT((ptrdiff_t)readResult == offset + (eofIsDelimiter ? 0 : 1));
    // Don't copyOut the delimiter itself
    std::string result;
    result.resize(offset);
    buf.copyOut(const_cast<char*>(result.data()), offset);
    return result;
}

}
