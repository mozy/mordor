// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "fd.h"

#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include "mordor/exception.h"
#include "mordor/log.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:streams:fd");

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
        int rc = ::close(m_fd);
        MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this
            << " close(" << m_fd << "): " << rc << " (" << errno << ")";
    }
}

void
FDStream::close(CloseType type)
{
    MORDOR_ASSERT(type == BOTH);
    if (m_fd > 0 && m_own) {
        SchedulerSwitcher switcher(m_scheduler);
        int rc = ::close(m_fd);
        int error = errno;
        MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this
            << " close(" << m_fd << "): " << rc << " (" << error << ")";
        if (rc)
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "close");
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
        MORDOR_LOG_TRACE(g_log) << this << " readv(" << m_fd << ", " << length
            << "): " << rc << " (EAGAIN)";
        m_ioManager->registerEvent(m_fd, IOManager::READ);
        Scheduler::yieldTo();
        rc = readv(m_fd, &iovs[0], iovs.size());
    }
    int error = errno;
    MORDOR_LOG_LEVEL(g_log, rc < 0 ? Log::ERROR : Log::DEBUG) << this
        << " readv(" << m_fd << ", " << length << "): " << rc << " (" << error
        << ")";
    if (rc < 0)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "readv");
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
        MORDOR_LOG_TRACE(g_log) << this << " read(" << m_fd << ", " << length
            << "): " << rc << " (EAGAIN)";
        m_ioManager->registerEvent(m_fd, IOManager::READ);
        Scheduler::yieldTo();
        rc = ::read(m_fd, buffer, length);
    }
    int error = errno;
    MORDOR_LOG_LEVEL(g_log, rc < 0 ? Log::ERROR : Log::DEBUG) << this
        << " read(" << m_fd << ", " << length << "): " << rc << " (" << error
        << ")";
    if (rc < 0)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "read");
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
        MORDOR_LOG_TRACE(g_log) << this << " writev(" << m_fd << ", " << length
            << "): " << rc << " (EAGAIN)";
        m_ioManager->registerEvent(m_fd, IOManager::WRITE);
        Scheduler::yieldTo();
        rc = writev(m_fd, &iovs[0], iovs.size());
    }
    int error = errno;
    MORDOR_LOG_LEVEL(g_log, rc < 0 ? Log::ERROR : Log::DEBUG) << this
        << " writev(" << m_fd << ", " << length << "): " << rc << " (" << error
        << ")";
    if (rc == 0)
        MORDOR_THROW_EXCEPTION(std::runtime_error("Zero length write"));
    if (rc < 0)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "writev");
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
        MORDOR_LOG_TRACE(g_log) << this << " write(" << m_fd << ", " << length
            << "): " << rc << " (EAGAIN)";
        m_ioManager->registerEvent(m_fd, IOManager::WRITE);
        Scheduler::yieldTo();
        rc = ::write(m_fd, buffer, length);
    }
    int error = errno;
    MORDOR_LOG_LEVEL(g_log, rc < 0 ? Log::ERROR : Log::DEBUG) << this
        << " write(" << m_fd << ", " << length << "): " << rc << " (" << error
        << ")";
    if (rc == 0)
        MORDOR_THROW_EXCEPTION(std::runtime_error("Zero length write"));
    if (rc < 0)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "write");
    return rc;
}

long long
FDStream::seek(long long offset, Anchor anchor)
{
    SchedulerSwitcher switcher(m_scheduler);
    MORDOR_ASSERT(m_fd >= 0);
    long long pos = lseek(m_fd, offset, (int)anchor);
    int error = errno;
    MORDOR_LOG_LEVEL(g_log, pos < 0 ? Log::ERROR : Log::VERBOSE) << this
        << " lseek(" << m_fd << ", " << offset << ", " << anchor << "): "
        << pos << " (" << error << ")";
    if (pos < 0)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "lseek");
    return pos;
}

long long
FDStream::size()
{
    SchedulerSwitcher switcher(m_scheduler);
    MORDOR_ASSERT(m_fd >= 0);
    struct stat statbuf;
    int rc = fstat(m_fd, &statbuf);
    int error = errno;
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this
        << " fstat(" << m_fd << "): " << rc << " (" << error << ")";
    if (rc)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "fstat");
    return statbuf.st_size;
}

void
FDStream::truncate(long long size)
{
    SchedulerSwitcher switcher(m_scheduler);
    MORDOR_ASSERT(m_fd >= 0);
    int rc = ftruncate(m_fd, size);
    int error = errno;
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this
        << " ftruncate(" << m_fd << ", " << size << "): " << rc
        << " (" << error << ")";
    if (rc)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "ftruncate");
}

void
FDStream::flush(bool flushParent)
{
    SchedulerSwitcher switcher(m_scheduler);
    MORDOR_ASSERT(m_fd >= 0);
    int rc = fsync(m_fd);
    int error = errno;
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::VERBOSE) << this
        << " fsync(" << m_fd << "): " << rc << " (" << error << ")";
    if (rc)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "fsync");
}

}
