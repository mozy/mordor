#ifndef __MORDOR_BUFFER_H__
#define __MORDOR_BUFFER_H__

#include <list>
#include <vector>

#include <boost/shared_array.hpp>
#include <boost/function.hpp>

#include "mordor/socket.h"

namespace Mordor {

struct Buffer
{
private:
    struct SegmentData
    {
        friend struct Buffer;
    public:
        SegmentData();
        SegmentData(size_t length);
        SegmentData(void *buffer, size_t length);

        SegmentData slice(size_t start, size_t length = ~0);
        const SegmentData slice(size_t start, size_t length = ~0) const;

        void extend(size_t len);

    public:
        void *start() { return m_start; }
        const void *start() const { return m_start; }
        size_t length() const { return m_length; }
    private:
        void start(void *p) { m_start = p; }
        void length(size_t l) { m_length = l; }
        void *m_start;
        size_t m_length;
    private:
        boost::shared_array<unsigned char> m_array;
    };

    struct Segment
    {
        friend struct Buffer;
    public:
        Segment(size_t len);
        Segment(SegmentData);
        Segment(void *buffer, size_t length);

        size_t readAvailable() const;
        size_t writeAvailable() const;
        size_t length() const;
        void produce(size_t length);
        void consume(size_t length);
        void truncate(size_t length);
        void extend(size_t length);
        const SegmentData readBuffer() const;
        const SegmentData writeBuffer() const;
        SegmentData writeBuffer();

    private:
        size_t m_writeIndex;
        SegmentData m_data;

        void invariant() const;
    };

public:
    Buffer();
    Buffer(const Buffer &copy);
    Buffer(const char *string);
    Buffer(const std::string &string);
    Buffer(const void *data, size_t length);

    Buffer &operator =(const Buffer &copy);

    size_t readAvailable() const;
    size_t writeAvailable() const;
    // Primarily for unit tests
    size_t segments() const;

    void adopt(void *buffer, size_t length);
    void reserve(size_t length);
    void compact();
    void clear(bool clearWriteAvailableAsWell = true);
    void produce(size_t length);
    void consume(size_t length);
    void truncate(size_t length);

    const std::vector<iovec> readBuffers(size_t length = ~0) const;
    const iovec readBuffer(size_t length, bool reallocate) const;
    std::vector<iovec> writeBuffers(size_t length = ~0);
    iovec writeBuffer(size_t length, bool reallocate);

    void copyIn(const Buffer& buf, size_t length = ~0);
    void copyIn(const char* string);
    void copyIn(const void* data, size_t length);

    void copyOut(Buffer &buffer, size_t length) const
    { buffer.copyIn(*this, length); }
    void copyOut(void* buffer, size_t length) const;

    ptrdiff_t find(char delimiter, size_t length = ~0) const;
    ptrdiff_t find(const std::string &string, size_t length = ~0) const;
    std::string getDelimited(char delimiter, bool eofIsDelimiter = true,
        bool includeDelimiter = true);
    std::string getDelimited(const std::string &delimiter,
        bool eofIsDelimiter = true, bool includeDelimiter = true);

    void visit(boost::function<void (const void *, size_t)> dg, size_t length = ~0) const;

    bool operator== (const Buffer &rhs) const;
    bool operator!= (const Buffer &rhs) const;
    bool operator== (const std::string &str) const;
    bool operator!= (const std::string &str) const;
    bool operator== (const char *str) const;
    bool operator!= (const char *str) const;

private:
    std::list<Segment> m_segments;
    size_t m_readAvailable;
    size_t m_writeAvailable;
    std::list<Segment>::iterator m_writeIt;

    int opCmp(const Buffer &rhs) const;
    int opCmp(const char *string, size_t length) const;

    void invariant() const;
};

}

#endif
