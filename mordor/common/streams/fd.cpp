// Copyright (c) 2009 - Decho Corp.

#include "fd.h"

#include <sys/fcntl.h>
#include <sys/uio.h>

#include "common/exception.h"

FDStream::FDStream()
: m_ioManager(NULL),
  m_fd(-1),
  m_own(false)
{}

void
FDStream::init(int fd, bool own)
{
    assert(fd > 0);
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
    assert(m_fd > 0);
}

FDStream::FDStream(IOManager &ioManager, int fd, bool own)
: m_ioManager(&ioManager), m_fd(fd), m_own(own)
{
    assert(m_fd > 0);
    try {
        if (fcntl(m_fd, F_SETFL, O_NONBLOCK)) {
            throwExceptionFromLastError();
        }
    } catch(...) {
        if (own) {
            ::close(m_fd);
        }
        throw;
    }
}

FDStream::~FDStream()
{
    if (m_own && m_fd > 0) {
        ::close(m_fd);
    }
}

void
FDStream::close(CloseType type)
{
    if (type == BOTH && m_fd > 0) {
        if (::close(m_fd)) {
            throwExceptionFromLastError();
        }
        m_fd = -1;
    }
}

size_t
FDStream::read(Buffer &b, size_t len)
{
    assert(m_fd > 0);
    if (len > 0xfffffffe)
        len = 0xfffffffe;
    std::vector<Buffer::DataBuf> bufs = b.writeBufs(len);
    int rc = readv(m_fd, (iovec *)&bufs[0], bufs.size());
    while (rc < 0 && errno == EAGAIN && m_ioManager) {
        m_ioManager->registerEvent(m_fd, IOManager::READ);
        Scheduler::getThis()->yieldTo();
        rc = readv(m_fd, (iovec *)&bufs[0], bufs.size());
    }
    if (rc < 0) {
        throwExceptionFromLastError();
    }
    b.produce(rc);
    return rc;
}

size_t
FDStream::write(const Buffer &b, size_t len)
{
    assert(m_fd > 0);
    if (len > 0xfffffffe)
        len = 0xfffffffe;
    const std::vector<Buffer::DataBuf> bufs = b.readBufs(len);
    int rc = writev(m_fd, (iovec *)&bufs[0], bufs.size());
    while (rc < 0 && errno == EAGAIN && m_ioManager) {
        m_ioManager->registerEvent(m_fd, IOManager::WRITE);
        Scheduler::getThis()->yieldTo();
        rc = writev(m_fd, (iovec *)&bufs[0], bufs.size());
    }
    if (rc == 0) {
        throw std::runtime_error("Zero length write");
    }
    if (rc < 0) {
        throwExceptionFromLastError();
    }
    return rc;    
}

long long
FDStream::seek(long long offset, Anchor anchor)
{
    long long pos = lseek64(m_fd, offset, (int)anchor);
    if (pos < 0) {
        throwExceptionFromLastError();
    }
    return pos;
}

long long
FDStream::size()
{
    struct stat64 statbuf;
    if (fstat64(m_fd, &statbuf)) {
        throwExceptionFromLastError();
    }
    return statbuf.st_size;
}

void
FDStream::truncate(long long size)
{
    if (ftruncate64(m_fd, size)) {
        throwExceptionFromLastError();
    }
}

void
FDStream::flush()
{
    if (fsync(m_fd)) {
        throwExceptionFromLastError();
    }
}
