#ifndef __MORDOR_FD_STREAM_H__
#define __MORDOR_FD_STREAM_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "stream.h"

namespace Mordor {

class IOManager;
class Scheduler;

class FDStream : public Stream
{
public:
    typedef boost::shared_ptr<FDStream> ptr;

protected:
    FDStream();
    void init(int fd, IOManager *ioManager = NULL, Scheduler *scheduler = NULL,
        bool own = true);
public:
    FDStream(int fd, IOManager *ioManager = NULL, Scheduler *scheduler = NULL,
        bool own = true)
    { init(fd, ioManager, scheduler, own); }
    ~FDStream();

    bool supportsRead() { return true; }
    bool supportsWrite() { return true; }
    bool supportsSeek() { return true; }
    bool supportsSize() { return true; }
    bool supportsTruncate() { return true; }

    void close(CloseType type = BOTH);
    size_t read(Buffer &buffer, size_t length);
    size_t read(void *buffer, size_t length);
    void cancelRead();
    size_t write(const Buffer &buffer, size_t length);
    size_t write(const void *buffer, size_t length);
    void cancelWrite();
    long long seek(long long offset, Anchor anchor);
    long long size();
    void truncate(long long size);
    void flush(bool flushParent = true);

    int fd() { return m_fd; }

private:
    IOManager *m_ioManager;
    Scheduler *m_scheduler;
    int m_fd;
    bool m_own, m_cancelledRead, m_cancelledWrite;
};

typedef FDStream NativeStream;
typedef int NativeHandle;

}

#endif

