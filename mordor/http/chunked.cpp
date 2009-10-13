// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "chunked.h"

#include <sstream>
#include <stdexcept>

#include "mordor/version.h"

namespace Mordor {
namespace HTTP {

InvalidChunkException::InvalidChunkException(const std::string &line,
                                           Type type)
: m_line(line),
  m_type(type)
{}  

ChunkedStream::ChunkedStream(Stream::ptr parent, bool own)
: MutatingFilterStream(parent, own),
  m_nextChunk(~0)
{
    if (parent->supportsRead()) {
        MORDOR_ASSERT(parent->supportsFind());
    }
}

void
ChunkedStream::close(Stream::CloseType type)
{
    if (supportsWrite() && (type & Stream::WRITE)) {
        parent()->write("0\r\n", 3);
    }
    if (ownsParent())
        parent()->close(type);
}

size_t
ChunkedStream::read(Buffer &b, size_t len)
{
    if (m_nextChunk == ~0ull - 1) {
        std::string chunk = parent()->getDelimited();
        if (!chunk.empty() && chunk[chunk.size() - 1] == '\r')
            chunk.resize(chunk.size() - 1);
        if (!chunk.empty()) {
            MORDOR_THROW_EXCEPTION(InvalidChunkException(chunk, InvalidChunkException::FOOTER));
        }
        m_nextChunk = ~0;
    }
    if (m_nextChunk == ~0ull) {
        std::string chunk = parent()->getDelimited();
        if (!chunk.empty() && chunk[chunk.size() - 1] == '\r')
            chunk.resize(chunk.size() - 1);
        char *end;
        m_nextChunk = strtoull(chunk.c_str(), &end, 16);
        if (end == chunk.c_str()) {
            MORDOR_THROW_EXCEPTION(InvalidChunkException(chunk, InvalidChunkException::HEADER));
        }
    }
    if (m_nextChunk == 0)
        return 0;
    size_t toRead = (size_t)std::min<unsigned long long>(len, m_nextChunk);
    size_t result = parent()->read(b, toRead);
    m_nextChunk -= result;
    if (m_nextChunk == 0) {
        m_nextChunk = ~0ull - 1;
    }
    return result;
}

size_t
ChunkedStream::write(const Buffer &b, size_t len)
{
    std::ostringstream os;
    os << std::hex << len << "\r\n";
    std::string str = os.str();
    parent()->write(str.c_str(), str.size());
    Buffer copy;
    copy.copyIn(b, len);
    while (copy.readAvailable()) {
        size_t result = parent()->write(copy, copy.readAvailable());
        copy.consume(result);
    }
    parent()->write("\r\n", 2);
    return len;
}

}}
