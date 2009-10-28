#ifndef __MORDOR_FD_STREAM_H__
#define __MORDOR_FD_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "mordor/iomanager.h"
#include "stream.h"

namespace Mordor {

class FDStream : public Stream
{
protected:
    FDStream();
    void init(IOManager *ioManager, Scheduler *scheduler, int fd, bool own = true);
public:
    FDStream(int fd, bool own = true)
    { init(NULL, NULL, fd, own); }
    FDStream(IOManager &ioManager, int fd, bool own = true)
    { init(&ioManager, NULL, fd, own); }
    FDStream(Scheduler &scheduler, int fd, bool own = true)
    { init(NULL, &scheduler, fd, own); }
    FDStream(IOManager &ioManager, Scheduler &scheduler, int fd, bool own = true)
    { init(&ioManager, &scheduler, fd, own); }
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

