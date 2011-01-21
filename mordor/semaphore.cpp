// Copyright (c) 2009 - Mozy, Inc.

#include "semaphore.h"

#include "assert.h"
#include "exception.h"

#ifndef WINDOWS
#include <errno.h>
#endif

#ifdef FREEBSD
#include <sys/sem.h>
#endif

#ifdef OSX
#include <mach/mach_init.h>
#include <mach/task.h>
#endif

namespace Mordor {

Semaphore::Semaphore(unsigned int count)
{
#ifdef WINDOWS
    m_semaphore = CreateSemaphore(NULL, count, 2147483647, NULL);
    if (m_semaphore == NULL) {
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateSemaphore");
    }
#elif defined(OSX)
    m_task = mach_task_self();
    if (semaphore_create(m_task, &m_semaphore, SYNC_POLICY_FIFO, count)) {
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("semaphore_create");
    }
#elif defined(FREEBSD)
    m_semaphore = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
    if (m_semaphore < 0) {
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("semget");
    }
    semun init;
    init.val = count;
    if (semctl(m_semaphore, 0, SETVAL, init) < 0) {
        semctl(m_semaphore, 0, IPC_RMID);
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("semctl");
    }
#else
    if (sem_init(&m_semaphore, 0, count)) {
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("sem_init");
    }
#endif
}

Semaphore::~Semaphore()
{
#ifdef WINDOWS
    MORDOR_VERIFY(CloseHandle(m_semaphore));
#elif defined(OSX)
    MORDOR_VERIFY(!semaphore_destroy(m_task, m_semaphore));
#elif defined(FREEBSD)
    MORDOR_VERIFY(semctl(m_semaphore, 0, IPC_RMID) >= 0);
#else
    MORDOR_VERIFY(!sem_destroy(&m_semaphore));
#endif
}

void
Semaphore::wait()
{
#ifdef WINDOWS
    DWORD dwRet = WaitForSingleObject(m_semaphore, INFINITE);
    if (dwRet != WAIT_OBJECT_0) {
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WaitForSingleObject");
    }
#elif defined(OSX)
    while (true) {
        if (!semaphore_wait(m_semaphore))
            return;
        if (errno != EINTR) {
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("semaphore_wait");
        }
    }
#elif defined(FREEBSD)
    sembuf op;
    op.sem_num = 0;
    op.sem_op = -1;
    op.sem_flg = 0;
    while (true) {
        if (!semop(m_semaphore, &op, 1))
            return;
        if (errno != EINTR) {
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("semop");
        }
    }
#else
    while (true) {
        if (!sem_wait(&m_semaphore))
            return;
        if (errno != EINTR) {
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("sem_wait");
        }
    }
#endif
}

void
Semaphore::notify()
{
#ifdef WINDOWS
    if (!ReleaseSemaphore(m_semaphore, 1, NULL)) {
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("ReleaseSemaphore");
    }
#elif defined(OSX)
    semaphore_signal(m_semaphore);
#elif defined(FREEBSD)
    sembuf op;
    op.sem_num = 0;
    op.sem_op = 1;
    op.sem_flg = 0;
    if (semop(m_semaphore, &op, 1)) {
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("semop");
    }
#else
    if (sem_post(&m_semaphore)) {
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("sem_post");
    }
#endif
}

}
