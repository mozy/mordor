// Copyright (c) 2009 - Decho Corp.

#include "chunked.h"

#include <sstream>
#include <stdexcept>

#include "common/version.h"

#ifdef WINDOWS
#define strtoull _strtoui64
#endif

HTTP::ChunkedStream::ChunkedStream(Stream::ptr parent, bool own)
: MutatingFilterStream(parent, own),
  m_nextChunk(~0)
{
    if (parent->supportsRead()) {
        assert(parent->supportsFind());
    }
}

void
HTTP::ChunkedStream::close(Stream::CloseType type)
{
    if (supportsWrite() && (type & Stream::WRITE)) {
        parent()->write("0\r\n", 3);
    }
    MutatingFilterStream::close(type);
}

size_t
HTTP::ChunkedStream::read(Buffer &b, size_t len)
{
    if (m_nextChunk == ~0ull) {
        std::string chunk = parent()->getDelimited();
        char *end;
        m_nextChunk = strtoull(chunk.c_str(), &end, 16);
        if (end == chunk.c_str()) {
            throw std::runtime_error("Invalid chunk size: " + chunk);
        }
    }
    if (m_nextChunk == 0)
        return 0;
    size_t toRead = (size_t)std::min<unsigned long long>(len, m_nextChunk);
    size_t result = MutatingFilterStream::read(b, toRead);
    m_nextChunk -= result;
    if (m_nextChunk == 0) {
        std::string chunk = parent()->getDelimited();
        if (chunk != "\r" && !chunk.empty()) {
            throw std::runtime_error("Invalid end-of-chunk line");
        }
        m_nextChunk = ~0;
    }
    return result;
}

size_t
HTTP::ChunkedStream::write(const Buffer &b, size_t len)
{
    std::ostringstream os;
    os << std::hex << len;
    std::string str = os.str();
    parent()->write(str.c_str(), str.size());
    Buffer copy;
    copy.copyIn(b, len);
    while (copy.readAvailable()) {
        size_t result = MutatingFilterStream::write(copy, copy.readAvailable());
        copy.consume(result);
    }
    parent()->write("\r\n", 2);
    return len;
}
