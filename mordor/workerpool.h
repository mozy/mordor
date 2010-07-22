#ifndef __MORDOR_WORKERPOOL_H__
#define __MORDOR_WORKERPOOL_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "scheduler.h"
#include "semaphore.h"

namespace Mordor {

/// Generic Scheduler

/// A WorkerPool is a generic Scheduler that does nothing when there is no work
/// to be done.
class WorkerPool : public Scheduler
{
public:
    WorkerPool(int threads = 1, bool useCaller = true, size_t batchSize = 1);
    ~WorkerPool() { stop(); }

protected:
    /// The idle Fiber for a WorkerPool simply loops waiting on a Semaphore,
    /// and yields whenever that Semaphore is signalled, returning if
    /// stopping() is true.
    void idle();
    /// Signals the semaphore so that the idle Fiber will yield.
    void tickle();

private:
    Semaphore m_semaphore;
};

}

#endif
