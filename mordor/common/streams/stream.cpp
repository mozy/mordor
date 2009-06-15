// Copyright (c) 2009 - Decho Corp.

#include "stream.h"

#include <string.h>

size_t
Stream::write(const char *sz)
{
    return write(sz, strlen(sz));
}

size_t
Stream::write(const void *b, size_t len)
{
    Buffer buf;
    buf.copyIn(b, len);
    return write(buf, len);
}

std::string
Stream::getDelimited(char delim)
{
    size_t offset = find(delim);
    if (offset == 0)
        return std::string();
    Buffer buf;
    size_t readResult = read(buf, offset);
    assert(readResult == offset);
    // Don't copyOut the delimiter itself
    std::string result;
    result.resize(readResult - 1);
    buf.copyOut(const_cast<char*>(result.data()), readResult - 1);
    return result;
}
