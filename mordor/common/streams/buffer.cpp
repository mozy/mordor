// Copyright (c) 2009 - Decho Corp.

#include "buffer.h"

#include <string.h>
#include <algorithm>
#include <cassert>

#ifdef min
#undef min
#endif

Buffer::DataBuf::DataBuf()
{
    start(NULL);
    length(0);
}

Buffer::DataBuf::DataBuf(size_t length)
{
    assert(length <= 0xffffffff);
    m_array.reset(new unsigned char[length]);
    start(m_array.get());
    this->length(length);
}

Buffer::DataBuf
Buffer::DataBuf::slice(size_t start, size_t length)
{
    if (length == (size_t)~0) {
        length = this->length() - start;
    }
    assert(start <= this->length());
    assert(length + start <= this->length());
    DataBuf result;
    result.m_array = m_array;
    result.start((unsigned char*)this->start() + start);
    result.length(length);
    return result;
}

const Buffer::DataBuf
Buffer::DataBuf::slice(size_t start, size_t length) const
{
    if (length == (size_t)~0) {
        length = this->length() - start;
    }
    assert(start <= this->length());
    assert(length + start <= this->length());
    DataBuf result;
    result.m_array = m_array;
    result.start((unsigned char*)this->start() + start);
    result.length(length);
    return result;
}

Buffer::Data::Data(size_t len)
: m_writeIndex(0), m_buf(len)
{
    invariant();
}

Buffer::Data::Data(Buffer::DataBuf buf)
: m_writeIndex(buf.length()), m_buf(buf)
{
    invariant();
}

size_t
Buffer::Data::readAvailable() const
{
    invariant();
    return m_writeIndex;
}

size_t
Buffer::Data::writeAvailable() const
{
    invariant();
    return m_buf.length() - m_writeIndex;
}

size_t
Buffer::Data::length() const
{
    invariant();
    return m_buf.length();
}

void
Buffer::Data::produce(size_t len)
{
    assert(len <= writeAvailable());
    m_writeIndex += len;
    invariant();
}

void
Buffer::Data::consume(size_t len)
{
    assert(len <= readAvailable());
    m_writeIndex -= len;
    m_buf = m_buf.slice(len);
    invariant();
}

const Buffer::DataBuf
Buffer::Data::readBuf() const
{
    invariant();
    return m_buf.slice(0, m_writeIndex);
}

Buffer::DataBuf
Buffer::Data::writeBuf()
{
    invariant();
    return m_buf.slice(m_writeIndex);
}

void
Buffer::Data::invariant() const
{
    assert(m_writeIndex <= m_buf.length());
}


Buffer::Buffer()
{
    m_readAvailable = m_writeAvailable = 0;
    m_writeIt = m_bufs.end();
    invariant();
}

size_t
Buffer::readAvailable() const
{
    invariant();
    return m_readAvailable;
}

size_t
Buffer::writeAvailable() const
{
    invariant();
    return m_writeAvailable;
}

void
Buffer::reserve(size_t len)
{
    if (writeAvailable() < len) {
        // over-reserve to avoid fragmentation
        Data newBuf(len * 2 - writeAvailable());
        if (readAvailable() == 0) {
            // put the new buffer at the front if possible to avoid
            // fragmentation
            m_bufs.push_front(newBuf);
            m_writeIt = m_bufs.begin();
        } else {
            m_bufs.push_back(newBuf);
            m_writeIt = m_bufs.end();
            --m_writeIt;
        }
        m_writeAvailable += newBuf.length();
        invariant();
    }
}

void
Buffer::compact()
{
    invariant();
    if (m_writeIt != m_bufs.end()) {
        if (m_writeIt->readAvailable() > 0) {
            Data newBuf = Data(m_writeIt->readBuf());
            m_bufs.insert(m_writeIt, newBuf);
        }
        m_writeIt = m_bufs.erase(m_writeIt, m_bufs.end());
        m_writeAvailable = 0;
    }
    assert(writeAvailable() == 0);
}

void
Buffer::clear()
{
    invariant();
    m_readAvailable = m_writeAvailable = 0;
    m_bufs.clear();
    m_writeIt = m_bufs.end();
    invariant();
    assert(m_readAvailable == 0);
    assert(m_writeAvailable == 0);
}

void
Buffer::produce(size_t len)
{
    assert(len <= writeAvailable());
    m_readAvailable += len;
    m_writeAvailable -= len;
    while (len > 0) {
        Data& buf = *m_writeIt;
        size_t toProduce = std::min(buf.writeAvailable(), len);
        buf.produce(toProduce);
        len -= toProduce;
        if (buf.writeAvailable() == 0) {
            ++m_writeIt;
        }
    }
    assert(len == 0);
    invariant();
}

void
Buffer::consume(size_t len)
{
    assert(len <= readAvailable());
    m_readAvailable -= len;
    while (len > 0) {
        Data& buf = *m_bufs.begin();
        size_t toConsume = std::min(buf.readAvailable(), len);
        buf.consume(toConsume);
        len -= toConsume;
        if (buf.length() == 0) {
            m_bufs.pop_front();
        }
    }
    assert(len == 0);
    invariant();
}

const std::vector<iovec>
Buffer::readBufs(size_t len) const
{
    if (len == (size_t)~0)
        len = readAvailable();
    assert(len <= readAvailable());
    std::vector<iovec> result;
    result.reserve(m_bufs.size());
    size_t remaining = len;
    std::list<Data>::const_iterator it;
    for (it = m_bufs.begin(); it != m_bufs.end(); ++it) {
        size_t toConsume = std::min(it->readAvailable(), remaining);
        DataBuf buf = it->readBuf().slice(0, toConsume);
#ifdef WINDOWS
        while (buf.length() > 0) {
            iovec wsabuf;
            wsabuf.iov_base = (void *)buf.start();
            wsabuf.iov_len = (unsigned int)std::min<size_t>(0xffffffff, buf.length());
            result.push_back(wsabuf);
            buf = buf.slice(wsabuf.iov_len);
        }
#else
        iovec iov;
        iov.iov_base = (void *)buf.start();
        iov.iov_len = buf.length();
        result.push_back(iov);
#endif
        remaining -= toConsume;
        if (remaining == 0) {
            break;
        }
    }
    assert(remaining == 0);
    invariant();
    return result;
}

const Buffer::DataBuf
Buffer::readBuf(size_t len) const
{
    assert(len <= readAvailable());
    if (readAvailable() == 0) {
        return DataBuf();
    }
    // Optimize case where all that is requested is contained in the first
    // buffer
    if (m_bufs.front().readAvailable() >= len) {
        return m_bufs.front().readBuf().slice(0, len);
    }
    // Breaking constness!
    Buffer* _this = const_cast<Buffer*>(this);
    // try to avoid allocation
    if (m_writeIt != m_bufs.end() && m_writeIt->writeAvailable() >= readAvailable()) {
        copyOut(m_writeIt->writeBuf().start(), readAvailable());
        Data newBuf = Data(m_writeIt->writeBuf().slice(0, readAvailable()));
        _this->m_bufs.clear();
        _this->m_bufs.push_back(newBuf);
        _this->m_writeAvailable = 0;
        _this->m_writeIt = _this->m_bufs.end();
        invariant();
        return newBuf.readBuf().slice(0, len);
    }
    Data newBuf = Data(readAvailable());
    copyOut(newBuf.writeBuf().start(), readAvailable());
    newBuf.produce(readAvailable());
    _this->m_bufs.clear();
    _this->m_bufs.push_back(newBuf);
    _this->m_writeAvailable = 0;
    _this->m_writeIt = _this->m_bufs.end();
    invariant();
    return newBuf.readBuf().slice(0, len);
}

std::vector<iovec>
Buffer::writeBufs(size_t len)
{
    if (len == (size_t)~0)
        len = writeAvailable();
    reserve(len);
    std::vector<iovec> result;
    result.reserve(m_bufs.size());
    size_t remaining = len;
    std::list<Data>::iterator it = m_writeIt;
    while (remaining > 0) {
        Data& data = *it;
        size_t toProduce = std::min(data.writeAvailable(), remaining);
        DataBuf buf = data.writeBuf().slice(0, toProduce);
#ifdef WINDOWS    
        while (buf.length() > 0) {
            iovec wsabuf;
            wsabuf.iov_base = (void *)buf.start();
            wsabuf.iov_len = (unsigned int)std::min<size_t>(0xffffffff, buf.length());
            result.push_back(wsabuf);
            buf = buf.slice(wsabuf.iov_len);
        }
#else
        iovec iov;
        iov.iov_base = (void *)buf.start();
        iov.iov_len = buf.length();
        result.push_back(iov);
#endif
        remaining -= toProduce;
        ++it;
    }
    assert(remaining == 0);
    invariant();
    return result;
}

Buffer::DataBuf
Buffer::writeBuf(size_t len)
{
    // Must allocate just the write buf
    if (writeAvailable() == 0) {
        reserve(len);
        assert(m_writeIt != m_bufs.end());
        assert(m_writeIt->writeAvailable() >= len);
        return m_writeIt->writeBuf().slice(0, len);
    }
    // Can use an existing write buf
    if (writeAvailable() >= 0 && m_writeIt->writeAvailable() >= len) {
        return m_writeIt->writeBuf().slice(0, len);
    }
    // Existing bufs are insufficient... remove them and reserve anew
    compact();
    reserve(len);
    reserve(len);
    assert(m_writeIt != m_bufs.end());
    assert(m_writeIt->writeAvailable() >= len);
    return m_writeIt->writeBuf().slice(0, len);
}

void
Buffer::copyIn(const Buffer &buf, size_t len)
{
    if (len == (size_t)~0)
        len = buf.readAvailable();
    assert(buf.readAvailable() >= len);
    invariant();
    // Split any mixed read/write bufs
    if (m_writeIt != m_bufs.end() && m_writeIt->readAvailable() != 0) {
        m_bufs.insert(m_writeIt, Data(m_writeIt->readBuf()));
        m_writeIt->consume(m_writeIt->readAvailable());
    }

    std::list<Data>::const_iterator it;
    for (it = buf.m_bufs.begin(); it != buf.m_bufs.end(); ++it) {
        size_t toConsume = std::min(it->readAvailable(), len);
        Data newBuf = Data(it->readBuf().slice(0, toConsume));
        m_bufs.insert(m_writeIt, newBuf);
        m_readAvailable += toConsume;
        len -= toConsume;
        if (len == 0)
            break;
    }
    assert(len == 0);
    assert(readAvailable() >= len);
}

void
Buffer::copyIn(const void *data, size_t len)
{
    invariant();
    
    while (m_writeIt != m_bufs.end() && len > 0) {
        size_t todo = std::min(len, m_writeIt->writeAvailable());
        memcpy(m_writeIt->writeBuf().start(), data, todo);
        m_writeIt->produce(todo);
        m_writeAvailable -= todo;
        m_readAvailable += todo;
        data = (unsigned char*)data + todo;
        len -= todo;
        if (m_writeIt->writeAvailable() == 0)
            ++m_writeIt;
        invariant();
    }

    if (len > 0) {
        Data newBuf(len);
        memcpy(newBuf.writeBuf().start(), data, len);
        newBuf.produce(len);
        m_bufs.push_back(newBuf);
        m_readAvailable += len;
    }

    assert(readAvailable() >= len);
}

void
Buffer::copyIn(const char *sz)
{
    copyIn(sz, strlen(sz));
}

void
Buffer::copyOut(void *buf, size_t len) const
{
    assert(len <= readAvailable());
    unsigned char *next = (unsigned char*)buf;
    std::list<Data>::const_iterator it;
    for (it = m_bufs.begin(); it != m_bufs.end(); ++it) {
        size_t todo = std::min(len, it->readAvailable());
        memcpy(next, it->readBuf().start(), todo);
        next += todo;
        len -= todo;
        if (len == 0)
            break;
    }
    assert(len == 0);
}

ptrdiff_t
Buffer::findDelimited(char delim, size_t len) const
{
    if (len == (size_t)~0)
        len = readAvailable();
    assert(len <= readAvailable());

    size_t totalLength = 0;
    bool success = false;

    std::list<Data>::const_iterator it;
    for (it = m_bufs.begin(); it != m_bufs.end(); ++it) {
        const void *start = it->readBuf().start();
        size_t toscan = std::min(len, it->readAvailable());
        const void *point = memchr(start, delim, toscan);
        if (point != NULL) {
            success = true;
            totalLength += (unsigned char*)point - (unsigned char*)start;
            break;
        }
        totalLength += toscan;
        len -= toscan;
        if (len == 0)
            break;
    }
    if (success) {
        return totalLength + 1;
    }
    return -1;
}

void
Buffer::invariant() const
{
    size_t read = 0;
    size_t write = 0;
    bool seenWrite = false;
    std::list<Data>::const_iterator it;
    for (it = m_bufs.begin(); it != m_bufs.end(); ++it) {
        const Data& buf = *it;
        // Strict ordering
        assert(!seenWrite || (seenWrite && buf.readAvailable() == 0));
        read += buf.readAvailable();
        write += buf.writeAvailable();
        if (!seenWrite && buf.writeAvailable() != 0) {
            seenWrite = true;
            assert(m_writeIt == it);
        }
    }
    assert(read == m_readAvailable);
    assert(write == m_writeAvailable);
    assert(write != 0 || (write == 0 && m_writeIt == m_bufs.end()));
}
