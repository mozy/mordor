#ifndef __MORDOR_SOCKET_STREAM_H__
#define __MORDOR_SOCKET_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "stream.h"

namespace Mordor {

class Socket;

class SocketStream : public Stream
{
public:
    typedef boost::shared_ptr<SocketStream> ptr;

public:
    SocketStream(boost::shared_ptr<Socket> s, bool own = true);

    bool supportsRead() { return true; }
    bool supportsWrite() { return true; }

    void close(CloseType type = BOTH);

    size_t read(Buffer &b, size_t len);
    size_t write(const Buffer &b, size_t len);

    boost::shared_ptr<Socket> socket() { return m_socket; }

private:
    boost::shared_ptr<Socket> m_socket;
    bool m_own;
};

}

#endif

