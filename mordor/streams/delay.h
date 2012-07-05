#ifndef __MORDOR_DELAY_STREAM_H_
#define __MORDOR_DELAY_STREAM_H_

#include "mordor/sleep.h"
#include "mordor/streams/filter.h"

namespace Mordor {

/// Helper stream for unittest to force delay r/w.
class DelayStream: public FilterStream
{
public:
    typedef boost::shared_ptr<DelayStream> ptr;

    DelayStream(Stream::ptr parent, TimerManager * mgr = NULL,
            unsigned long long delay = 0ull, bool own = true):
        FilterStream(parent, own),
        m_delay(delay),
        m_p_mgr(mgr)
    {}

    using FilterStream::read;
    size_t read(Buffer &buffer, size_t length)
    {
        delay();
        return parent()->read(buffer, length);
    }

    using FilterStream::write;
    size_t write(const Buffer &buffer, size_t length)
    {
        delay();
        return parent()->write(buffer, length);
    }

protected:
    void delay()
    {
        if (m_delay > 0) {
            if (m_p_mgr)
                Mordor::sleep(*m_p_mgr, m_delay);
            else
                Mordor::sleep(m_delay);
        }
    }

private:
    unsigned long long m_delay;
    TimerManager * m_p_mgr;
};

}
#endif
