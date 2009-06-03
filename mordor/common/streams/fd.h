#ifndef __FD_STREAM_H__
#define __FD_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "stream.h"

class FDStream : public Stream
{
protected:
    FDStream();
    void init(int fd, bool own = true);
public:
    FDStream(int fd, bool own = true);
    ~FDStream();

private:
    int m_fd;
    bool m_own;
};

#endif

