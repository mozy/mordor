// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "fd.h"

#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include "mordor/exception.h"

namespace Mordor {

FDStream::FDStream()
: m_ioManager(NULL),
  m_scheduler(NULL),
  m_fd(-1),
  m_own(false)
{}

void
FDStream::init(IOManager *ioManager, Scheduler *scheduler, int fd, bool own)
{
    MORDOR_ASSERT(fd >= 0);
    m_ioManager = ioManager;
    m_scheduler = scheduler;
    m_fd = fd;
    m_own = own;
    if (m_ioManager) {
        try {
        if (fcntl(m_fd, F_SETFL, O_NONBLOCK))
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("fcntl");
        } catch(...) {
            if (own) {
                ::close(m_fd);
                m_fd = -1;
            }
            throw;
        }
    }
}

FDStream::~FDStream()
{
    if (m_own && m_fd >= 0) {
        SchedulerSwitcher switcher(m_scheduler);
        ::close(m_fd);
    }
}

void
FDStream::close(CloseType type)
{
    MORDOR_ASSERT(type == BOTH);
    if (m_fd > 0 && m_own) {
        SchedulerSwitcher switcher(m_scheduler);
        if (::close(m_fd))
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("close");
        m_fd = -1;
    }
}

size_t
FDStream::read(Buffer &b, size_t len)
{
    SchedulerSwitcher switcher(m_ioManager ? NULL : m_scheduler);
    MORDOR_ASSERT(m_fd >= 0);
    if (len > 0xfffffffe)
        len = 0xfffffffe;
    std::vector<iovec> bufs = b.writeBufs(len);
    int rc = readv(m_fd, &bufs[0], bufs.size());
    while (rc < 0 && errno == EAGAIN && m_ioManager) {
        m_ioManager->registerEvent(m_fd, IOManager::READ);
        Scheduler::getThis()->yieldTo();
        rc = readv(m_fd, &bufs[0], bufs.size());
    }
    if (rc < 0)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("readv");
    b.produce(rc);
    return rc;
}

size_t
FDStream::write(const Buffer &b, size_t len)
{
    SchedulerSwitcher switcher(m_ioManager ? NULL : m_scheduler);
    MORDOR_ASSERT(m_fd >= 0);
    if (len > 0xfffffffe)
        len = 0xfffffffe;
    const std::vector<iovec> bufs = b.readBufs(len);
    int rc = writev(m_fd, &bufs[0], bufs.size());
    while (rc < 0 && errno == EAGAIN && m_ioManager) {
        m_ioManager->registerEvent(m_fd, IOManager::WRITE);
        Scheduler::getThis()->yieldTo();
        rc = writev(m_fd, &bufs[0], bufs.size());
    }
    if (rc == 0) {
        MORDOR_THROW_EXCEPTION(std::runtime_error("Zero length write"));
    }
    if (rc < 0)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("writev");
    return rc;    
}

long long
FDStream::seek(long long offset, Anchor anchor)
{
    SchedulerSwitcher switcher(m_scheduler);
    MORDOR_ASSERT(m_fd >= 0);
    long long pos = lseek(m_fd, offset, (int)anchor);
    if (pos < 0)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("lseek");
    return pos;
}

long long
FDStream::size()
{
    SchedulerSwitcher switcher(m_scheduler);
    MORDOR_ASSERT(m_fd >= 0);
    struct stat statbuf;
    if (fstat(m_fd, &statbuf))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("fstat");
    return statbuf.st_size;
}

void
FDStream::truncate(long long size)
{
    SchedulerSwitcher switcher(m_scheduler);
    MORDOR_ASSERT(m_fd >= 0);
    if (ftruncate(m_fd, size))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("ftruncate");
}

void
FDStream::flush()
{
    SchedulerSwitcher switcher(m_scheduler);
    MORDOR_ASSERT(m_fd >= 0);
    if (fsync(m_fd))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("fsync");
}

}
