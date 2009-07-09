#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <list>
#include <vector>

#include <boost/shared_array.hpp>

#include "mordor/common/socket.h"

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
    Buffer(const Buffer &copy);
    Buffer(const char *str);
    Buffer(const std::string &str);
    Buffer(const void *data, size_t len);

    size_t readAvailable() const;
    size_t writeAvailable() const;

    void reserve(size_t len);
    void compact();
    void clear();
    void produce(size_t len);
    void consume(size_t len);

    const std::vector<iovec> readBufs(size_t len = ~0) const;
    const DataBuf readBuf(size_t len) const;
    std::vector<iovec> writeBufs(size_t len = ~0);
    DataBuf writeBuf(size_t len);

    void copyIn(const Buffer& buf, size_t len = ~0);
    void copyIn(const char* sz);
    void copyIn(const void* data, size_t len);

    void copyOut(void* buf, size_t len) const;

    ptrdiff_t find(char delim, size_t len = ~0) const;
    ptrdiff_t find(const std::string &str, size_t len = ~0) const;

    bool operator== (const std::string &str) const;
    bool operator!= (const std::string &str) const;
    bool operator== (const char *str) const;
    bool operator!= (const char *str) const;

private:
    std::list<Data> m_bufs;
    size_t m_readAvailable;
    size_t m_writeAvailable;
    std::list<Data>::iterator m_writeIt;

    int opCmp(const char *str, size_t len) const;

    void invariant() const;
};

#endif
