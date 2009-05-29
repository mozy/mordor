// Copyright (c) 2009 - Decho Corp.

#include "socket.h"

#include <cassert>

#include "common/socket.h"

SocketStream::SocketStream(Socket *s, bool own)
: m_socket(s),
  m_own(own)
{
    assert(s);
}

SocketStream::~SocketStream()
{
    if (m_socket && m_own) {
        delete m_socket;
    }
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
            Socket *s = m_socket;
            m_socket = NULL;
            delete s;
        }
    } else if (type == BOTH) {
        m_socket = NULL;
    }
}

size_t
SocketStream::read(Buffer *b, size_t len)
{
    assert(m_socket);
    if (len == 0)
        return 0;
    std::vector<Buffer::DataBuf> bufs = b->writeBufs(len);
    size_t result = m_socket->receive((iovec *)&bufs[0], bufs.size());
    b->produce(result);
    return result;
}

size_t
SocketStream::write(const Buffer *b, size_t len)
{
    assert(m_socket);
    if (len == 0)
        return 0;
    std::vector<const Buffer::DataBuf> bufs = b->readBufs(len);
    size_t result = m_socket->receive((const iovec *)&bufs[0], bufs.size());
    assert(result > 0);
    return result;
}
