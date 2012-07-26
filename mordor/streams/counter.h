#ifndef __MORDOR_COUNTER_STREAM_H__
#define __MORDOR_COUNTER_STREAM_H__

#include "filter.h"

namespace Mordor {

class CounterStream : public FilterStream
{
public:
    typedef boost::shared_ptr<CounterStream> ptr;

public:
    CounterStream(Stream::ptr parent, bool own = true)
        : FilterStream(parent, own)
        , m_read(0)
        , m_written(0)
    {}

    using FilterStream::read;
    size_t read(Buffer &, size_t);
    using FilterStream::write;
    size_t write(const Buffer &, size_t);

    unsigned long long bytesRead() const { return m_read; }
    unsigned long long bytesWritten() const { return m_written; }

private:
    unsigned long long m_read, m_written;
};
}

#endif
