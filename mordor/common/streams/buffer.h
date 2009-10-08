#ifndef __MORDOR_BUFFER_H__
#define __MORDOR_BUFFER_H__

#include <list>
#include <vector>

#include <boost/shared_array.hpp>
#include <boost/function.hpp>

#include "mordor/common/socket.h"

namespace Mordor {

struct Buffer
{
public:
    struct SegmentData
    {
        friend struct Buffer;
    public:
        SegmentData();
        SegmentData(size_t length);

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

private:
    struct Segment
    {
        friend struct Buffer;
    public:
        Segment(size_t len);
        Segment(SegmentData);

        size_t readAvailable() const;
        size_t writeAvailable() const;
        size_t length() const;
        void produce(size_t len);
        void consume(size_t len);
        void truncate(size_t len);
        void extend(size_t len);
        const SegmentData readBuf() const;
        const SegmentData writeBuf() const;
        SegmentData writeBuf();

    private:
        size_t m_writeIndex;
        SegmentData m_data;

        void invariant() const;
    };

public:
    Buffer();
    Buffer(const Buffer &copy);
    Buffer(const char *str);
    Buffer(const std::string &str);
    Buffer(const void *data, size_t len);

    size_t readAvailable() const;
    size_t writeAvailable() const;
    // Primarily for unit tests
    size_t segments() const;

    void reserve(size_t len);
    void compact();
    void clear();
    void produce(size_t len);
    void consume(size_t len);
    void truncate(size_t len);

    const std::vector<iovec> readBufs(size_t len = ~0) const;
    const SegmentData readBuf(size_t len) const;
    std::vector<iovec> writeBufs(size_t len = ~0);
    SegmentData writeBuf(size_t len);

    void copyIn(const Buffer& buf, size_t len = ~0);
    void copyIn(const char* sz);
    void copyIn(const void* data, size_t len);

    void copyOut(void* buf, size_t len) const;

    ptrdiff_t find(char delim, size_t len = ~0) const;
    ptrdiff_t find(const std::string &str, size_t len = ~0) const;

    void visit(boost::function<void (const void *, size_t)> dg, size_t len = ~0) const;

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
    int opCmp(const char *str, size_t len) const;

    void invariant() const;
};

}

#endif
