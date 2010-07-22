// Copyright (c) 2009 - Mozy, Inc.

#include "socket.h"

#include "buffer.h"
#include "mordor/assert.h"
#include "mordor/socket.h"

namespace Mordor {

SocketStream::SocketStream(Socket::ptr socket, bool own)
: m_socket(socket),
  m_own(own)
{
    MORDOR_ASSERT(socket);
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
    }
}

size_t
SocketStream::read(Buffer &buffer, size_t length)
{
    std::vector<iovec> iovs = buffer.writeBuffers(length);
    size_t result = m_socket->receive(&iovs[0], iovs.size());
    buffer.produce(result);
    return result;
}

size_t
SocketStream::read(void *buffer, size_t length)
{
    return m_socket->receive(buffer, length);
}

void
SocketStream::cancelRead()
{
    m_socket->cancelReceive();
}

size_t
SocketStream::write(const Buffer &buffer, size_t length)
{
    const std::vector<iovec> iovs = buffer.readBuffers(length);
    size_t result = m_socket->send(&iovs[0], iovs.size());
    MORDOR_ASSERT(result > 0);
    return result;
}

size_t
SocketStream::write(const void *buffer, size_t length)
{
    return m_socket->send(buffer, length);
}

void
SocketStream::cancelWrite()
{
    m_socket->cancelSend();
}

}
