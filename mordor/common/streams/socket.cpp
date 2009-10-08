// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "socket.h"

#include "mordor/common/assert.h"
#include "mordor/common/socket.h"

namespace Mordor {

SocketStream::SocketStream(boost::shared_ptr<Socket> s, bool own)
: m_socket(s),
  m_own(own)
{
    MORDOR_ASSERT(s);
}

void
SocketStream::close(CloseType type)
{
    if (m_socket && m_own) {
        int how;
        switch(type) {
            case READ:
                how = SHUT_RD;
                break;
            case WRITE:
                how = SHUT_WR;
                break;
            default:
                how = SHUT_RDWR;
                break;
        }
        m_socket->shutdown(how);
        if (how == SHUT_RDWR) {
            m_socket->close();
            m_socket.reset();
        }
    } else if (type == BOTH) {
        m_socket.reset();
    }
}

size_t
SocketStream::read(Buffer &b, size_t len)
{
    MORDOR_ASSERT(m_socket);
    if (len == 0)
        return 0;
    std::vector<iovec> bufs = b.writeBufs(len);
    size_t result = m_socket->receive(&bufs[0], bufs.size());
    b.produce(result);
    return result;
}

size_t
SocketStream::write(const Buffer &b, size_t len)
{
    MORDOR_ASSERT(m_socket);
    if (len == 0)
        return 0;
    const std::vector<iovec> bufs = b.readBufs(len);
    size_t result = m_socket->send(&bufs[0], bufs.size());
    MORDOR_ASSERT(result > 0);
    return result;
}

}
