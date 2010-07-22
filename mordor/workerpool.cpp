// Copyright (c) 2009 - Mozy, Inc.

#include "workerpool.h"

#include "fiber.h"
#include "log.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:workerpool");

WorkerPool::WorkerPool(int threads, bool useCaller, size_t batchSize)
    : Scheduler(threads, useCaller, batchSize)
{
    start();
}

void
WorkerPool::idle()
{
    while (true) {
        if (stopping()) {
            return;
        }
        m_semaphore.wait();
        Fiber::yield();
    }
}

void
WorkerPool::tickle()
{
    MORDOR_LOG_DEBUG(g_log) << this << " tickling";
    m_semaphore.notify();
}

}
