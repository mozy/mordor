#ifndef __SOCKET_STREAM_H__
#define __SOCKET_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "stream.h"

class Socket;

class SocketStream : public Stream
{
public:
    SocketStream(Socket *s, bool own = true);
    ~SocketStream();

    bool supportsRead() { return true; }
    bool supportsWrite() { return true; }

    void close(CloseType type = BOTH);

    size_t read(Buffer *b, size_t len);
    size_t write(Buffer *b, size_t len);

private:
    Socket *m_socket;
    bool m_own;
};

#endif