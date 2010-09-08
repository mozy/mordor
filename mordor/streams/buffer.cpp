// Copyright (c) 2009 - Mozy, Inc.

#include "buffer.h"

#include <string.h>
#include <algorithm>

#include "mordor/assert.h"
#include "mordor/util.h"

#ifdef WINDOWS
static u_long iovLength(size_t length)
{
    return (u_long)std::min<size_t>(length, 0xffffffff);
}
#else
static size_t iovLength(size_t length)
{
    return length;
}
#endif

namespace Mordor {

Buffer::SegmentData::SegmentData()
{
    start(NULL);
    length(0);
}

Buffer::SegmentData::SegmentData(size_t length)
{
    m_array.reset(new unsigned char[length]);
    start(m_array.get());
    this->length(length);
}

Buffer::SegmentData::SegmentData(void *buffer, size_t length)
{
    m_array.reset((unsigned char *)buffer, &nop<unsigned char *>);
    start(m_array.get());
    this->length(length);
}

Buffer::SegmentData
Buffer::SegmentData::slice(size_t start, size_t length)
{
    if (length == (size_t)~0)
        length = this->length() - start;
    MORDOR_ASSERT(start <= this->length());
    MORDOR_ASSERT(length + start <= this->length());
    SegmentData result;
    result.m_array = m_array;
    result.start((unsigned char*)this->start() + start);
    result.length(length);
    return result;
}

const Buffer::SegmentData
Buffer::SegmentData::slice(size_t start, size_t length) const
{
    if (length == (size_t)~0)
        length = this->length() - start;
    MORDOR_ASSERT(start <= this->length());
    MORDOR_ASSERT(length + start <= this->length());
    SegmentData result;
    result.m_array = m_array;
    result.start((unsigned char*)this->start() + start);
    result.length(length);
    return result;
}

void
Buffer::SegmentData::extend(size_t length)
{
    // NO CHECKS FOR BUFFER OVERRUN!!
    m_length += length;
}

Buffer::Segment::Segment(size_t length)
: m_writeIndex(0), m_data(length)
{
    invariant();
}

Buffer::Segment::Segment(Buffer::SegmentData data)
: m_writeIndex(data.length()), m_data(data)
{
    invariant();
}

Buffer::Segment::Segment(void *buffer, size_t length)
: m_writeIndex(0), m_data(buffer, length)
{
    invariant();
}

size_t
Buffer::Segment::readAvailable() const
{
    invariant();
    return m_writeIndex;
}

size_t
Buffer::Segment::writeAvailable() const
{
    invariant();
    return m_data.length() - m_writeIndex;
}

size_t
Buffer::Segment::length() const
{
    invariant();
    return m_data.length();
}

void
Buffer::Segment::produce(size_t length)
{
    MORDOR_ASSERT(length <= writeAvailable());
    m_writeIndex += length;
    invariant();
}

void
Buffer::Segment::consume(size_t length)
{
    MORDOR_ASSERT(length <= readAvailable());
    m_writeIndex -= length;
    m_data = m_data.slice(length);
    invariant();
}

void
Buffer::Segment::truncate(size_t length)
{
    MORDOR_ASSERT(length <= readAvailable());
    MORDOR_ASSERT(m_writeIndex = readAvailable());
    m_writeIndex = length;
    m_data = m_data.slice(0, length);
    invariant();
}

void
Buffer::Segment::extend(size_t length)
{
    m_data.extend(length);
    m_writeIndex += length;
}

const Buffer::SegmentData
Buffer::Segment::readBuffer() const
{
    invariant();
    return m_data.slice(0, m_writeIndex);
}

Buffer::SegmentData
Buffer::Segment::writeBuffer()
{
    invariant();
    return m_data.slice(m_writeIndex);
}

const Buffer::SegmentData
Buffer::Segment::writeBuffer() const
{
    invariant();
    return m_data.slice(m_writeIndex);
}

void
Buffer::Segment::invariant() const
{
    MORDOR_ASSERT(m_writeIndex <= m_data.length());
}


Buffer::Buffer()
{
    m_readAvailable = m_writeAvailable = 0;
    m_writeIt = m_segments.end();
    invariant();
}

Buffer::Buffer(const Buffer &copy)
{
    m_readAvailable = m_writeAvailable = 0;
    m_writeIt = m_segments.end();
    copyIn(copy);
}

Buffer::Buffer(const char *string)
{
    m_readAvailable = m_writeAvailable = 0;
    m_writeIt = m_segments.end();
    copyIn(string, strlen(string));
}

Buffer::Buffer(const std::string &string)
{
    m_readAvailable = m_writeAvailable = 0;
    m_writeIt = m_segments.end();
    copyIn(string.c_str(), string.size());
}

Buffer::Buffer(const void *data, size_t length)
{
    m_readAvailable = m_writeAvailable = 0;
    m_writeIt = m_segments.end();
    copyIn(data, length);
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

size_t
Buffer::segments() const
{
    invariant();
    return m_segments.size();
}

void
Buffer::adopt(void *buffer, size_t length)
{
    invariant();
    Segment newSegment(buffer, length);
    if (readAvailable() == 0) {
        // put the new buffer at the front if possible to avoid
        // fragmentation
        m_segments.push_front(newSegment);
        m_writeIt = m_segments.begin();
    } else {
        m_segments.push_back(newSegment);
        if (m_writeAvailable == 0) {
            m_writeIt = m_segments.end();
            --m_writeIt;
        }
    }
    m_writeAvailable += length;
    invariant();
}

void
Buffer::reserve(size_t length)
{
    if (writeAvailable() < length) {
        // over-reserve to avoid fragmentation
        Segment newSegment(length * 2 - writeAvailable());
        if (readAvailable() == 0) {
            // put the new buffer at the front if possible to avoid
            // fragmentation
            m_segments.push_front(newSegment);
            m_writeIt = m_segments.begin();
        } else {
            m_segments.push_back(newSegment);
            if (m_writeAvailable == 0) {
                m_writeIt = m_segments.end();
                --m_writeIt;
            }
        }
        m_writeAvailable += newSegment.length();
        invariant();
    }
}

void
Buffer::compact()
{
    invariant();
    if (m_writeIt != m_segments.end()) {
        if (m_writeIt->readAvailable() > 0) {
            Segment newSegment = Segment(m_writeIt->readBuffer());
            m_segments.insert(m_writeIt, newSegment);
        }
        m_writeIt = m_segments.erase(m_writeIt, m_segments.end());
        m_writeAvailable = 0;
    }
    MORDOR_ASSERT(writeAvailable() == 0);
}

void
Buffer::clear()
{
    invariant();
    m_readAvailable = m_writeAvailable = 0;
    m_segments.clear();
    m_writeIt = m_segments.end();
    invariant();
    MORDOR_ASSERT(m_readAvailable == 0);
    MORDOR_ASSERT(m_writeAvailable == 0);
}

void
Buffer::produce(size_t length)
{
    MORDOR_ASSERT(length <= writeAvailable());
    m_readAvailable += length;
    m_writeAvailable -= length;
    while (length > 0) {
        Segment &segment = *m_writeIt;
        size_t toProduce = std::min(segment.writeAvailable(), length);
        segment.produce(toProduce);
        length -= toProduce;
        if (segment.writeAvailable() == 0)
            ++m_writeIt;
    }
    MORDOR_ASSERT(length == 0);
    invariant();
}

void
Buffer::consume(size_t length)
{
    MORDOR_ASSERT(length <= readAvailable());
    m_readAvailable -= length;
    while (length > 0) {
        Segment &segment = *m_segments.begin();
        size_t toConsume = std::min(segment.readAvailable(), length);
        segment.consume(toConsume);
        length -= toConsume;
        if (segment.length() == 0)
            m_segments.pop_front();
    }
    MORDOR_ASSERT(length == 0);
    invariant();
}

void
Buffer::truncate(size_t length)
{
    MORDOR_ASSERT(length <= readAvailable());
    if (length == m_readAvailable)
        return;
    // Split any mixed read/write bufs
    if (m_writeIt != m_segments.end() && m_writeIt->readAvailable() != 0) {
        m_segments.insert(m_writeIt, Segment(m_writeIt->readBuffer()));
        m_writeIt->consume(m_writeIt->readAvailable());
    }
    m_readAvailable = length;
    std::list<Segment>::iterator it;
    for (it = m_segments.begin(); it != m_segments.end() && length > 0; ++it) {
        Segment &segment = *it;
        if (length <= segment.readAvailable()) {
            segment.truncate(length);
            length = 0;
            ++it;
            break;
        } else {
            length -= segment.readAvailable();
        }
    }
    MORDOR_ASSERT(length == 0);
    while (it != m_segments.end() && it->readAvailable() > 0) {
        MORDOR_ASSERT(it->writeAvailable() == 0);
        it = m_segments.erase(it);
    }
    invariant();
}

const std::vector<iovec>
Buffer::readBuffers(size_t length) const
{
    if (length == (size_t)~0)
        length = readAvailable();
    MORDOR_ASSERT(length <= readAvailable());
    std::vector<iovec> result;
    result.reserve(m_segments.size());
    size_t remaining = length;
    std::list<Segment>::const_iterator it;
    for (it = m_segments.begin(); it != m_segments.end(); ++it) {
        size_t toConsume = std::min(it->readAvailable(), remaining);
        SegmentData data = it->readBuffer().slice(0, toConsume);
#ifdef WINDOWS
        while (data.length() > 0) {
            iovec wsabuf;
            wsabuf.iov_base = (void *)data.start();
            wsabuf.iov_len = iovLength(data.length());
            result.push_back(wsabuf);
            data = data.slice(wsabuf.iov_len);
        }
#else
        iovec iov;
        iov.iov_base = (void *)data.start();
        iov.iov_len = data.length();
        result.push_back(iov);
#endif
        remaining -= toConsume;
        if (remaining == 0)
            break;
    }
    MORDOR_ASSERT(remaining == 0);
    invariant();
    return result;
}

const iovec
Buffer::readBuffer(size_t length, bool coalesce) const
{
    iovec result;
    result.iov_base = NULL;
    result.iov_len = 0;
    if (length == (size_t)~0)
        length = readAvailable();
    MORDOR_ASSERT(length <= readAvailable());
    if (readAvailable() == 0)
        return result;
    // Optimize case where all that is requested is contained in the first
    // buffer
    if (m_segments.front().readAvailable() >= length) {
        SegmentData data = m_segments.front().readBuffer().slice(0, length);
        result.iov_base = data.start();
        result.iov_len = iovLength(data.length());
        return result;
    }
    // If they don't want us to coalesce, just return as much as we can from
    // the first segment
    if (!coalesce) {
        SegmentData data = m_segments.front().readBuffer();
        result.iov_base = data.start();
        result.iov_len = iovLength(data.length());
        return result;
    }
    // Breaking constness!
    Buffer* _this = const_cast<Buffer*>(this);
    // try to avoid allocation
    if (m_writeIt != m_segments.end() && m_writeIt->writeAvailable()
        >= readAvailable()) {
        copyOut(m_writeIt->writeBuffer().start(), readAvailable());
        Segment newSegment = Segment(m_writeIt->writeBuffer().slice(0,
            readAvailable()));
        _this->m_segments.clear();
        _this->m_segments.push_back(newSegment);
        _this->m_writeAvailable = 0;
        _this->m_writeIt = _this->m_segments.end();
        invariant();
        SegmentData data = newSegment.readBuffer().slice(0, length);
        result.iov_base = data.start();
        result.iov_len = iovLength(data.length());
        return result;
    }
    Segment newSegment = Segment(readAvailable());
    copyOut(newSegment.writeBuffer().start(), readAvailable());
    newSegment.produce(readAvailable());
    _this->m_segments.clear();
    _this->m_segments.push_back(newSegment);
    _this->m_writeAvailable = 0;
    _this->m_writeIt = _this->m_segments.end();
    invariant();
    SegmentData data = newSegment.readBuffer().slice(0, length);
    result.iov_base = data.start();
    result.iov_len = iovLength(data.length());
    return result;
}

std::vector<iovec>
Buffer::writeBuffers(size_t length)
{
    if (length == (size_t)~0)
        length = writeAvailable();
    reserve(length);
    std::vector<iovec> result;
    result.reserve(m_segments.size());
    size_t remaining = length;
    std::list<Segment>::iterator it = m_writeIt;
    while (remaining > 0) {
        Segment& segment = *it;
        size_t toProduce = std::min(segment.writeAvailable(), remaining);
        SegmentData data = segment.writeBuffer().slice(0, toProduce);
#ifdef WINDOWS
        while (data.length() > 0) {
            iovec wsabuf;
            wsabuf.iov_base = (void *)data.start();
            wsabuf.iov_len = iovLength(data.length());
            result.push_back(wsabuf);
            data = data.slice(wsabuf.iov_len);
        }
#else
        iovec iov;
        iov.iov_base = (void *)data.start();
        iov.iov_len = data.length();
        result.push_back(iov);
#endif
        remaining -= toProduce;
        ++it;
    }
    MORDOR_ASSERT(remaining == 0);
    invariant();
    return result;
}

iovec
Buffer::writeBuffer(size_t length, bool coalesce)
{
    iovec result;
    result.iov_base = NULL;
    result.iov_len = 0;
    if (length == 0u)
        return result;
    // Must allocate just the write segment
    if (writeAvailable() == 0) {
        reserve(length);
        MORDOR_ASSERT(m_writeIt != m_segments.end());
        MORDOR_ASSERT(m_writeIt->writeAvailable() >= length);
        SegmentData data = m_writeIt->writeBuffer().slice(0, length);
        result.iov_base = data.start();
        result.iov_len = iovLength(data.length());
        return result;
    }
    // Can use an existing write segment
    if (writeAvailable() > 0 && m_writeIt->writeAvailable() >= length) {
        SegmentData data = m_writeIt->writeBuffer().slice(0, length);
        result.iov_base = data.start();
        result.iov_len = iovLength(data.length());
        return result;
    }
    // If they don't want us to coalesce, just return as much as we can from
    // the first segment
    if (!coalesce) {
        SegmentData data = m_writeIt->writeBuffer();
        result.iov_base = data.start();
        result.iov_len = iovLength(data.length());
        return result;
    }
    // Existing bufs are insufficient... remove them and reserve anew
    compact();
    reserve(length);
    MORDOR_ASSERT(m_writeIt != m_segments.end());
    MORDOR_ASSERT(m_writeIt->writeAvailable() >= length);
    SegmentData data = m_writeIt->writeBuffer().slice(0, length);
    result.iov_base = data.start();
    result.iov_len = iovLength(data.length());
    return result;
}

void
Buffer::copyIn(const Buffer &buffer, size_t length)
{
    if (length == (size_t)~0)
        length = buffer.readAvailable();
    MORDOR_ASSERT(buffer.readAvailable() >= length);
    invariant();
    if (length == 0)
        return;

    // Split any mixed read/write bufs
    if (m_writeIt != m_segments.end() && m_writeIt->readAvailable() != 0) {
        m_segments.insert(m_writeIt, Segment(m_writeIt->readBuffer()));
        m_writeIt->consume(m_writeIt->readAvailable());
        invariant();
    }

    std::list<Segment>::const_iterator it;
    for (it = buffer.m_segments.begin(); it != buffer.m_segments.end(); ++it) {
        size_t toConsume = std::min(it->readAvailable(), length);
        if (m_readAvailable != 0 && it == buffer.m_segments.begin()) {
            std::list<Segment>::iterator previousIt = m_writeIt;
            --previousIt;
            if ((unsigned char *)previousIt->readBuffer().start() +
                previousIt->readBuffer().length() == it->readBuffer().start() &&
                previousIt->m_data.m_array.get() == it->m_data.m_array.get()) {
                MORDOR_ASSERT(previousIt->writeAvailable() == 0);
                previousIt->extend(toConsume);
                m_readAvailable += toConsume;
                length -= toConsume;
                if (length == 0)
                    break;
                continue;
            }
        }
        Segment newSegment = Segment(it->readBuffer().slice(0, toConsume));
        m_segments.insert(m_writeIt, newSegment);
        m_readAvailable += toConsume;
        length -= toConsume;
        if (length == 0)
            break;
    }
    MORDOR_ASSERT(length == 0);
    MORDOR_ASSERT(readAvailable() >= length);
}

void
Buffer::copyIn(const void *data, size_t length)
{
    invariant();

    while (m_writeIt != m_segments.end() && length > 0) {
        size_t todo = std::min(length, m_writeIt->writeAvailable());
        memcpy(m_writeIt->writeBuffer().start(), data, todo);
        m_writeIt->produce(todo);
        m_writeAvailable -= todo;
        m_readAvailable += todo;
        data = (unsigned char*)data + todo;
        length -= todo;
        if (m_writeIt->writeAvailable() == 0)
            ++m_writeIt;
        invariant();
    }

    if (length > 0) {
        Segment newSegment(length);
        memcpy(newSegment.writeBuffer().start(), data, length);
        newSegment.produce(length);
        m_segments.push_back(newSegment);
        m_readAvailable += length;
    }

    MORDOR_ASSERT(readAvailable() >= length);
}

void
Buffer::copyIn(const char *string)
{
    copyIn(string, strlen(string));
}

void
Buffer::copyOut(void *buffer, size_t length) const
{
    MORDOR_ASSERT(length <= readAvailable());
    unsigned char *next = (unsigned char*)buffer;
    std::list<Segment>::const_iterator it;
    for (it = m_segments.begin(); it != m_segments.end(); ++it) {
        size_t todo = std::min(length, it->readAvailable());
        memcpy(next, it->readBuffer().start(), todo);
        next += todo;
        length -= todo;
        if (length == 0)
            break;
    }
    MORDOR_ASSERT(length == 0);
}

ptrdiff_t
Buffer::find(char delimiter, size_t length) const
{
    if (length == (size_t)~0)
        length = readAvailable();
    MORDOR_ASSERT(length <= readAvailable());

    size_t totalLength = 0;
    bool success = false;

    std::list<Segment>::const_iterator it;
    for (it = m_segments.begin(); it != m_segments.end(); ++it) {
        const void *start = it->readBuffer().start();
        size_t toscan = std::min(length, it->readAvailable());
        const void *point = memchr(start, delimiter, toscan);
        if (point != NULL) {
            success = true;
            totalLength += (unsigned char*)point - (unsigned char*)start;
            break;
        }
        totalLength += toscan;
        length -= toscan;
        if (length == 0)
            break;
    }
    if (success)
        return totalLength;
    return -1;
}

ptrdiff_t
Buffer::find(const std::string &string, size_t length) const
{
    if (length == (size_t)~0)
        length = readAvailable();
    MORDOR_ASSERT(length <= readAvailable());
    MORDOR_ASSERT(!string.empty());

    size_t totalLength = 0;
    size_t foundSoFar = 0;

    std::list<Segment>::const_iterator it;
    for (it = m_segments.begin(); it != m_segments.end(); ++it) {
        const void *start = it->readBuffer().start();
        size_t toscan = std::min(length, it->readAvailable());
        while (toscan > 0) {
            if (foundSoFar == 0) {
                const void *point = memchr(start, string[0], toscan);
                if (point != NULL) {
                    foundSoFar = 1;
                    size_t found = (unsigned char*)point -
                        (unsigned char*)start;
                    toscan -= found + 1;
                    length -= found + 1;
                    totalLength += found;
                    start = (unsigned char*)point + 1;
                } else {
                    totalLength += toscan;
                    length -= toscan;
                    toscan = 0;
                    continue;
                }
            }
            MORDOR_ASSERT(foundSoFar != 0);
            size_t tocompare = std::min(toscan, string.size() - foundSoFar);
            if (memcmp(start, string.c_str() + foundSoFar, tocompare) == 0) {
                foundSoFar += tocompare;
                toscan -= tocompare;
                length -= tocompare;
                if (foundSoFar == string.size())
                    break;
            } else {
                totalLength += foundSoFar;
                foundSoFar = 0;
            }
        }
        if (foundSoFar == string.size())
            break;
        if (length == 0)
            break;
    }
    if (foundSoFar == string.size())
        return totalLength;
    return -1;
}

std::string
Buffer::getDelimited(char delimiter, bool eofIsDelimiter, bool includeDelimiter)
{
    ptrdiff_t offset = find(delimiter, ~0);
    MORDOR_ASSERT(offset >= -1);
    if (offset == -1 && !eofIsDelimiter)
        MORDOR_THROW_EXCEPTION(UnexpectedEofException());
    eofIsDelimiter = offset == -1;
    if (offset == -1)
        offset = readAvailable();;
    std::string result;
    result.resize(offset + (eofIsDelimiter ? 0 : (includeDelimiter ? 1 : 0)));
    copyOut(&result[0], result.size());
    consume(result.size());
    if (!eofIsDelimiter && !includeDelimiter)
        consume(1u);
    return result;
}

std::string
Buffer::getDelimited(const std::string &delimiter, bool eofIsDelimiter,
    bool includeDelimiter)
{
    ptrdiff_t offset = find(delimiter, ~0);
    MORDOR_ASSERT(offset >= -1);
    if (offset == -1 && !eofIsDelimiter)
        MORDOR_THROW_EXCEPTION(UnexpectedEofException());
    eofIsDelimiter = offset == -1;
    if (offset == -1)
        offset = readAvailable();;
    std::string result;
    result.resize(offset + (eofIsDelimiter ? 0 :
        (includeDelimiter ? delimiter.size() : 0)));
    copyOut(&result[0], result.size());
    consume(result.size());
    if (!eofIsDelimiter && !includeDelimiter)
        consume(delimiter.size());
    return result;
}

void
Buffer::visit(boost::function<void (const void *, size_t)> dg, size_t length) const
{
    if (length == (size_t)~0)
        length = readAvailable();
    MORDOR_ASSERT(length <= readAvailable());

    std::list<Segment>::const_iterator it;
    for (it = m_segments.begin(); it != m_segments.end() && length > 0; ++it) {
        size_t todo = std::min(length, it->readAvailable());
        MORDOR_ASSERT(todo != 0);
        dg(it->readBuffer().start(), todo);
        length -= todo;
    }
    MORDOR_ASSERT(length == 0);
}

bool
Buffer::operator == (const Buffer &rhs) const
{
    if (rhs.readAvailable() != readAvailable())
        return false;
    return opCmp(rhs) == 0;
}

bool
Buffer::operator != (const Buffer &rhs) const
{
    if (rhs.readAvailable() != readAvailable())
        return true;
    return opCmp(rhs) != 0;
}

bool
Buffer::operator== (const std::string &string) const
{
    if (string.size() != readAvailable())
        return false;
    return opCmp(string.c_str(), string.size()) == 0;
}

bool
Buffer::operator!= (const std::string &string) const
{
    if (string.size() != readAvailable())
        return true;
    return opCmp(string.c_str(), string.size()) != 0;
}

bool
Buffer::operator== (const char *string) const
{
    size_t length = strlen(string);
    if (length != readAvailable())
        return false;
    return opCmp(string, length) == 0;
}

bool
Buffer::operator!= (const char *string) const
{
    size_t length = strlen(string);
    if (length != readAvailable())
        return true;
    return opCmp(string, length) != 0;
}

int
Buffer::opCmp(const Buffer &rhs) const
{
    std::list<Segment>::const_iterator leftIt, rightIt;
    int lengthResult = (int)((ptrdiff_t)readAvailable() - (ptrdiff_t)rhs.readAvailable());
    leftIt = m_segments.begin(); rightIt = rhs.m_segments.begin();
    size_t leftOffset = 0, rightOffset = 0;
    while (leftIt != m_segments.end() && rightIt != rhs.m_segments.end())
    {
        MORDOR_ASSERT(leftOffset <= leftIt->readAvailable());
        MORDOR_ASSERT(rightOffset <= rightIt->readAvailable());
        size_t tocompare = std::min(leftIt->readAvailable() - leftOffset,
            rightIt->readAvailable() - rightOffset);
        if (tocompare == 0)
            break;
        int result = memcmp(
            (const unsigned char *)leftIt->readBuffer().start() + leftOffset,
            (const unsigned char *)rightIt->readBuffer().start() + rightOffset,
            tocompare);
        if (result != 0)
            return result;
        leftOffset += tocompare;
        rightOffset += tocompare;
        if (leftOffset == leftIt->readAvailable()) {
            leftOffset = 0;
            ++leftIt;
        }
        if (rightOffset == rightIt->readAvailable()) {
            rightOffset = 0;
            ++rightIt;
        }
    }
    return lengthResult;
}

int
Buffer::opCmp(const char *string, size_t length) const
{
    size_t offset = 0;
    std::list<Segment>::const_iterator it;
    int lengthResult = (int)((ptrdiff_t)readAvailable() - (ptrdiff_t)length);
    if (lengthResult > 0)
        length = readAvailable();
    for (it = m_segments.begin(); it != m_segments.end(); ++it) {
        size_t tocompare = std::min(it->readAvailable(), length);
        int result = memcmp(it->readBuffer().start(), string + offset, tocompare);
        if (result != 0)
            return result;
        length -= tocompare;
        offset += tocompare;
        if (length == 0)
            return lengthResult;
    }
    return lengthResult;
}

void
Buffer::invariant() const
{
#ifdef DEBUG
    size_t read = 0;
    size_t write = 0;
    bool seenWrite = false;
    std::list<Segment>::const_iterator it;
    for (it = m_segments.begin(); it != m_segments.end(); ++it) {
        const Segment &segment = *it;
        // Strict ordering
        MORDOR_ASSERT(!seenWrite || (seenWrite && segment.readAvailable() == 0));
        read += segment.readAvailable();
        write += segment.writeAvailable();
        if (!seenWrite && segment.writeAvailable() != 0) {
            seenWrite = true;
            MORDOR_ASSERT(m_writeIt == it);
        }
        // We should keep segments optimally merged together
        std::list<Segment>::const_iterator nextIt = it;
        ++nextIt;
        if (nextIt != m_segments.end()) {
            const Segment& next = *nextIt;
            if (segment.writeAvailable() == 0 &&
                next.readAvailable() != 0) {
                MORDOR_ASSERT((const unsigned char*)segment.readBuffer().start() +
                    segment.readAvailable() != next.readBuffer().start() ||
                    segment.m_data.m_array.get() != next.m_data.m_array.get());
            } else if (segment.writeAvailable() != 0 &&
                next.readAvailable() == 0) {
                MORDOR_ASSERT((const unsigned char*)segment.writeBuffer().start() +
                    segment.writeAvailable() != next.writeBuffer().start() ||
                    segment.m_data.m_array.get() != next.m_data.m_array.get());
            }
        }
    }
    MORDOR_ASSERT(read == m_readAvailable);
    MORDOR_ASSERT(write == m_writeAvailable);
    MORDOR_ASSERT(write != 0 || (write == 0 && m_writeIt == m_segments.end()));
#endif
}

}
