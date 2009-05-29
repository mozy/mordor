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
#define closesocket close
#endif

Socket::Socket(IOManager *ioManager, int family, int type, int protocol, int initialize)
: m_sock(-1),
  m_ioManager(ioManager),
  m_family(family),
  m_protocol(protocol)
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
  m_ioManager(NULL),
  m_family(family),
  m_protocol(protocol)
{
    m_sock = socket(family, type, protocol);
    if (m_sock == -1) {
        throwExceptionFromLastError();
    }
}

Socket::Socket(IOManager *ioManager, int family, int type, int protocol)
: m_sock(-1),
  m_ioManager(ioManager),
  m_family(family),
  m_protocol(protocol)
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
Socket::bind(const Address *addr)
{
    assert(addr->family() == m_family);
    if (::bind(m_sock, addr->name(), addr->nameLen())) {
        throwExceptionFromLastError();
    }
}

void
Socket::connect(const Address *to)
{
    assert(to->family() == m_family);
    if (!m_ioManager) {
        if (::connect(m_sock, to->name(), to->nameLen())) {
            throwExceptionFromLastError();
        }
    } else {
#ifdef WINDOWS
        // need to be bound, even to ADDR_ANY, before calling ConnectEx
        switch (m_family) {
            case AF_INET:
                sockaddr_in addr;
                addr.sin_family = AF_INET;
                addr.sin_port = 0;
                *(int*)&addr.sin_addr = ADDR_ANY;
                if(::bind(m_sock, (sockaddr*)&addr, sizeof(sockaddr_in))) {
                    throwExceptionFromLastError();
                }
                break;
            default:
                assert(false);
        }
        
        m_ioManager->registerEvent(&m_sendEvent);
        if (!ConnectEx(m_sock, to->name(), to->nameLen(), NULL, 0, NULL, &m_sendEvent.overlapped)) {
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
        if (!::connect(m_sock, to->name(), to->nameLen())) {
            // TODO: what, it worked?!
        }
        if (errno == EINPROGRESS) {
            m_ioManager->registerEvent(&m_sendEvent);
            Scheduler::getThis()->yieldTo();
            int err;
            getOption(SOL_SOCKET, SO_ERROR, &err, sizeof(err));
            if (error != 0) {
                throwExceptionFromLastError(error);
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

Socket *
Socket::accept()
{
    std::auto_ptr<Socket> sock(new Socket(m_ioManager, m_family, type(), m_protocol, 0));
    accept(sock.get());
    return sock.release();
}

void
Socket::accept(Socket *target)
{
#ifdef WINDOWS
    if (m_ioManager) {
        assert(target->m_sock != -1);
    } else {
        assert(target->m_sock == -1);
    }
#else
    assert(target->m_sock == -1);
#endif
    assert(target->m_family == m_family);
    assert(target->m_protocol == m_protocol);
    if (!m_ioManager) {
        int newsock = ::accept(m_sock, NULL, NULL);
        if (newsock == -1) {
            throwExceptionFromLastError();
        }
        target->m_sock = newsock;
    } else {
#ifdef WINDOWS
        m_ioManager->registerEvent(&m_receiveEvent);
        unsigned char addrs[64];
        DWORD bytes;
        BOOL ret = pAcceptEx(m_sock, target->m_sock, addrs, 64, (64 - 16) / 2, (64 - 16) / 2, &bytes,
            &m_receiveEvent.overlapped);
        if (!ret && GetLastError() != WSA_IO_PENDING) {
            throwExceptionFromLastError();
        }
        Scheduler::getThis()->yieldTo();
        if (!m_receiveEvent.ret && m_receiveEvent.lastError != ERROR_MORE_DATA) {
            throwExceptionFromLastError(m_receiveEvent.lastError);
        }
        target->setOption(SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, &m_sock, sizeof(m_sock));
#else
        int newsock = ::accept(m_sock, NULL, NULL);
        while (newsock == -1 && errno == EAGAIN) {
            m_ioManager->registerEvent(&m_receiveEvent);
            Scheduler::getThis()->yieldTo();
            newsock = ::accept(m_sock, NULL, NULL);
        }
        if (newsock == -1) {
            throwExceptionFromLastError();
        }

        target->m_sock(newsock);
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
        if (closesocket(m_sock)) {
            throwExceptionFromLastError();
        }
        m_sock = -1;
    }
}

size_t
Socket::send(const void *buf, size_t len, int flags)
{
    if (!m_ioManager) {
        int rc = ::send(m_sock, (const char*)buf, len, flags);
        if (rc == -1) {
            throwExceptionFromLastError();
        }
        return rc;
    } else {
#ifdef WINDOWS
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
#else
        int rc = ::send(m_sock, buf, len, flags);
        while (rc == -1 && errno == EAGAIN) {
            m_ioManager->registerEvent(&m_sendEvent);
            Scheduler::getThis()->yieldTo();
            int rc = ::send(m_sock, buf, len, flags);
        }
        if (rc == -1) {
            throwExceptionFromLastError();
        }
        return rc;
#endif
    }
}

size_t
Socket::receive(void *buf, size_t len, int flags)
{
    if (!m_ioManager) {
        int rc = ::recv(m_sock, (char*)buf, len, flags);
        if (rc == -1) {
            throwExceptionFromLastError();
        }
        return rc;
    } else {
#ifdef WINDOWS
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
        return m_sendEvent.numberOfBytes;
#else
        int rc = ::recv(m_sock, buf, len, flags);
        while (rc == -1 && errno == EAGAIN) {
            m_ioManager->registerEvent(&m_receiveEvent);
            Scheduler::getThis()->yieldTo();
            int rc = ::recv(m_sock, buf, len, flags);
        }
        if (rc == -1) {
            throwExceptionFromLastError();
        }
        return rc;
#endif
    }
}


void
Socket::getOption(int level, int option, void *result, size_t *len)
{
    int ret = getsockopt(m_sock, level, option, (char*)result, (int*)len);
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

Address *
Socket::remoteAddress()
{
    std::auto_ptr<Address> result;
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
    int namelen = result->nameLen();
    if (getpeername(m_sock, result->name(), &namelen)) {
        throwExceptionFromLastError();
    }
    assert(namelen <= result->nameLen());
    return result.release();
}

Address *
Socket::localAddress()
{
    std::auto_ptr<Address> result;
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
    int namelen = result->nameLen();
    if (getsockname(m_sock, result->name(), &namelen)) {
        throwExceptionFromLastError();
    }
    assert(namelen <= result->nameLen());
    return result.release();
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
    std::vector<boost::shared_ptr<Address> > result;
    next = results;
    while (next) {
        boost::shared_ptr<Address> addr;
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

Socket *
Address::createSocket()
{
    return new Socket(family(), type(), protocol());
}

Socket *
Address::createSocket(IOManager *ioManager)
{
    return new Socket(ioManager, family(), type(), protocol());
}

IPv4Address::IPv4Address(int type, int protocol)
: Address(type, protocol)
{
    sin.sin_family = AF_INET;
    sin.sin_port = 0;
    *(int*)&sin.sin_addr = 0;
}

IPv6Address::IPv6Address(int type, int protocol)
: Address(type, protocol)
{
    sin.sin6_family = AF_INET6;
    sin.sin6_port = 0;
}

UnknownAddress::UnknownAddress(int family, int type, int protocol)
: Address(type, protocol)
{
    sa.sa_family = family;
}
