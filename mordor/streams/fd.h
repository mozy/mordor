#ifndef __MORDOR_FD_STREAM_H__
#define __MORDOR_FD_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "mordor/iomanager.h"
#include "stream.h"

namespace Mordor {

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
    size_t read(Buffer &b, size_t len);
    size_t write(const Buffer &b, size_t len);
    long long seek(long long offset, Anchor anchor);
    long long size();
    void truncate(long long size);
    void flush();

    int fd() { return m_fd; }

private:
    IOManager *m_ioManager;
    Scheduler *m_scheduler;
    int m_fd;
    bool m_own;
};

typedef FDStream NativeStream;
typedef int NativeHandle;

}

#endif

