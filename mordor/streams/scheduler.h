#ifndef __MORDOR_SCHEDULER_STREAM_H__
#define __MORDOR_SCHEDULER_STREAM_H__
// Copyright (c) 2009 - Decho Corporation

#include "filter.h"
#include "mordor/scheduler.h"

namespace Mordor {

class SchedulerStream : public FilterStream
{
public:
    SchedulerStream(Stream::ptr parent, bool own = true)
        : FilterStream(parent, own),
          m_closeScheduler(NULL),
          m_ioScheduler(NULL),
          m_seekScheduler(NULL),
          m_sizeScheduler(NULL),
          m_truncateScheduler(NULL),
          m_flushScheduler(NULL)
    {}

    Scheduler *closeScheduler() { return m_closeScheduler; }
    void closeScheduler(Scheduler *scheduler) { m_closeScheduler = scheduler; }
    Scheduler *ioScheduler() { return m_ioScheduler; }
    void ioScheduler(Scheduler *scheduler) { m_ioScheduler = scheduler; }
    Scheduler *seekScheduler() { return m_seekScheduler; }
    void seekScheduler(Scheduler *scheduler) { m_seekScheduler = scheduler; }
    Scheduler *sizeScheduler() { return m_sizeScheduler; }
    void sizeScheduler(Scheduler *scheduler) { m_sizeScheduler = scheduler; }
    Scheduler *truncateScheduler() { return m_truncateScheduler; }
    void truncateScheduler(Scheduler *scheduler) { m_truncateScheduler = scheduler; }
    Scheduler *flushScheduler() { return m_flushScheduler; }
    void flushScheduler(Scheduler *scheduler) { m_flushScheduler = scheduler; }

    void close(CloseType type = BOTH)
    {
        if (ownsParent()) {
            SchedulerSwitcher switcher(m_closeScheduler);
            parent()->close(type);
        }
    }
    size_t read(Buffer &b, size_t len)
    {
        SchedulerSwitcher switcher(m_ioScheduler);
        return parent()->read(b, len);
    }
    size_t write(const Buffer &b, size_t len)
    {
        SchedulerSwitcher switcher(m_ioScheduler);
        return parent()->write(b, len);
    }
    long long seek(long long offset, Anchor anchor = BEGIN)
    {
        SchedulerSwitcher switcher(m_seekScheduler);
        return parent()->seek(offset, anchor);
    }
    long long size()
    {
        SchedulerSwitcher switcher(m_sizeScheduler);
        return parent()->size();
    }
    void truncate(long long size)
    {
        SchedulerSwitcher switcher(m_truncateScheduler);
        parent()->truncate(size);
    }
    void flush(bool flushParent = true)
    {
        SchedulerSwitcher switcher(m_flushScheduler);
        parent()->flush(flushParent);
    }

private:
    Scheduler *m_closeScheduler;
    Scheduler *m_ioScheduler;
    Scheduler *m_seekScheduler;
    Scheduler *m_sizeScheduler;
    Scheduler *m_truncateScheduler;
    Scheduler *m_flushScheduler;
};

}

#endif
