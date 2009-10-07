// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "fd.h"

#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include "mordor/common/exception.h"

FDStream::FDStream()
: m_ioManager(NULL),
  m_fd(-1),
  m_own(false)
{}

void
FDStream::init(int fd, bool own)
{
    ASSERT(fd >= 0);
    m_fd = fd;
    m_own = own;
}

void
FDStream::init(IOManager *ioManager, int fd, bool own)
{
    init(fd, own);
    m_ioManager = ioManager;
}

FDStream::FDStream(int fd, bool own)
: m_ioManager(NULL), m_fd(fd), m_own(own)
{
    ASSERT(m_fd >= 0);
}

FDStream::FDStream(IOManager &ioManager, int fd, bool own)
: m_ioManager(&ioManager), m_fd(fd), m_own(own)
{
    ASSERT(m_fd >= 0);
    try {
        if (fcntl(m_fd, F_SETFL, O_NONBLOCK))
            THROW_EXCEPTION_FROM_LAST_ERROR_API("fcntl");
    } catch(...) {
        if (own) {
            ::close(m_fd);
        }
        throw;
    }
}

FDStream::~FDStream()
{
    if (m_own && m_fd >= 0) {
        ::close(m_fd);
    }
}

void
FDStream::close(CloseType type)
{
    if (type == BOTH && m_fd > 0) {
        if (::close(m_fd)) {
            THROW_EXCEPTION_FROM_LAST_ERROR_API("close");
        }
        m_fd = -1;
    }
}

size_t
FDStream::read(Buffer &b, size_t len)
{
    ASSERT(m_fd >= 0);
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
        THROW_EXCEPTION_FROM_LAST_ERROR_API("readv");
    b.produce(rc);
    return rc;
}

size_t
FDStream::write(const Buffer &b, size_t len)
{
    ASSERT(m_fd >= 0);
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
        THROW_EXCEPTION_FROM_LAST_ERROR_API("writev");
    return rc;    
}

long long
FDStream::seek(long long offset, Anchor anchor)
{
    ASSERT(m_fd >= 0);
    long long pos = lseek(m_fd, offset, (int)anchor);
    if (pos < 0)
        THROW_EXCEPTION_FROM_LAST_ERROR_API("lseek");
    return pos;
}

long long
FDStream::size()
{
    ASSERT(m_fd >= 0);
    struct stat statbuf;
    if (fstat(m_fd, &statbuf))
        THROW_EXCEPTION_FROM_LAST_ERROR_API("fstat");
    return statbuf.st_size;
}

void
FDStream::truncate(long long size)
{
    ASSERT(m_fd >= 0);
    if (ftruncate(m_fd, size))
        THROW_EXCEPTION_FROM_LAST_ERROR_API("ftruncate");
}

void
FDStream::flush()
{
    ASSERT(m_fd >= 0);
    if (fsync(m_fd))
        THROW_EXCEPTION_FROM_LAST_ERROR_API("fsync");
}
