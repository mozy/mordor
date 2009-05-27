#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <list>
#include <vector>

#include <boost/shared_array.hpp>

struct Buffer
{
public:
    struct DataBuf
    {
    public:
        DataBuf();
        DataBuf(size_t length);

        DataBuf slice(size_t start, size_t length = ~0);
        const DataBuf slice(size_t start, size_t length = ~0) const;

        void*  m_start;
        size_t m_length;
    private:
        boost::shared_array<unsigned char> m_array;
    };

private:
    struct Data
    {
        Data(size_t len);
        Data(DataBuf);

        size_t readAvailable() const;
        size_t writeAvailable() const;
        size_t length() const;
        void produce(size_t len);
        void consume(size_t len);
        const DataBuf readBuf() const;
        DataBuf writeBuf();

    private:
        size_t m_writeIndex;
        DataBuf m_buf;

        void invariant() const;
    };

public:
    Buffer();
    size_t readAvailable() const;
    size_t writeAvailable() const;

    void reserve(size_t len);
    void compact();
    void clear();
    void produce(size_t len);
    void consume(size_t len);

    std::vector<const DataBuf> readBufs(size_t len = ~0) const;
    const DataBuf readBuf(size_t len) const;
    std::vector<DataBuf> writeBufs(size_t len = ~0);
    DataBuf writeBuf(size_t len);

    void copyIn(const Buffer& buf, size_t len = ~0);
    void copyIn(const char* sz);
    void copyIn(const void* data, size_t len);

    void copyOut(void* buf, size_t len) const;

    ptrdiff_t findDelimited(char delim, size_t len = ~0) const;

private:
    std::list<Data> m_bufs;
    size_t m_readAvailable;
    size_t m_writeAvailable;
    std::list<Data>::iterator m_writeIt;

    void invariant() const;
};

#endif
