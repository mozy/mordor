#ifndef __FD_STREAM_H__
#define __FD_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/iomanager.h"
#include "stream.h"

class FDStream : public Stream
{
protected:
    FDStream();
    void init(int fd, bool own = true);
    void init(IOManager *ioManager, int fd, bool own = true);
public:
    FDStream(int fd, bool own = true);
    FDStream(IOManager &ioManager, int fd, bool own = true);
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
    int m_fd;
    bool m_own;
};

#endif

