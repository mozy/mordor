// Copyright (c) 2009 - Decho Corp.

#include "chunked.h"

#include <sstream>

HTTP::ChunkedStream::ChunkedStream(Stream *parent, bool owns)
: MutatingFilterStream(parent, owns),
  m_nextChunk(~0)
{
    if (parent->supportsRead()) {
        assert(parent->supportsFindDelimited());
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
HTTP::ChunkedStream::read(Buffer *b, size_t len)
{
    if (m_nextChunk == ~0) {
        std::string chunk = parent()->getDelimited();
        char *end;
        m_nextChunk = _strtoui64(chunk.c_str(), &end, 16);
        if (end == chunk.c_str()) {
            throw std::runtime_error("Invalid chunk size: " + chunk);
        }
    }
    if (m_nextChunk == 0)
        return 0;
    size_t toRead = std::min(len, m_nextChunk);
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
HTTP::ChunkedStream::write(const Buffer *b, size_t len)
{
    std::ostringstream os;
    os << std::hex << len;
    std::string str = os.str();
    parent()->write(str.c_str(), str.size());
    Buffer copy;
    copy.copyIn(*b, len);
    while (copy.readAvailable()) {
        size_t result = MutatingFilterStream::write(&copy, copy.readAvailable());
        copy.consume(result);
    }
    parent()->write("\r\n", 2);
    return len;
}
