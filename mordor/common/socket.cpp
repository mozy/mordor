// Copyright (c) 2009 - Decho Corp.

#include "socket.h"

#include "exception.h"
#include "version.h"

#ifdef WINDOWS
#include <mswsock.h>

#pragma comment(lib, "ws2_32")

static LPFN_ACCEPTEX pAcceptEx;
static LPFN_CONNECTEX ConnectEx;

struct SocketInitializer {
    SocketInitializer()
    {
        WSADATA wd;
        WSAStartup(0x0101, &wd);

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        DWORD bytes = 0;

        GUID acceptExGuid = WSAID_ACCEPTEX;
        WSAIoctl(sock,
                 SIO_GET_EXTENSION_FUNCTION_POINTER,
                 &acceptExGuid,
                 sizeof(GUID),
                 &pAcceptEx,
                 sizeof(LPFN_ACCEPTEX),
                 &bytes,
                 NULL,
                 NULL);
        
        GUID connectExGuid = WSAID_CONNECTEX;
        WSAIoctl(sock,
                 SIO_GET_EXTENSION_FUNCTION_POINTER,
                 &connectExGuid,
                 sizeof(GUID),
                 &ConnectEx,
                 sizeof(LPFN_CONNECTEX),
                 &bytes,
                 NULL,
                 NULL);
        closesocket(sock);        
    }
    ~SocketInitializer()
    {
        WSACleanup();
    }
};

static SocketInitializer g_init;

#else
#include <netdb.h>
#define closesocket close
#endif

Socket::Socket(IOManager *ioManager, int family, int type, int protocol, int initialize)
: m_sock(-1),
  m_family(family),
  m_protocol(protocol),
  m_ioManager(ioManager)
{
#ifdef WINDOWS
    if (m_ioManager) {
        m_sock = socket(family, type, protocol);
        if (m_sock == -1) {
            throwExceptionFromLastError();
        }
    }
#endif
}

Socket::Socket(int family, int type, int protocol)
: m_sock(-1),
  m_family(family),
  m_protocol(protocol),
  m_ioManager(NULL)
{
    m_sock = socket(family, type, protocol);
    if (m_sock == -1) {
        throwExceptionFromLastError();
    }
}

Socket::Socket(IOManager &ioManager, int family, int type, int protocol)
: m_sock(-1),
  m_family(family),
  m_protocol(protocol),
  m_ioManager(&ioManager)
{
    m_sock = socket(family, type, protocol);
    if (m_sock == -1) {
        throwExceptionFromLastError();
    }
#ifdef WINDOWS
    try {
        m_ioManager->registerFile((HANDLE)m_sock);
    } catch(...) {
        closesocket(m_sock);
        throw;
    }
#endif
}

Socket::~Socket()
{
    close();
}

void
Socket::bind(const Address &addr)
{
    assert(addr.family() == m_family);
    if (::bind(m_sock, addr.name(), addr.nameLen())) {
        throwExceptionFromLastError();
    }
}

void
Socket::connect(const Address &to)
{
    assert(to.family() == m_family);
    if (!m_ioManager) {
        if (::connect(m_sock, to.name(), to.nameLen())) {
            throwExceptionFromLastError();
        }
    } else {
#ifdef WINDOWS
        // need to be bound, even to ADDR_ANY, before calling ConnectEx
        switch (m_family) {
            case AF_INET:
                {
                    sockaddr_in addr;
                    addr.sin_family = AF_INET;
                    addr.sin_port = 0;
                    addr.sin_addr.s_addr = ADDR_ANY;
                    if(::bind(m_sock, (sockaddr*)&addr, sizeof(sockaddr_in))) {
                        throwExceptionFromLastError();
                    }
                    break;
                }
            case AF_INET6:
                {
                    sockaddr_in6 addr;
                    addr.sin6_family = AF_INET;
                    addr.sin6_port = 0;
                    in6_addr anyaddr = IN6ADDR_ANY_INIT;
                    addr.sin6_addr = anyaddr;
                    if(::bind(m_sock, (sockaddr*)&addr, sizeof(sockaddr_in))) {
                        throwExceptionFromLastError();
                    }
                    break;
                }
            default:
                assert(false);
        }
        
        m_ioManager->registerEvent(&m_sendEvent);
        if (!ConnectEx(m_sock, to.name(), to.nameLen(), NULL, 0, NULL, &m_sendEvent.overlapped)) {
            if (GetLastError() != WSA_IO_PENDING) {
                throwExceptionFromLastError();
            }
        }
        Scheduler::getThis()->yieldTo();
        if (!m_sendEvent.ret) {
            throwExceptionFromLastError(m_sendEvent.lastError);
        }
        setOption(SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
#else
        if (!::connect(m_sock, to.name(), to.nameLen())) {
            // Worked first time
            return;
        }
        if (errno == EINPROGRESS) {
            m_ioManager->registerEvent(m_sock, IOManager::WRITE);
            Scheduler::getThis()->yieldTo();
            int err;
            size_t size = sizeof(int);
            getOption(SOL_SOCKET, SO_ERROR, &err, &size);
            if (err != 0) {
                throwExceptionFromLastError(err);
            }
        } else {
            throwExceptionFromLastError();
        }
#endif
    }
}

void
Socket::listen(int backlog)
{
    if (::listen(m_sock, backlog)) {
        throwExceptionFromLastError();
    }
}

Socket::ptr
Socket::accept()
{
    Socket::ptr sock(new Socket(m_ioManager, m_family, type(), m_protocol, 0));
    accept(*sock.get());
    return sock;
}

void
Socket::accept(Socket &target)
{
#ifdef WINDOWS
    if (m_ioManager) {
        assert(target.m_sock != -1);
    } else {
        assert(target.m_sock == -1);
    }
#else
    assert(target.m_sock == -1);
#endif
    assert(target.m_family == m_family);
    assert(target.m_protocol == m_protocol);
    if (!m_ioManager) {
        int newsock = ::accept(m_sock, NULL, NULL);
        if (newsock == -1) {
            throwExceptionFromLastError();
        }
        target.m_sock = newsock;
    } else {
#ifdef WINDOWS
        m_ioManager->registerEvent(&m_receiveEvent);
        unsigned char addrs[64];
        DWORD bytes;
        BOOL ret = pAcceptEx(m_sock, target.m_sock, addrs, 64, (64 - 16) / 2, (64 - 16) / 2, &bytes,
            &m_receiveEvent.overlapped);
        if (!ret && GetLastError() != WSA_IO_PENDING) {
            throwExceptionFromLastError();
        }
        Scheduler::getThis()->yieldTo();
        if (!m_receiveEvent.ret && m_receiveEvent.lastError != ERROR_MORE_DATA) {
            throwExceptionFromLastError(m_receiveEvent.lastError);
        }
        target.setOption(SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, &m_sock, sizeof(m_sock));
#else
        int newsock = ::accept(m_sock, NULL, NULL);
        while (newsock == -1 && errno == EAGAIN) {
            m_ioManager->registerEvent(m_sock, IOManager::READ);
            Scheduler::getThis()->yieldTo();
            newsock = ::accept(m_sock, NULL, NULL);
        }
        if (newsock == -1) {
            throwExceptionFromLastError();
        }

        target.m_sock = newsock;
#endif
    }
}

void
Socket::shutdown(int how)
{
    if(::shutdown(m_sock, how)) {
        throwExceptionFromLastError();
    }
}

void
Socket::close()
{
    if (m_sock != -1) {
        if (::closesocket(m_sock)) {
            throwExceptionFromLastError();
        }
        m_sock = -1;
    }
}

size_t
Socket::send(const void *buf, size_t len, int flags)
{
#ifdef WINDOWS
    if (m_ioManager) {
        WSABUF wsabuf;
        wsabuf.buf = (char*)buf;
        wsabuf.len = len;
        m_ioManager->registerEvent(&m_sendEvent);
        int ret = WSASend(m_sock, &wsabuf, 1, NULL, flags,
            &m_sendEvent.overlapped, NULL);
        if (ret && GetLastError() != WSA_IO_PENDING) {
            throwExceptionFromLastError();
        }
        Scheduler::getThis()->yieldTo();
        if (!m_sendEvent.ret) {
            throwExceptionFromLastError(m_sendEvent.lastError);
        }
        return m_sendEvent.numberOfBytes;
    } else
#endif
    {
        int rc = ::send(m_sock, (const char*)buf, len, flags);
#ifndef WINDOWS
        while (m_ioManager && rc == -1 && errno == EAGAIN) {
            m_ioManager->registerEvent(m_sock, IOManager::WRITE);
            Scheduler::getThis()->yieldTo();
            rc = ::send(m_sock, buf, len, flags);
        }
#endif
        if (rc == -1) {
            throwExceptionFromLastError();
        }
        return rc;
    }
}

size_t
Socket::send(const iovec *bufs, size_t len, int flags)
{
#ifdef WINDOWS
    if (m_ioManager) {
        m_ioManager->registerEvent(&m_sendEvent);
        int ret = WSASend(m_sock, (LPWSABUF)bufs, len, NULL, flags,
            &m_sendEvent.overlapped, NULL);
        if (ret && GetLastError() != WSA_IO_PENDING) {
            throwExceptionFromLastError();
        }
        Scheduler::getThis()->yieldTo();
        if (!m_sendEvent.ret) {
            throwExceptionFromLastError(m_sendEvent.lastError);
        }
        return m_sendEvent.numberOfBytes;
    } else {
        DWORD sent;
        if (WSASend(m_sock, (LPWSABUF)bufs, len, &sent, flags,
            NULL, NULL)) {
            throwExceptionFromLastError();
        }
        return sent;
    }
#else
    msghdr msg;
    memset(&msg, 0, sizeof(msghdr));
    msg.msg_iov = (iovec*)bufs;
    msg.msg_iovlen = len;
    int rc = ::sendmsg(m_sock, &msg, flags);
    while (m_ioManager && rc == -1 && errno == EAGAIN) {
        m_ioManager->registerEvent(m_sock, IOManager::WRITE);
        Scheduler::getThis()->yieldTo();
        rc = ::sendmsg(m_sock, &msg, flags);
    }
    if (rc == -1) {
        throwExceptionFromLastError();
    }
    return rc;
#endif
}

size_t
Socket::sendTo(const void *buf, size_t len, int flags, const Address &to)
{
    assert(to.family() == family());
#ifdef WINDOWS
    if (m_ioManager) {
        WSABUF wsabuf;
        wsabuf.buf = (char*)buf;
        wsabuf.len = len;
        m_ioManager->registerEvent(&m_sendEvent);
        int ret = WSASendTo(m_sock, &wsabuf, 1, NULL, flags,
            to.name(), to.nameLen(),
            &m_sendEvent.overlapped, NULL);
        if (ret && GetLastError() != WSA_IO_PENDING) {
            throwExceptionFromLastError();
        }
        Scheduler::getThis()->yieldTo();
        if (!m_sendEvent.ret) {
            throwExceptionFromLastError(m_sendEvent.lastError);
        }
        return m_sendEvent.numberOfBytes;
    } else
#endif
    {
        int rc = ::sendto(m_sock, (const char*)buf, len, flags, to.name(), to.nameLen());
#ifndef WINDOWS
        while (m_ioManager && rc == -1 && errno == EAGAIN) {
            m_ioManager->registerEvent(m_sock, IOManager::WRITE);
            Scheduler::getThis()->yieldTo();
            rc = ::sendto(m_sock, buf, len, flags, to.name(), to.nameLen());
        }
#endif
        if (rc == -1) {
            throwExceptionFromLastError();
        }
        return rc;
    }
}

size_t
Socket::sendTo(const iovec *bufs, size_t len, int flags, const Address &to)
{
    assert(to.family() == family());
#ifdef WINDOWS
    if (m_ioManager) {
        m_ioManager->registerEvent(&m_sendEvent);
        int ret = WSASendTo(m_sock, (LPWSABUF)bufs, len, NULL, flags,
            to.name(), to.nameLen(),
            &m_sendEvent.overlapped, NULL);
        if (ret && GetLastError() != WSA_IO_PENDING) {
            throwExceptionFromLastError();
        }
        Scheduler::getThis()->yieldTo();
        if (!m_sendEvent.ret) {
            throwExceptionFromLastError(m_sendEvent.lastError);
        }
        return m_sendEvent.numberOfBytes;
    } else {
        DWORD sent;
        if (WSASendTo(m_sock, (LPWSABUF)bufs, len, &sent, flags,
            to.name(), to.nameLen(),
            NULL, NULL)) {
            throwExceptionFromLastError();
        }
        return sent;
    }
#else
    msghdr msg;
    memset(&msg, 0, sizeof(msghdr));
    msg.msg_iov = (iovec*)bufs;
    msg.msg_iovlen = len;
    msg.msg_name = (sockaddr*)to.name();
    msg.msg_namelen = to.nameLen();
    int rc = ::sendmsg(m_sock, &msg, flags);
    while (m_ioManager && rc == -1 && errno == EAGAIN) {
        m_ioManager->registerEvent(m_sock, IOManager::WRITE);
        Scheduler::getThis()->yieldTo();
        rc = ::sendmsg(m_sock, &msg, flags);
    }
    if (rc == -1) {
        throwExceptionFromLastError();
    }
    return rc;
#endif
}

size_t
Socket::receive(void *buf, size_t len, int flags)
{
#ifdef WINDOWS
    if (m_ioManager) {
        WSABUF wsabuf;
        wsabuf.buf = (char*)buf;
        wsabuf.len = len;
        m_ioManager->registerEvent(&m_receiveEvent);
        int ret = WSARecv(m_sock, &wsabuf, 1, NULL, (LPDWORD)&flags,
            &m_receiveEvent.overlapped, NULL);
        if (ret && GetLastError() != WSA_IO_PENDING) {
            throwExceptionFromLastError();
        }
        Scheduler::getThis()->yieldTo();
        if (!m_receiveEvent.ret) {
            throwExceptionFromLastError(m_receiveEvent.lastError);
        }
        return m_receiveEvent.numberOfBytes;
    } else
#endif
    {
        int rc = ::recv(m_sock, (char*)buf, len, flags);
#ifndef WINDOWS
        while (m_ioManager && rc == -1 && errno == EAGAIN) {
            m_ioManager->registerEvent(m_sock, IOManager::READ);
            Scheduler::getThis()->yieldTo();
            rc = ::recv(m_sock, buf, len, flags);
        }
#endif
        if (rc == -1) {
            throwExceptionFromLastError();
        }
        return rc;
    }
}

size_t
Socket::receive(iovec *bufs, size_t len, int flags)
{
#ifdef WINDOWS
    if (m_ioManager) {
        m_ioManager->registerEvent(&m_receiveEvent);
        int ret = WSARecv(m_sock, bufs, len, NULL, (LPDWORD)&flags,
            &m_receiveEvent.overlapped, NULL);
        if (ret && GetLastError() != WSA_IO_PENDING) {
            throwExceptionFromLastError();
        }
        Scheduler::getThis()->yieldTo();
        if (!m_receiveEvent.ret) {
            throwExceptionFromLastError(m_receiveEvent.lastError);
        }
        return m_receiveEvent.numberOfBytes;
    } else {
        DWORD received;
        if (WSARecv(m_sock, bufs, len, &received, (LPDWORD)&flags,
            NULL, NULL)) {
            throwExceptionFromLastError();
        }
        return received;
    }
#else
    msghdr msg;
    memset(&msg, 0, sizeof(msghdr));
    msg.msg_iov = bufs;
    msg.msg_iovlen = len;
    int rc = ::recvmsg(m_sock, &msg, flags);
    while (m_ioManager && rc == -1 && errno == EAGAIN) {
        m_ioManager->registerEvent(m_sock, IOManager::READ);
        Scheduler::getThis()->yieldTo();
        rc = ::recvmsg(m_sock, &msg, flags);
    }
    if (rc == -1) {
        throwExceptionFromLastError();
    }
    return rc;
#endif
}

size_t
Socket::receiveFrom(void *buf, size_t len, int *flags, Address *from)
{
    assert(from->family() == family());
#ifdef WINDOWS
    WSABUF wsabuf;
    wsabuf.buf = (char*)buf;
    wsabuf.len = len;
    int namelen = from->nameLen();
    if (m_ioManager) {        
        m_ioManager->registerEvent(&m_sendEvent);        
        int ret = WSARecvFrom(m_sock, &wsabuf, 1, NULL, (LPDWORD)flags,
            from->name(), &namelen,
            &m_sendEvent.overlapped, NULL);
        if (ret && GetLastError() != WSA_IO_PENDING) {
            throwExceptionFromLastError();
        }
        Scheduler::getThis()->yieldTo();
        if (!m_sendEvent.ret) {
            throwExceptionFromLastError(m_sendEvent.lastError);
        }
        return m_sendEvent.numberOfBytes;
    } else {
        DWORD sent;
        if (WSARecvFrom(m_sock, &wsabuf, 1, &sent, (LPDWORD)flags,
            from->name(), &namelen,
            NULL, NULL)) {
            throwExceptionFromLastError();
        }
        return sent;
    }
#else
    msghdr msg;
    memset(&msg, 0, sizeof(msghdr));
    iovec iov;
    iov.iov_base = buf;
    iov.iov_len = len;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_name = from->name();
    msg.msg_namelen = from->nameLen();
    int rc = ::recvmsg(m_sock, &msg, *flags);
    while (m_ioManager && rc == -1 && errno == EAGAIN) {
        m_ioManager->registerEvent(m_sock, IOManager::READ);
        Scheduler::getThis()->yieldTo();
        rc = ::recvmsg(m_sock, &msg, *flags);
    }
    if (rc == -1) {
        throwExceptionFromLastError();
    }
    *flags = msg.msg_flags;
    return rc;
#endif
}

size_t
Socket::receiveFrom(iovec *bufs, size_t len, int *flags, Address *from)
{
    assert(from->family() == family());
#ifdef WINDOWS
    int namelen = from->nameLen();
    if (m_ioManager) {
        m_ioManager->registerEvent(&m_sendEvent);
        int ret = WSARecvFrom(m_sock, bufs, len, NULL, (LPDWORD)flags,
            from->name(), &namelen,
            &m_sendEvent.overlapped, NULL);
        if (ret && GetLastError() != WSA_IO_PENDING) {
            throwExceptionFromLastError();
        }
        Scheduler::getThis()->yieldTo();
        if (!m_sendEvent.ret) {
            throwExceptionFromLastError(m_sendEvent.lastError);
        }
        return m_sendEvent.numberOfBytes;
    } else {
        DWORD sent;
        if (WSARecvFrom(m_sock, bufs, len, &sent, (LPDWORD)flags,
            from->name(), &namelen,
            NULL, NULL)) {
            throwExceptionFromLastError();
        }
        return sent;
    }
#else
    msghdr msg;
    memset(&msg, 0, sizeof(msghdr));
    msg.msg_iov = bufs;
    msg.msg_iovlen = len;
    msg.msg_name = from->name();
    msg.msg_namelen = from->nameLen();
    int rc = ::recvmsg(m_sock, &msg, *flags);
    while (m_ioManager && rc == -1 && errno == EAGAIN) {
        m_ioManager->registerEvent(m_sock, IOManager::READ);
        Scheduler::getThis()->yieldTo();
        rc = ::recvmsg(m_sock, &msg, *flags);
    }
    if (rc == -1) {
        throwExceptionFromLastError();
    }
    *flags = msg.msg_flags;
    return rc;
#endif
}

void
Socket::getOption(int level, int option, void *result, size_t *len)
{
    int ret = getsockopt(m_sock, level, option, (char*)result, (socklen_t*)len);
    if (ret) {
        throwExceptionFromLastError();
    }
}

void
Socket::setOption(int level, int option, const void *value, size_t len)
{
    if (setsockopt(m_sock, level, option, (const char*)value, len)) {
        throwExceptionFromLastError();
    }
}

Address::ptr
Socket::emptyAddress()
{
    switch (m_family) {
        case AF_INET:
            return Address::ptr(new IPv4Address(type(), m_protocol));
        case AF_INET6:
            return Address::ptr(new IPv6Address(type(), m_protocol));
        default:
            return Address::ptr(new UnknownAddress(m_family, type(), m_protocol));
    }
}

Address::ptr
Socket::remoteAddress()
{
    Address::ptr result;
    switch (m_family) {
        case AF_INET:
            result.reset(new IPv4Address(type(), m_protocol));
            break;
        case AF_INET6:
            result.reset(new IPv6Address(type(), m_protocol));
            break;
        default:
            result.reset(new UnknownAddress(m_family, type(), m_protocol));
            break;
    }
    socklen_t namelen = result->nameLen();
    if (getpeername(m_sock, result->name(), &namelen)) {
        throwExceptionFromLastError();
    }
    assert(namelen <= result->nameLen());
    return result;
}

Address::ptr
Socket::localAddress()
{
    Address::ptr result;
    switch (m_family) {
        case AF_INET:
            result.reset(new IPv4Address(type(), m_protocol));
            break;
        case AF_INET6:
            result.reset(new IPv6Address(type(), m_protocol));
            break;
        default:
            result.reset(new UnknownAddress(m_family, type(), m_protocol));
            break;
    }
    socklen_t namelen = result->nameLen();
    if (getsockname(m_sock, result->name(), &namelen)) {
        throwExceptionFromLastError();
    }
    assert(namelen <= result->nameLen());
    return result;
}

int
Socket::type()
{
    int result;
    size_t len = sizeof(int);
    getOption(SOL_SOCKET, SO_TYPE, &result, &len);
    return result;
}

Address::Address(int type, int protocol)
: m_type(type),
  m_protocol(protocol)
{}

std::vector<boost::shared_ptr<Address> >
Address::lookup(const std::string &host, int family, int type, int protocol)
{
    addrinfo hints;
    hints.ai_flags = 0;
    hints.ai_family = family;
    hints.ai_socktype = type;
    hints.ai_protocol = protocol;
    hints.ai_addrlen = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    std::string node = host;
    const char *server = (const char*)memchr(node.c_str(), ':', node.size());
    if (server) {
        node = node.substr(0, server - node.c_str());
        ++server;
    }
    addrinfo *results, *next;
    if (getaddrinfo(node.c_str(), server, &hints, &results)) {
        throwExceptionFromLastError();
    }
    std::vector<Address::ptr> result;
    next = results;
    while (next) {
        Address::ptr addr;
        switch (next->ai_family) {
            case AF_INET:
                addr.reset(new IPv4Address(next->ai_socktype, next->ai_protocol));
                assert(next->ai_addrlen <= addr->nameLen());
                memcpy(addr->name(), next->ai_addr, next->ai_addrlen);
                break;
            case AF_INET6:
                addr.reset(new IPv6Address(next->ai_socktype, next->ai_protocol));
                assert(next->ai_addrlen <= addr->nameLen());
                memcpy(addr->name(), next->ai_addr, next->ai_addrlen);
                break;
            default:
                addr.reset(new UnknownAddress(next->ai_family, next->ai_socktype, next->ai_protocol));
                assert(next->ai_addrlen <= addr->nameLen());
                memcpy(addr->name(), next->ai_addr, next->ai_addrlen);
                break;
        }
        result.push_back(addr);
        next = next->ai_next;
    }
    freeaddrinfo(results);
    return result;
}

Socket::ptr
Address::createSocket()
{
    return Socket::ptr(new Socket(family(), type(), protocol()));
}

Socket::ptr
Address::createSocket(IOManager &ioManager)
{
    return Socket::ptr(new Socket(ioManager, family(), type(), protocol()));
}

IPAddress::IPAddress(int type, int protocol)
: Address(type, protocol)
{}

IPv4Address::IPv4Address(int type, int protocol)
: IPAddress(type, protocol)
{
    sin.sin_family = AF_INET;
    sin.sin_port = 0;
    sin.sin_addr.s_addr = INADDR_ANY;
}

IPv6Address::IPv6Address(int type, int protocol)
: IPAddress(type, protocol)
{
    sin.sin6_family = AF_INET6;
    sin.sin6_port = 0;
    in6_addr anyaddr = IN6ADDR_ANY_INIT;
    sin.sin6_addr = anyaddr;
}

UnknownAddress::UnknownAddress(int family, int type, int protocol)
: Address(type, protocol)
{
    sa.sa_family = family;
}
