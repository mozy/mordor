#include "counter.h"

namespace Mordor {

size_t
CounterStream::read(Buffer &b, size_t len)
{
    size_t ret = parent()->read(b, len);
    m_read += ret;
    return ret;
}

size_t
CounterStream::write(const Buffer &b, size_t len)
{
    size_t ret = parent()->write(b, len);
    m_written += ret;
    return ret;
}

}
