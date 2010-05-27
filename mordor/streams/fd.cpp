// Copyright (c) 2009 - Mozy, Inc.

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
FDStream::init(int fd, IOManager *ioManager, Scheduler *scheduler, bool own)
{
    MORDOR_ASSERT(fd >= 0);
    m_ioManager = ioManager;
    m_scheduler = scheduler;
    m_fd = fd;
    m_own = own;
    if (m_ioManager) {
        if (fcntl(m_fd, F_SETFL, O_NONBLOCK)) {
            int error = errno;
            if (own) {
                ::close(m_fd);
                m_fd = -1;
            }
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "fcntl");
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
FDStream::read(Buffer &buffer, size_t length)
{
    SchedulerSwitcher switcher(m_ioManager ? NULL : m_scheduler);
    MORDOR_ASSERT(m_fd >= 0);
    if (length > 0xfffffffe)
        length = 0xfffffffe;
    std::vector<iovec> iovs = buffer.writeBuffers(length);
    int rc = readv(m_fd, &iovs[0], iovs.size());
    while (rc < 0 && errno == EAGAIN && m_ioManager) {
        m_ioManager->registerEvent(m_fd, IOManager::READ);
        Scheduler::yieldTo();
        rc = readv(m_fd, &iovs[0], iovs.size());
    }
    if (rc < 0)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("readv");
    buffer.produce(rc);
    return rc;
}

size_t
FDStream::read(void *buffer, size_t length)
{
    SchedulerSwitcher switcher(m_ioManager ? NULL : m_scheduler);
    MORDOR_ASSERT(m_fd >= 0);
    if (length > 0xfffffffe)
        length = 0xfffffffe;
    int rc = ::read(m_fd, buffer, length);
    while (rc < 0 && errno == EAGAIN && m_ioManager) {
        m_ioManager->registerEvent(m_fd, IOManager::READ);
        Scheduler::yieldTo();
        rc = ::read(m_fd, buffer, length);
    }
    if (rc < 0)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("read");
    return rc;
}

size_t
FDStream::write(const Buffer &buffer, size_t length)
{
    SchedulerSwitcher switcher(m_ioManager ? NULL : m_scheduler);
    MORDOR_ASSERT(m_fd >= 0);
    if (length > 0xfffffffe)
        length = 0xfffffffe;
    const std::vector<iovec> iovs = buffer.readBuffers(length);
    int rc = writev(m_fd, &iovs[0], iovs.size());
    while (rc < 0 && errno == EAGAIN && m_ioManager) {
        m_ioManager->registerEvent(m_fd, IOManager::WRITE);
        Scheduler::yieldTo();
        rc = writev(m_fd, &iovs[0], iovs.size());
    }
    if (rc == 0)
        MORDOR_THROW_EXCEPTION(std::runtime_error("Zero length write"));
    if (rc < 0)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("writev");
    return rc;
}

size_t
FDStream::write(const void *buffer, size_t length)
{
    SchedulerSwitcher switcher(m_ioManager ? NULL : m_scheduler);
    MORDOR_ASSERT(m_fd >= 0);
    if (length > 0xfffffffe)
        length = 0xfffffffe;
    int rc = ::write(m_fd, buffer, length);
    while (rc < 0 && errno == EAGAIN && m_ioManager) {
        m_ioManager->registerEvent(m_fd, IOManager::WRITE);
        Scheduler::yieldTo();
        rc = ::write(m_fd, buffer, length);
    }
    if (rc == 0)
        MORDOR_THROW_EXCEPTION(std::runtime_error("Zero length write"));
    if (rc < 0)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("write");
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
FDStream::flush(bool flushParent)
{
    SchedulerSwitcher switcher(m_scheduler);
    MORDOR_ASSERT(m_fd >= 0);
    if (fsync(m_fd))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("fsync");
}

}
