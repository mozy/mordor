// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "socket.h"

#include <boost/bind.hpp>

#include "assert.h"
#include "exception.h"
#include "log.h"
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

        socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
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
#include <fcntl.h>
#include <netdb.h>
#define closesocket close
#endif

static Logger::ptr g_log = Log::lookup("mordor:common:socket");

Socket::Socket(IOManager *ioManager, int family, int type, int protocol, int initialize)
: m_sock(-1),
  m_family(family),
  m_protocol(protocol),
  m_ioManager(ioManager),
  m_receiveTimeout(~0ull),
  m_sendTimeout(~0ull),
  m_cancelledSend(0),
  m_cancelledReceive(0)
#ifdef WINDOWS
  , m_hEvent(NULL),
  m_scheduler(NULL)  
#endif
{
#ifdef WINDOWS
    if (m_ioManager) {
        m_sock = socket(family, type, protocol);
        LOG_LEVEL(g_log, m_sock == -1 ? Log::ERROR : Log::VERBOSE) << this
            << " socket(" << family << ", " << type << ", " << protocol
            << "): " << m_sock << " (" << lastError() << ")";
        if (m_sock == -1) {
            throwExceptionFromLastError("socket");
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
    LOG_VERBOSE(g_log) << this << " socket(" << family << ", " << type << ", "
        << protocol << "): " << m_sock << " (" << lastError() << ")";
    if (m_sock == -1) {
        throwExceptionFromLastError("socket");
    }
#ifdef OSX
    unsigned int opt = 1;
    if (setsockopt(m_sock, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt)) == -1) {
        ::closesocket(m_sock);
        throwExceptionFromLastError("setsockopt");
    }
#endif
}

Socket::Socket(IOManager &ioManager, int family, int type, int protocol)
: m_sock(-1),
  m_family(family),
  m_protocol(protocol),
  m_ioManager(&ioManager),
  m_receiveTimeout(~0ull),
  m_sendTimeout(~0ull),
  m_cancelledSend(0),
  m_cancelledReceive(0)
#ifdef WINDOWS
  , m_hEvent(NULL),
  m_scheduler(NULL)  
#endif
{
    m_sock = socket(family, type, protocol);
    LOG_VERBOSE(g_log) << this << " socket(" << family << ", " << type << ", "
        << protocol << "): " << m_sock << " (" << lastError() << ")";
    if (m_sock == -1) {
        throwExceptionFromLastError("socket");
    }
#ifdef WINDOWS
    try {
        m_ioManager->registerFile((HANDLE)m_sock);
    } catch(...) {
        closesocket(m_sock);
        throw;
    }
#else
    if (fcntl(m_sock, F_SETFL, O_NONBLOCK) == -1) {
        ::closesocket(m_sock);
        throwExceptionFromLastError("fcntl");
    }
#endif
#ifdef OSX
    unsigned int opt = 1;
    if (setsockopt(m_sock, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt)) == -1) {
        ::closesocket(m_sock);
        throwExceptionFromLastError("setsockopt");
    }
#endif
}

Socket::~Socket()
{
    close();
#ifdef WINDOWS
    if (m_ioManager && m_hEvent)
        CloseHandle(m_hEvent);
#endif
}

void
Socket::bind(const Address &addr)
{
    ASSERT(addr.family() == m_family);
    if (::bind(m_sock, addr.name(), addr.nameLen())) {
        LOG_ERROR(g_log) << this << " bind(" << m_sock << ", " << addr
            << "): (" << lastError() << ")";
        throwExceptionFromLastError("bind");
    }
    LOG_VERBOSE(g_log) << this << " bind(" << m_sock << ", " << addr << ")";
}

void
Socket::connect(const Address &to)
{
    ASSERT(to.family() == m_family);
    if (!m_ioManager) {
        if (::connect(m_sock, to.name(), to.nameLen())) {
            LOG_ERROR(g_log) << this << " connect(" << m_sock << ", " << to
                << "): (" << lastError() << ")";
            throwExceptionFromLastError("connect");
        }
        LOG_INFO(g_log) << this << " connect(" << m_sock << ", " << to << ")";
    } else {
#ifdef WINDOWS
        if (ConnectEx) {
            // need to be bound, even to ADDR_ANY, before calling ConnectEx
            switch (m_family) {
                case AF_INET:
                    {
                        sockaddr_in addr;
                        addr.sin_family = AF_INET;
                        addr.sin_port = 0;
                        addr.sin_addr.s_addr = ADDR_ANY;
                        if(::bind(m_sock, (sockaddr*)&addr, sizeof(sockaddr_in))) {
                            LOG_ERROR(g_log) << this << " bind(" << m_sock
                                << ", 0.0.0.0:0): (" << lastError() << ")";
                            throwExceptionFromLastError("bind");
                        }
                        break;
                    }
                case AF_INET6:
                    {
                        sockaddr_in6 addr;
                        memset(&addr, 0, sizeof(sockaddr_in6));
                        addr.sin6_family = AF_INET6;
                        addr.sin6_port = 0;
                        in6_addr anyaddr = IN6ADDR_ANY_INIT;
                        addr.sin6_addr = anyaddr;
                        if(::bind(m_sock, (sockaddr*)&addr, sizeof(sockaddr_in6))) {
                            LOG_ERROR(g_log) << this << " bind(" << m_sock
                                << ", [::]:0): (" << lastError() << ")";
                            throwExceptionFromLastError("bind");
                        }
                        break;
                    }
                default:
                    NOTREACHED();
            }

            ptr self = shared_from_this();
            m_ioManager->registerEvent(&m_sendEvent);
            if (!ConnectEx(m_sock, to.name(), to.nameLen(), NULL, 0, NULL, &m_sendEvent.overlapped)) {
                DWORD dwLastError = GetLastError();
                if (dwLastError != WSA_IO_PENDING) {
                    LOG_ERROR(g_log) << this << " connect(" << m_sock << ", " << to
                        << "): (" << lastError() << ")";
                    m_ioManager->unregisterEvent(&m_sendEvent);
                    throwExceptionFromLastError(dwLastError, "ConnectEx");
                }
            }
            if (m_cancelledSend) {
                LOG_ERROR(g_log) << this << " connect(" << m_sock << ", " << to
                        << "): (" << m_cancelledSend << ")";
                m_ioManager->cancelEvent((HANDLE)m_sock, &m_sendEvent);
                Scheduler::getThis()->yieldTo();
                throwExceptionFromLastError(m_cancelledSend, "ConnectEx");
            }
            Timer::ptr timeout;
            if (m_sendTimeout != ~0ull)
                timeout = m_ioManager->registerTimer(m_sendTimeout, boost::bind(
                    &IOManagerIOCP::cancelEvent, m_ioManager, (HANDLE)m_sock, &m_receiveEvent));
            Scheduler::getThis()->yieldTo();
            if (timeout)
                timeout->cancel();
            if (!m_sendEvent.ret) {
                if (m_sendEvent.lastError == ERROR_OPERATION_ABORTED &&
                    m_cancelledSend != ERROR_OPERATION_ABORTED)
                    m_sendEvent.lastError = WSAETIMEDOUT;
                LOG_ERROR(g_log) << this << " connect(" << m_sock << ", " << to
                    << "): (" << m_sendEvent.lastError << ")";
                throwExceptionFromLastError(m_sendEvent.lastError, "ConnectEx");
            }
            LOG_INFO(g_log) << this << " connect(" << m_sock << ", " << to
                << ")";
            setOption(SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
        } else {
            if (!m_hEvent) {
                m_hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
                if (!m_hEvent)
                    throwExceptionFromLastError("CreateEventW");
            }
            if (WSAEventSelect(m_sock, m_hEvent, FD_CONNECT))
                throwExceptionFromLastError("WSAEventSelect");
            if (!::connect(m_sock, to.name(), to.nameLen())) {
                LOG_INFO(g_log) << this << " connect(" << m_sock << ", "
                    << to << ")";
                // Worked first time
                return;
            }
            if (GetLastError() == WSAEWOULDBLOCK) {
                m_ioManager->registerEvent(m_hEvent);
                m_fiber = Fiber::getThis();
                m_scheduler = Scheduler::getThis();
                m_unregistered = false;
                if (m_cancelledSend) {
                    LOG_ERROR(g_log) << this << " connect(" << m_sock << ", " << to
                            << "): (" << m_cancelledSend << ")";
                    if (!m_ioManager->unregisterEvent(m_hEvent))
                        Scheduler::getThis()->yieldTo();
                    throwExceptionFromLastError(m_cancelledSend, "connect");
                }
                Timer::ptr timeout;
                if (m_sendTimeout != ~0ull)
                    timeout = m_ioManager->registerTimer(m_sendTimeout,
                        boost::bind(&Socket::cancelConnect, this));
                Scheduler::getThis()->yieldTo();
                m_fiber.reset();
                m_scheduler = NULL;
                if (timeout)
                    timeout->cancel();
                // The timeout expired, but the event fired before we could
                // cancel it, so we got scheduled twice
                if (m_cancelledSend && !m_unregistered)
                    Scheduler::getThis()->yieldTo();
                if (m_cancelledSend) {
                    LOG_ERROR(g_log) << this << " connect(" << m_sock
                        << ", " << to << "): (" << m_cancelledSend << ")";
                    throwExceptionFromLastError(m_cancelledSend, "connect");
                }
                ::connect(m_sock, to.name(), to.nameLen());
                LOG_LEVEL(g_log, GetLastError() ? Log::ERROR : Log::INFO)
                    << this << " connect(" << m_sock << ", " << to
                    << "): (" << lastError() << ")";
                if (GetLastError() != ERROR_SUCCESS)
                    throwExceptionFromLastError("connect");
            } else {
                LOG_ERROR(g_log) << this << " connect(" << m_sock << ", "
                    << to << "): (" << lastError() << ")";
                throwExceptionFromLastError("connect");
            }
        }
#else
        if (!::connect(m_sock, to.name(), to.nameLen())) {
            LOG_INFO(g_log) << this << " connect(" << m_sock << ", " << to
                << ")";
            // Worked first time
            return;
        }
        if (errno == EINPROGRESS) {
            ptr self = shared_from_this();
            m_ioManager->registerEvent(m_sock, IOManager::WRITE);
            if (m_cancelledSend) {
                LOG_ERROR(g_log) << this << " connect(" << m_sock << ", " << to
                    << "): (" << m_cancelledSend << ")";
                m_ioManager->cancelEvent(m_sock, IOManager::WRITE);
                Scheduler::getThis()->yieldTo();
                throwExceptionFromLastError(m_cancelledSend, "connect");
            }
            Timer::ptr timeout;
            if (m_sendTimeout != ~0ull)
                timeout = m_ioManager->registerTimer(m_sendTimeout, boost::bind(
                    &Socket::cancelIo, this, IOManager::WRITE,
                    boost::ref(m_cancelledSend), ETIMEDOUT));
            Scheduler::getThis()->yieldTo();
            if (timeout)
                timeout->cancel();
            if (m_cancelledSend) {
                LOG_ERROR(g_log) << this << " connect(" << m_sock << ", " << to
                    << "): (" << m_cancelledSend << ")";
                throwExceptionFromLastError(m_cancelledSend, "connect");
            }
            int err;
            size_t size = sizeof(int);
            getOption(SOL_SOCKET, SO_ERROR, &err, &size);
            if (err != 0) {
                LOG_ERROR(g_log) << this << " connect(" << m_sock << ", " << to
                    << "): (" << err << ")";
                throwExceptionFromLastError(err, "connect");
            }
            LOG_INFO(g_log) << this << " connect(" << m_sock << ", " << to
                << ")";
        } else {
            LOG_ERROR(g_log) << this << " connect(" << m_sock << ", " << to
                << "): (" << lastError() << ")";
            throwExceptionFromLastError("connect");
        }
#endif
    }
}

void
Socket::listen(int backlog)
{
    if (::listen(m_sock, backlog)) {
        LOG_ERROR(g_log) << this << " listen(" << m_sock << "): ("
            << lastError() << ")";
        throwExceptionFromLastError("listen");
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
        ASSERT(target.m_sock != -1);
    } else {
        ASSERT(target.m_sock == -1);
    }
#else
    ASSERT(target.m_sock == -1);
#endif
    ASSERT(target.m_family == m_family);
    ASSERT(target.m_protocol == m_protocol);
    if (!m_ioManager) {
        socket_t newsock = ::accept(m_sock, NULL, NULL);
        LOG_LEVEL(g_log, newsock == -1 ? Log::ERROR : Log::TRACE)
            << this << " accept(" << m_sock << "): " << newsock << " ("
            << lastError() << ")";
        if (newsock == -1) {
            throwExceptionFromLastError("accept");
        }
        target.m_sock = newsock;
    } else {
        ptr self = shared_from_this();
#ifdef WINDOWS
        if (pAcceptEx) {
            m_ioManager->registerEvent(&m_receiveEvent);
            unsigned char addrs[sizeof(SOCKADDR_STORAGE) * 2 + 16];
            DWORD bytes;
            BOOL ret = pAcceptEx(m_sock, target.m_sock, addrs, 0, sizeof(SOCKADDR_STORAGE), sizeof(SOCKADDR_STORAGE), &bytes,
                &m_receiveEvent.overlapped);
            DWORD dwLastError = GetLastError();
            if (!ret && dwLastError != WSA_IO_PENDING) {
                LOG_ERROR(g_log) << this << " accept(" << m_sock << "):  ("
                    << lastError() << ")";
                m_ioManager->unregisterEvent(&m_receiveEvent);
                throwExceptionFromLastError(dwLastError, "AcceptEx");
            }
            if (m_cancelledReceive) {
                LOG_ERROR(g_log) << this << " accept(" << m_sock << "): (" 
                    << m_cancelledReceive << ")";
                m_ioManager->cancelEvent((HANDLE)m_sock, &m_receiveEvent);
                Scheduler::getThis()->yieldTo();
                throwExceptionFromLastError(m_cancelledReceive, "AcceptEx");
            }
            Timer::ptr timeout;
            if (m_receiveTimeout != ~0ull)
                timeout = m_ioManager->registerTimer(m_receiveTimeout, boost::bind(
                    &IOManagerIOCP::cancelEvent, m_ioManager, (HANDLE)m_sock, &m_receiveEvent));
            Scheduler::getThis()->yieldTo();
            if (timeout)
                timeout->cancel();
            if (!m_receiveEvent.ret && m_receiveEvent.lastError != ERROR_MORE_DATA) {
                if (m_receiveEvent.lastError == ERROR_OPERATION_ABORTED &&
                    m_cancelledReceive != ERROR_OPERATION_ABORTED)
                    m_receiveEvent.lastError = WSAETIMEDOUT;
                LOG_ERROR(g_log) << this << " accept(" << m_sock << "): ("
                    << m_receiveEvent.lastError << ")";
                throwExceptionFromLastError(m_receiveEvent.lastError, "AcceptEx");
            }
            LOG_TRACE(g_log) << this << " accept(" << m_sock << "): "
                << target.m_sock;
            target.setOption(SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, &m_sock, sizeof(m_sock));
            target.m_ioManager->registerFile((HANDLE)target.m_sock);
        } else {
            if (!m_hEvent) {
                m_hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
                if (!m_hEvent)
                    throwExceptionFromLastError("CreateEventW");
            }
            if (WSAEventSelect(m_sock, m_hEvent, FD_ACCEPT))
                throwExceptionFromLastError("WSAEventSelect");
            socket_t newsock = ::accept(m_sock, NULL, NULL);
            if (newsock != -1) {
                LOG_INFO(g_log) << this << " accept(" << m_sock << "): "
                    << newsock;
                // Worked first time
                return;
            }
            if (GetLastError() == WSAEWOULDBLOCK) {
                m_ioManager->registerEvent(m_hEvent);
                m_fiber = Fiber::getThis();
                m_scheduler = Scheduler::getThis();
                if (m_cancelledReceive) {
                    LOG_ERROR(g_log) << this << " accept(" << m_sock << "): (" 
                        << m_cancelledReceive << ")";
                    if (!m_ioManager->unregisterEvent(m_hEvent))
                        Scheduler::getThis()->yieldTo();
                    throwExceptionFromLastError(m_cancelledReceive, "accept");
                }
                m_unregistered = false;
                Timer::ptr timeout;
                if (m_receiveTimeout != ~0ull)
                    timeout = m_ioManager->registerTimer(m_sendTimeout,
                        boost::bind(&Socket::cancelAccept, this));
                Scheduler::getThis()->yieldTo();
                m_fiber.reset();
                m_scheduler = NULL;
                if (timeout)
                    timeout->cancel();
                // The timeout expired, but the event fired before we could
                // cancel it, so we got scheduled twice
                if (m_cancelledReceive && !m_unregistered)
                    Scheduler::getThis()->yieldTo();
                if (m_cancelledReceive) {
                    LOG_ERROR(g_log) << this << " accept(" << m_sock
                        << "): (" << m_cancelledReceive << ")";
                    throwExceptionFromLastError(m_cancelledReceive, "accept");
                }
                newsock = ::accept(m_sock, NULL, NULL);
                LOG_LEVEL(g_log, GetLastError() ? Log::ERROR : Log::INFO)
                    << this << " accept(" << m_sock << "): " << newsock << " ("
                    << lastError() << ")";
                if (newsock == -1)
                    throwExceptionFromLastError("accept");
            } else {
                LOG_ERROR(g_log) << this << " accept(" << m_sock << "): ("
                    << lastError() << ")";
                throwExceptionFromLastError("accept");
            }
        }
#else
        int newsock = ::accept(m_sock, NULL, NULL);
        while (newsock == -1 && errno == EAGAIN) {
            m_ioManager->registerEvent(m_sock, IOManager::READ);
            if (m_cancelledReceive) {
                LOG_ERROR(g_log) << this << " accept(" << m_sock << "): ("
                    << m_cancelledReceive << ")";
                m_ioManager->cancelEvent(m_sock, IOManager::READ);
                Scheduler::getThis()->yieldTo();
                throwExceptionFromLastError(m_cancelledReceive, "accept");
            }
            Timer::ptr timeout;
            if (m_receiveTimeout != ~0ull)
                timeout = m_ioManager->registerTimer(m_receiveTimeout, boost::bind(
                    &Socket::cancelIo, this, IOManager::READ,
                    boost::ref(m_cancelledReceive), ETIMEDOUT));
            Scheduler::getThis()->yieldTo();
            if (timeout)
                timeout->cancel();
            if (m_cancelledReceive) {
                LOG_ERROR(g_log) << this << " accept(" << m_sock
                    << "): (" << m_cancelledReceive << ")";
                throwExceptionFromLastError(m_cancelledReceive, "accept");
            }
            newsock = ::accept(m_sock, NULL, NULL);
        }
        LOG_LEVEL(g_log, newsock == -1 ? Log::ERROR : Log::TRACE)
            << this << " accept(" << m_sock << "): " << newsock
            << " (" << lastError() << ")";
        if (newsock == -1) {
            throwExceptionFromLastError("accept");
        }
        if (fcntl(newsock, F_SETFL, O_NONBLOCK) == -1) {
            ::close(newsock);
            throwExceptionFromLastError("fcntl");
        }
        target.m_sock = newsock;
#endif
    }
}

void
Socket::shutdown(int how)
{
    if(::shutdown(m_sock, how)) {
        LOG_ERROR(g_log) << this << " shutdown(" << m_sock << ", "
            << how << "): (" << lastError() << ")";
        throwExceptionFromLastError("shutdown");
    }
    LOG_TRACE(g_log) << this << " shutdown(" << m_sock << ", "
        << how << ")";
}

void
Socket::close()
{
    if (m_sock != -1) {
        if (::closesocket(m_sock)) {
            LOG_ERROR(g_log) << this << " close(" << m_sock << "): ("
                << lastError() << ")";
            throwExceptionFromLastError("close");
        }
        LOG_VERBOSE(g_log) << this << " close(" << m_sock << ")";
        m_sock = -1;
    }
}

size_t
Socket::send(const void *buf, size_t len, int flags)
{
#if !defined(WINDOWS) && !defined(OSX)
    flags |= MSG_NOSIGNAL;
#endif

#ifdef WINDOWS
    if (m_ioManager) {
        if (len > 0xffffffff)
            len = 0xffffffff;
        WSABUF wsabuf;
        wsabuf.buf = (char*)buf;
        wsabuf.len = (unsigned int)len;
        ptr self = shared_from_this();
        m_ioManager->registerEvent(&m_sendEvent);
        int ret = WSASend(m_sock, &wsabuf, 1, NULL, flags,
            &m_sendEvent.overlapped, NULL);
        DWORD dwLastError = GetLastError();
        if (ret && dwLastError != WSA_IO_PENDING) {
            LOG_ERROR(g_log) << this << " send(" << m_sock << ", " << len
                << "): (" << lastError() << ")";
            m_ioManager->unregisterEvent(&m_sendEvent);
            throwExceptionFromLastError(dwLastError, "WSASend");
        }
        if (m_cancelledSend) {
            LOG_ERROR(g_log) << this << " send(" << m_sock << ", " << len
                << "): (" << m_cancelledSend << ")";
            m_ioManager->cancelEvent((HANDLE)m_sock, &m_sendEvent);
            Scheduler::getThis()->yieldTo();
            throwExceptionFromLastError(m_cancelledSend, "WSASend");
        }
        Timer::ptr timeout;
        if (m_sendTimeout != ~0ull)
            timeout = m_ioManager->registerTimer(m_sendTimeout, boost::bind(
                &IOManagerIOCP::cancelEvent, m_ioManager, (HANDLE)m_sock, &m_receiveEvent));
        Scheduler::getThis()->yieldTo();
        if (timeout)
            timeout->cancel();
        if (!m_sendEvent.ret) {
            if (m_sendEvent.lastError == ERROR_OPERATION_ABORTED &&
                m_cancelledSend != ERROR_OPERATION_ABORTED)
                m_sendEvent.lastError = WSAETIMEDOUT;
            LOG_ERROR(g_log) << this << " send(" << m_sock << ", "
                << len << "): (" << m_sendEvent.lastError << ")";
            throwExceptionFromLastError(m_sendEvent.lastError, "WSASend");
        }
        LOG_VERBOSE(g_log) << this << " send(" << m_sock << ", "
            << len << "): " << m_sendEvent.numberOfBytes;
        return m_sendEvent.numberOfBytes;
    } else
#endif
    {
        ptr self = shared_from_this();
        if (len > 0x7fffffff)
            len = 0x7fffffff;
        int rc = ::send(m_sock, (const char*)buf, (socklen_t)len, flags);
#ifndef WINDOWS
        while (m_ioManager && rc == -1 && errno == EAGAIN) {
            m_ioManager->registerEvent(m_sock, IOManager::WRITE);
            if (m_cancelledSend) {
                LOG_ERROR(g_log) << this << " send(" << m_sock << ", " << len
                    << "): (" << m_cancelledSend << ")";
                m_ioManager->cancelEvent(m_sock, IOManager::WRITE);
                Scheduler::getThis()->yieldTo();
                throwExceptionFromLastError(m_cancelledSend, "send");
            }
            Timer::ptr timeout;
            if (m_sendTimeout != ~0ull)
                timeout = m_ioManager->registerTimer(m_sendTimeout, boost::bind(
                    &Socket::cancelIo, this, IOManager::WRITE,
                    boost::ref(m_cancelledSend), ETIMEDOUT));
            Scheduler::getThis()->yieldTo();
            if (timeout)
                timeout->cancel();
            if (m_cancelledSend) {
                LOG_ERROR(g_log) << this << " send(" << m_sock << ", "
                    << len << "): (" << m_cancelledSend << ")";
                throwExceptionFromLastError(m_cancelledSend, "send");
            }
            rc = ::send(m_sock, buf, len, flags);
        }
#endif
        LOG_LEVEL(g_log, rc == -1 ? Log::ERROR : Log::VERBOSE) << this
            << " send(" << m_sock << ", " << len << "): " << rc << " ("
            << lastError() << ")";
        if (rc == -1) {
            throwExceptionFromLastError("send");
        }
        return rc;
    }
}

size_t
Socket::send(const iovec *bufs, size_t len, int flags)
{
#if !defined(WINDOWS) && !defined(OSX)
    flags |= MSG_NOSIGNAL;
#endif

#ifdef WINDOWS
    if (m_ioManager) {
        ptr self = shared_from_this();
        m_ioManager->registerEvent(&m_sendEvent);
        ASSERT(len <= 0xffffffff);
        int ret = WSASend(m_sock, (LPWSABUF)bufs, (DWORD)len, NULL, flags,
            &m_sendEvent.overlapped, NULL);
        DWORD dwLastError = GetLastError();
        if (ret && dwLastError != WSA_IO_PENDING) {
            LOG_ERROR(g_log) << this << " sendv(" << m_sock << ", " << len
                << "): (" << lastError() << ")";
            m_ioManager->unregisterEvent(&m_sendEvent);
            throwExceptionFromLastError(dwLastError, "WSASend");
        }
        if (m_cancelledSend) {
            LOG_ERROR(g_log) << this << " sendv(" << m_sock << ", " << len
                << "): (" << m_cancelledSend << ")";
            m_ioManager->cancelEvent((HANDLE)m_sock, &m_sendEvent);
            Scheduler::getThis()->yieldTo();
            throwExceptionFromLastError(m_cancelledSend, "WSASend");
        }
        Timer::ptr timeout;
        if (m_sendTimeout != ~0ull)
            timeout = m_ioManager->registerTimer(m_sendTimeout, boost::bind(
                &IOManagerIOCP::cancelEvent, m_ioManager, (HANDLE)m_sock, &m_receiveEvent));
        Scheduler::getThis()->yieldTo();
        if (timeout)
            timeout->cancel();
        if (!m_sendEvent.ret) {
            if (m_sendEvent.lastError == ERROR_OPERATION_ABORTED &&
                m_cancelledSend != ERROR_OPERATION_ABORTED)
                m_sendEvent.lastError = WSAETIMEDOUT;
            LOG_ERROR(g_log) << this << " sendv(" << m_sock << ", "
                << len << "): (" << m_sendEvent.lastError << ")";
            throwExceptionFromLastError(m_sendEvent.lastError, "WSASend");
        }
        LOG_VERBOSE(g_log) << this << " sendv(" << m_sock << ", "
            << len << "): " << m_sendEvent.numberOfBytes;
        return m_sendEvent.numberOfBytes;
    } else {
        DWORD sent;
        ASSERT(len <= 0xffffffff);
        if (WSASend(m_sock, (LPWSABUF)bufs, (DWORD)len, &sent, flags,
            NULL, NULL)) {
            LOG_ERROR(g_log) << this << " sendv(" << m_sock << ", "
                << len << "): (" << lastError() << ")";
            throwExceptionFromLastError("WSASend");
        }
        LOG_VERBOSE(g_log) << this << " sendv(" << m_sock << ", "
            << len << "): " << sent;
        return sent;
    }
#else
    ptr self = shared_from_this();
    msghdr msg;
    memset(&msg, 0, sizeof(msghdr));
    msg.msg_iov = (iovec*)bufs;
    msg.msg_iovlen = len;
    int rc = ::sendmsg(m_sock, &msg, flags);
    while (m_ioManager && rc == -1 && errno == EAGAIN) {
        m_ioManager->registerEvent(m_sock, IOManager::WRITE);
        if (m_cancelledSend) {
            LOG_ERROR(g_log) << this << " send(" << m_sock << ", " << len
                << "): (" << m_cancelledSend << ")";
            m_ioManager->cancelEvent(m_sock, IOManager::WRITE);
            Scheduler::getThis()->yieldTo();
            throwExceptionFromLastError(m_cancelledSend, "sendmsg");
        }
        Timer::ptr timeout;
        if (m_sendTimeout != ~0ull)
            timeout = m_ioManager->registerTimer(m_sendTimeout, boost::bind(
                &Socket::cancelIo, this, IOManager::WRITE,
                boost::ref(m_cancelledSend), ETIMEDOUT));
        Scheduler::getThis()->yieldTo();
        if (timeout)
            timeout->cancel();
        if (m_cancelledSend) {
            LOG_ERROR(g_log) << this << " sendv(" << m_sock << ", "
                    << len << "): (" << m_cancelledSend << ")";
            throwExceptionFromLastError(m_cancelledSend, "sendmsg");
        }
        rc = ::sendmsg(m_sock, &msg, flags);
    }
    LOG_LEVEL(g_log, rc == -1 ? Log::ERROR : Log::VERBOSE) << this
            << " sendv(" << m_sock << ", " << len << "): " << rc << " ("
            << lastError() << ")";
    if (rc == -1) {
        throwExceptionFromLastError("sendmsg");
    }
    return rc;
#endif
}

size_t
Socket::sendTo(const void *buf, size_t len, int flags, const Address &to)
{
    ASSERT(to.family() == family());
#ifdef WINDOWS
    if (m_ioManager) {
        if (len > 0xfffffff)
            len = 0xffffffff;
        WSABUF wsabuf;
        wsabuf.buf = (char*)buf;
        wsabuf.len = (unsigned int)len;
        ptr self = shared_from_this();
        m_ioManager->registerEvent(&m_sendEvent);
        int ret = WSASendTo(m_sock, &wsabuf, 1, NULL, flags,
            to.name(), to.nameLen(),
            &m_sendEvent.overlapped, NULL);
        DWORD dwLastError = GetLastError();
        if (ret && dwLastError != WSA_IO_PENDING) {
            LOG_ERROR(g_log) << this << " sendto(" << m_sock << ", " << len
                << ", " << to << "): (" << lastError() << ")";
            m_ioManager->unregisterEvent(&m_sendEvent);
            throwExceptionFromLastError(dwLastError, "WSASendTo");
        }
        if (m_cancelledSend) {
            LOG_ERROR(g_log) << this << " sendto(" << m_sock << ", " << len
                << ", " << to << "): (" << m_cancelledSend << ")";
            m_ioManager->cancelEvent((HANDLE)m_sock, &m_sendEvent);
            Scheduler::getThis()->yieldTo();
            throwExceptionFromLastError(m_cancelledSend, "WSASendTo");
        }
        Timer::ptr timeout;
        if (m_sendTimeout != ~0ull)
            timeout = m_ioManager->registerTimer(m_sendTimeout, boost::bind(
                &IOManagerIOCP::cancelEvent, m_ioManager, (HANDLE)m_sock, &m_receiveEvent));
        Scheduler::getThis()->yieldTo();
        if (timeout)
            timeout->cancel();
        if (!m_sendEvent.ret) {
            if (m_sendEvent.lastError == ERROR_OPERATION_ABORTED &&
                m_cancelledSend != ERROR_OPERATION_ABORTED)
                m_sendEvent.lastError = WSAETIMEDOUT;
            LOG_ERROR(g_log) << this << " sendv(" << m_sock << ", "
                << len << ", " << to << "): (" << m_sendEvent.lastError << ")";
            throwExceptionFromLastError(m_sendEvent.lastError, "WSASendTo");
        }
        LOG_VERBOSE(g_log) << this << " sendv(" << m_sock << ", "
            << len << ", " << to << "): " << m_sendEvent.numberOfBytes;
        return m_sendEvent.numberOfBytes;
    } else
#endif
    {
        int rc = ::sendto(m_sock, (const char*)buf, (socklen_t)len, flags, to.name(), to.nameLen());
#ifndef WINDOWS
        ptr self = shared_from_this();
        while (m_ioManager && rc == -1 && errno == EAGAIN) {
            m_ioManager->registerEvent(m_sock, IOManager::WRITE);
            if (m_cancelledSend) {
                LOG_ERROR(g_log) << this << " sendto(" << m_sock << ", " << len
                    << ", " << to << "): (" << m_cancelledSend << ")";
                m_ioManager->cancelEvent(m_sock, IOManager::WRITE);
                Scheduler::getThis()->yieldTo();
                throwExceptionFromLastError(m_cancelledSend, "sendto");
            }
            Timer::ptr timeout;
            if (m_sendTimeout != ~0ull)
                timeout = m_ioManager->registerTimer(m_sendTimeout, boost::bind(
                    &Socket::cancelIo, this, IOManager::WRITE,
                    boost::ref(m_cancelledSend), ETIMEDOUT));
            Scheduler::getThis()->yieldTo();
            if (timeout)
                timeout->cancel();
            if (m_cancelledSend) {
                LOG_ERROR(g_log) << this << " sendto(" << m_sock << ", "
                    << len << ", " << to << "): (" << m_cancelledSend << ")";
                throwExceptionFromLastError(m_cancelledSend, "sendto");
            }
            rc = ::sendto(m_sock, buf, len, flags, to.name(), to.nameLen());
        }
#endif
        LOG_LEVEL(g_log, rc == -1 ? Log::ERROR : Log::VERBOSE) << this
            << " sendto(" << m_sock << ", " << len << ", " << to << "): "
            << rc << " (" << lastError() << ")";
        if (rc == -1) {
            throwExceptionFromLastError("sendto");
        }
        return rc;
    }
}

size_t
Socket::sendTo(const iovec *bufs, size_t len, int flags, const Address &to)
{
    ASSERT(to.family() == family());
#ifdef WINDOWS
    if (m_ioManager) {
        ptr self = shared_from_this();
        m_ioManager->registerEvent(&m_sendEvent);
        ASSERT(len <= 0xffffffff);
        int ret = WSASendTo(m_sock, (LPWSABUF)bufs, (DWORD)len, NULL, flags,
            to.name(), to.nameLen(),
            &m_sendEvent.overlapped, NULL);
        DWORD dwLastError = GetLastError();
        if (ret && dwLastError != WSA_IO_PENDING) {
            LOG_ERROR(g_log) << this << " sendtov(" << m_sock << ", " << len
                << ", " << to << "): (" << lastError() << ")";
            m_ioManager->unregisterEvent(&m_sendEvent);
            throwExceptionFromLastError(dwLastError, "WSASendTo");
        }
        if (m_cancelledSend) {
            LOG_ERROR(g_log) << this << " sendtov(" << m_sock << ", " << len
                << ", " << to << "): (" << m_cancelledSend << ")";
            m_ioManager->cancelEvent((HANDLE)m_sock, &m_sendEvent);
            Scheduler::getThis()->yieldTo();
            throwExceptionFromLastError(m_cancelledSend, "WSASendTo");
        }
        Timer::ptr timeout;
        if (m_sendTimeout != ~0ull)
            timeout = m_ioManager->registerTimer(m_sendTimeout, boost::bind(
                &IOManagerIOCP::cancelEvent, m_ioManager, (HANDLE)m_sock, &m_receiveEvent));
        Scheduler::getThis()->yieldTo();
        if (timeout)
            timeout->cancel();
        if (!m_sendEvent.ret) {
            if (m_sendEvent.lastError == ERROR_OPERATION_ABORTED &&
                m_cancelledSend != ERROR_OPERATION_ABORTED)
                m_sendEvent.lastError = WSAETIMEDOUT;
            LOG_ERROR(g_log) << this << " sendtov(" << m_sock << ", "
                << len << ", " << to << "): (" << m_sendEvent.lastError << ")";
            throwExceptionFromLastError(m_sendEvent.lastError, "WSASendTo");
        }
        return m_sendEvent.numberOfBytes;
    } else {
        DWORD sent;
        ASSERT(len <= 0xffffffff);
        if (WSASendTo(m_sock, (LPWSABUF)bufs, (DWORD)len, &sent, flags,
            to.name(), to.nameLen(),
            NULL, NULL)) {
            LOG_ERROR(g_log) << this << " sendtov(" << m_sock << ", "
                << len << ", " << to << "): (" << lastError() << ")";
            throwExceptionFromLastError("WSASendTo");
        }
        LOG_VERBOSE(g_log) << this << " sendtov(" << m_sock << ", "
            << len << ", " << to << "): " << sent;
        return sent;
    }
#else
    ptr self = shared_from_this();
    msghdr msg;
    memset(&msg, 0, sizeof(msghdr));
    msg.msg_iov = (iovec*)bufs;
    msg.msg_iovlen = len;
    msg.msg_name = (sockaddr*)to.name();
    msg.msg_namelen = to.nameLen();
    int rc = ::sendmsg(m_sock, &msg, flags);
    while (m_ioManager && rc == -1 && errno == EAGAIN) {
        m_ioManager->registerEvent(m_sock, IOManager::WRITE);
        if (m_cancelledSend) {
            LOG_ERROR(g_log) << this << " sendtov(" << m_sock << ", " << len
                << ", " << to << "): (" << m_cancelledSend << ")";
            m_ioManager->cancelEvent(m_sock, IOManager::WRITE);
            Scheduler::getThis()->yieldTo();
            throwExceptionFromLastError(m_cancelledSend, "sendmsg");
        }
        Timer::ptr timeout;
        if (m_sendTimeout != ~0ull)
            timeout = m_ioManager->registerTimer(m_sendTimeout, boost::bind(
                &Socket::cancelIo, this, IOManager::WRITE,
                boost::ref(m_cancelledSend), ETIMEDOUT));
        Scheduler::getThis()->yieldTo();
        if (timeout)
            timeout->cancel();
        if (m_cancelledSend) {
            LOG_ERROR(g_log) << this << " sendtov(" << m_sock << ", "
                    << len << ", " << to << "): (" << m_cancelledSend << ")";
            throwExceptionFromLastError(m_cancelledSend, "sendmsg");
        }
        rc = ::sendmsg(m_sock, &msg, flags);
    }
    LOG_LEVEL(g_log, rc == -1 ? Log::ERROR : Log::VERBOSE) << this
            << " sendtov(" << m_sock << ", " << len << ", " << to << "): "
            << rc << " (" << lastError() << ")";
    if (rc == -1) {
        throwExceptionFromLastError("sendmsg");
    }
    return rc;
#endif
}

size_t
Socket::receive(void *buf, size_t len, int flags)
{
#ifdef WINDOWS
    if (m_ioManager) {
        if (len > 0xffffffff)
            len = 0xffffffff;
        WSABUF wsabuf;
        wsabuf.buf = (char*)buf;
        wsabuf.len = (unsigned int)len;
        ptr self = shared_from_this();
        m_ioManager->registerEvent(&m_receiveEvent);
        int ret = WSARecv(m_sock, &wsabuf, 1, NULL, (LPDWORD)&flags,
            &m_receiveEvent.overlapped, NULL);
        DWORD dwLastError = GetLastError();
        if (ret && dwLastError != WSA_IO_PENDING) {
            LOG_ERROR(g_log) << this << " recv(" << m_sock << ", "
                << len << "): (" << lastError() << ")";
            m_ioManager->unregisterEvent(&m_receiveEvent);
            throwExceptionFromLastError(dwLastError, "WSARecv");
        }
        if (m_cancelledReceive) {
            LOG_ERROR(g_log) << this << " recv(" << m_sock << ", " << len
                << "): (" << m_cancelledReceive << ")";
            m_ioManager->cancelEvent((HANDLE)m_sock, &m_receiveEvent);
            Scheduler::getThis()->yieldTo();
            throwExceptionFromLastError(m_cancelledReceive, "WSARecv");
        }
        Timer::ptr timeout;
        if (m_receiveTimeout != ~0ull)
            timeout = m_ioManager->registerTimer(m_receiveTimeout, boost::bind(
                &IOManagerIOCP::cancelEvent, m_ioManager, (HANDLE)m_sock, &m_receiveEvent));
        Scheduler::getThis()->yieldTo();
        if (timeout)
            timeout->cancel();
        if (!m_receiveEvent.ret) {
            if (m_receiveEvent.lastError == ERROR_OPERATION_ABORTED &&
                m_cancelledReceive != ERROR_OPERATION_ABORTED)
                m_receiveEvent.lastError = WSAETIMEDOUT;
            LOG_ERROR(g_log) << this << " recv(" << m_sock << ", "
                << len << "): (" << m_receiveEvent.lastError << ")";
            throwExceptionFromLastError(m_receiveEvent.lastError, "WSARecv");
        }
        LOG_VERBOSE(g_log) << this << " recv(" << m_sock << ", "
            << len << "): " << m_receiveEvent.numberOfBytes;
        return m_receiveEvent.numberOfBytes;
    } else
#endif
    {
        int rc = ::recv(m_sock, (char*)buf, (socklen_t)len, flags);
#ifndef WINDOWS
        ptr self = shared_from_this();
        while (m_ioManager && rc == -1 && errno == EAGAIN) {
            m_ioManager->registerEvent(m_sock, IOManager::READ);
            if (m_cancelledReceive) {
                LOG_ERROR(g_log) << this << " recv(" << m_sock << ", " << len
                    << "): (" << m_cancelledReceive << ")";
                m_ioManager->cancelEvent(m_sock, IOManager::READ);
                Scheduler::getThis()->yieldTo();
                throwExceptionFromLastError(m_cancelledReceive, "recv");
            }
            Timer::ptr timeout;
            if (m_receiveTimeout != ~0ull)
                timeout = m_ioManager->registerTimer(m_receiveTimeout, boost::bind(
                    &Socket::cancelIo, this, IOManager::READ,
                    boost::ref(m_cancelledReceive), ETIMEDOUT));
            Scheduler::getThis()->yieldTo();
            if (timeout)
                timeout->cancel();
            if (m_cancelledReceive) {
                LOG_ERROR(g_log) << this << " recv(" << m_sock << ", "
                    << len << "): (" << m_cancelledReceive << ")";
                throwExceptionFromLastError(m_cancelledReceive, "recv");
            }
            rc = ::recv(m_sock, buf, len, flags);
        }
#endif
        LOG_LEVEL(g_log, rc == -1 ? Log::ERROR : Log::VERBOSE) << this
            << " recv(" << m_sock << ", " << len << "): " << rc << " ("
            << lastError() << ")";
        if (rc == -1) {
            throwExceptionFromLastError("recv");
        }
        return rc;
    }
}

size_t
Socket::receive(iovec *bufs, size_t len, int flags)
{
#ifdef WINDOWS
    if (m_ioManager) {
        ptr self = shared_from_this();
        m_ioManager->registerEvent(&m_receiveEvent);
        ASSERT(len <= 0xffffffff);
        int ret = WSARecv(m_sock, (LPWSABUF)bufs, (DWORD)len, NULL, (LPDWORD)&flags,
            &m_receiveEvent.overlapped, NULL);
        DWORD dwLastError = GetLastError();
        if (ret && dwLastError != WSA_IO_PENDING) {
            LOG_ERROR(g_log) << this << " recvv(" << m_sock << ", "
                << len << "): (" << lastError() << ")";
            m_ioManager->unregisterEvent(&m_receiveEvent);
            throwExceptionFromLastError(dwLastError, "WSARecv");
        }
        if (m_cancelledReceive) {
            LOG_ERROR(g_log) << this << " recvv(" << m_sock << ", " << len
                << "): (" << m_cancelledReceive << ")";
            m_ioManager->cancelEvent((HANDLE)m_sock, &m_receiveEvent);
            Scheduler::getThis()->yieldTo();
            throwExceptionFromLastError(m_cancelledReceive, "WSARecv");
        }
        Timer::ptr timeout;
        if (m_receiveTimeout != ~0ull)
            timeout = m_ioManager->registerTimer(m_receiveTimeout, boost::bind(
                &IOManagerIOCP::cancelEvent, m_ioManager, (HANDLE)m_sock, &m_receiveEvent));
        Scheduler::getThis()->yieldTo();
        if (timeout)
            timeout->cancel();
        if (!m_receiveEvent.ret) {
            if (m_receiveEvent.lastError == ERROR_OPERATION_ABORTED &&
                m_cancelledReceive != ERROR_OPERATION_ABORTED)
                m_receiveEvent.lastError = WSAETIMEDOUT;
            LOG_ERROR(g_log) << this << " recvv(" << m_sock << ", "
                << len << "): (" << m_receiveEvent.lastError << ")";
            throwExceptionFromLastError(m_receiveEvent.lastError, "WSARecv");
        }
        LOG_VERBOSE(g_log) << this << " recvv(" << m_sock << ", "
            << len << "): " << m_receiveEvent.numberOfBytes;
        return m_receiveEvent.numberOfBytes;
    } else {
        DWORD received;
        ASSERT(len <= 0xffffffff);
        if (WSARecv(m_sock, (LPWSABUF)bufs, (DWORD)len, &received, (LPDWORD)&flags,
            NULL, NULL)) {
            LOG_ERROR(g_log) << this << " recvv(" << m_sock << ", "
                << len << "): (" << lastError() << ")";
            throwExceptionFromLastError("WSARecv");
        }
        LOG_VERBOSE(g_log) << this << " recvv(" << m_sock << ", "
            << len << "): " << received;
        return received;
    }
#else
    ptr self = shared_from_this();
    msghdr msg;
    memset(&msg, 0, sizeof(msghdr));
    msg.msg_iov = bufs;
    msg.msg_iovlen = len;
    int rc = ::recvmsg(m_sock, &msg, flags);
    while (m_ioManager && rc == -1 && errno == EAGAIN) {
        m_ioManager->registerEvent(m_sock, IOManager::READ);
        if (m_cancelledReceive) {
            LOG_ERROR(g_log) << this << " recvv(" << m_sock << ", " << len
                << "): (" << m_cancelledReceive << ")";
            m_ioManager->cancelEvent(m_sock, IOManager::READ);
            Scheduler::getThis()->yieldTo();
            throwExceptionFromLastError(m_cancelledReceive, "recvmsg");
        }
        Timer::ptr timeout;
        if (m_receiveTimeout != ~0ull)
            timeout = m_ioManager->registerTimer(m_receiveTimeout, boost::bind(
                &Socket::cancelIo, this, IOManager::READ,
                boost::ref(m_cancelledReceive), ETIMEDOUT));
        Scheduler::getThis()->yieldTo();
        if (timeout)
            timeout->cancel();
        if (m_cancelledReceive) {
            LOG_ERROR(g_log) << this << " recvv(" << m_sock << ", "
                    << len << "): (" << m_cancelledReceive << ")";
            throwExceptionFromLastError(m_cancelledReceive, "recvmsg");
        }
        rc = ::recvmsg(m_sock, &msg, flags);
    }
    LOG_LEVEL(g_log, rc == -1 ? Log::ERROR : Log::VERBOSE) << this
            << " recvv(" << m_sock << ", " << len << "): " << rc << " ("
            << lastError() << ")";
    if (rc == -1) {
        throwExceptionFromLastError("recvmsg");
    }
    return rc;
#endif
}

size_t
Socket::receiveFrom(void *buf, size_t len, int *flags, Address &from)
{
    ASSERT(from.family() == family());
#ifdef WINDOWS
    if (len > 0xffffffff)
        len = 0xffffffff;
    WSABUF wsabuf;
    wsabuf.buf = (char*)buf;
    wsabuf.len = (unsigned int)len;
    int namelen = from.nameLen();
    if (m_ioManager) {
        ptr self = shared_from_this();
        m_ioManager->registerEvent(&m_sendEvent);        
        int ret = WSARecvFrom(m_sock, &wsabuf, 1, NULL, (LPDWORD)flags,
            from.name(), &namelen,
            &m_receiveEvent.overlapped, NULL);
        DWORD dwLastError = GetLastError();
        if (ret && dwLastError != WSA_IO_PENDING) {
            LOG_ERROR(g_log) << this << " recvfrom(" << m_sock << ", "
                << len << "): (" << lastError() << ")";
            m_ioManager->unregisterEvent(&m_receiveEvent);
            throwExceptionFromLastError(dwLastError, "WSARecvFrom");
        }
        if (m_cancelledReceive) {
            LOG_ERROR(g_log) << this << " recvfrom(" << m_sock << ", " << len
                << "): (" << m_cancelledReceive << ")";
            m_ioManager->cancelEvent((HANDLE)m_sock, &m_receiveEvent);
            Scheduler::getThis()->yieldTo();
            throwExceptionFromLastError(m_cancelledReceive, "WSARecvFrom");
        }
        Timer::ptr timeout;
        if (m_receiveTimeout != ~0ull)
            timeout = m_ioManager->registerTimer(m_receiveTimeout, boost::bind(
                &IOManagerIOCP::cancelEvent, m_ioManager, (HANDLE)m_sock, &m_receiveEvent));
        Scheduler::getThis()->yieldTo();
        if (timeout)
            timeout->cancel();
        if (!m_receiveEvent.ret) {
            if (m_receiveEvent.lastError == ERROR_OPERATION_ABORTED &&
                m_cancelledReceive != ERROR_OPERATION_ABORTED)
                m_receiveEvent.lastError = WSAETIMEDOUT;
            LOG_ERROR(g_log) << this << " recvfrom(" << m_sock << ", "
                << len << "): (" << m_receiveEvent.lastError << ")";
            throwExceptionFromLastError(m_receiveEvent.lastError, "WSARecvFrom");
        }
        LOG_VERBOSE(g_log) << this << " recvfrom(" << m_sock << ", "
            << len << "): " << m_receiveEvent.numberOfBytes << ", " << from;
        return m_receiveEvent.numberOfBytes;
    } else {
        DWORD sent;
        if (WSARecvFrom(m_sock, &wsabuf, 1, &sent, (LPDWORD)flags,
            from.name(), &namelen,
            NULL, NULL)) {
            LOG_ERROR(g_log) << this << " recvfrom(" << m_sock << ", "
                << len << "): (" << lastError() << ")";
            throwExceptionFromLastError("WSARecvFrom");
        }
        LOG_VERBOSE(g_log) << this << " recvfrom(" << m_sock << ", "
            << len << "): " << sent << ", " << from;
        return sent;
    }
#else
    ptr self = shared_from_this();
    msghdr msg;
    memset(&msg, 0, sizeof(msghdr));
    iovec iov;
    iov.iov_base = buf;
    iov.iov_len = len;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_name = from.name();
    msg.msg_namelen = from.nameLen();
    int rc = ::recvmsg(m_sock, &msg, *flags);
    while (m_ioManager && rc == -1 && errno == EAGAIN) {
        m_ioManager->registerEvent(m_sock, IOManager::READ);
        if (m_cancelledReceive) {
            LOG_ERROR(g_log) << this << " recvfrom(" << m_sock << ", " << len
                << "): (" << m_cancelledReceive << ")";
            m_ioManager->cancelEvent(m_sock, IOManager::READ);
            Scheduler::getThis()->yieldTo();
            throwExceptionFromLastError(m_cancelledReceive, "recvmsg");
        }
        Timer::ptr timeout;
        if (m_receiveTimeout != ~0ull)
            timeout = m_ioManager->registerTimer(m_receiveTimeout, boost::bind(
                &Socket::cancelIo, this, IOManager::READ,
                boost::ref(m_cancelledReceive), ETIMEDOUT));
        Scheduler::getThis()->yieldTo();
        if (timeout)
            timeout->cancel();
        if (m_cancelledReceive) {
            LOG_ERROR(g_log) << this << " recvfrom(" << m_sock << ", "
                << len << "): (" << m_cancelledReceive << ")";
            throwExceptionFromLastError(m_cancelledReceive, "recvmsg");
        }
        rc = ::recvmsg(m_sock, &msg, *flags);
    }
    LOG_LEVEL(g_log, rc == -1 ? Log::ERROR : Log::VERBOSE) << this
        << " recvfrom(" << m_sock << ", " << len << "): "
        << rc << ", " << from << " (" << lastError() << ")";
    if (rc == -1) {
        throwExceptionFromLastError("recvmsg");
    }
    *flags = msg.msg_flags;
    return rc;
#endif
}

size_t
Socket::receiveFrom(iovec *bufs, size_t len, int *flags, Address &from)
{
    ASSERT(from.family() == family());
#ifdef WINDOWS
    int namelen = from.nameLen();
    if (m_ioManager) {
        ptr self = shared_from_this();
        m_ioManager->registerEvent(&m_receiveEvent);
        ASSERT(len <= 0xffffffff);
        int ret = WSARecvFrom(m_sock, (LPWSABUF)bufs, (DWORD)len, NULL, (LPDWORD)flags,
            from.name(), &namelen,
            &m_receiveEvent.overlapped, NULL);
        DWORD dwLastError = GetLastError();
        if (ret && dwLastError != WSA_IO_PENDING) {
            LOG_ERROR(g_log) << this << " recvfromv(" << m_sock << ", " << len
                << "): (" << lastError() << ")";
            m_ioManager->unregisterEvent(&m_receiveEvent);
            throwExceptionFromLastError(dwLastError, "WSARecvFrom");
        }
        if (m_cancelledReceive) {
            LOG_ERROR(g_log) << this << " recvfromv(" << m_sock << ", " << len
                << "): (" << m_cancelledReceive << ")";
            m_ioManager->cancelEvent((HANDLE)m_sock, &m_receiveEvent);
            Scheduler::getThis()->yieldTo();
            throwExceptionFromLastError(m_cancelledReceive, "WSARecvFrom");
        }
        Timer::ptr timeout;
        if (m_receiveTimeout != ~0ull)
            timeout = m_ioManager->registerTimer(m_receiveTimeout, boost::bind(
                &IOManagerIOCP::cancelEvent, m_ioManager, (HANDLE)m_sock, &m_receiveEvent));
        Scheduler::getThis()->yieldTo();
        if (timeout)
            timeout->cancel();
        if (!m_receiveEvent.ret) {
            if (m_receiveEvent.lastError == ERROR_OPERATION_ABORTED &&
                m_cancelledReceive != ERROR_OPERATION_ABORTED)
                m_receiveEvent.lastError = WSAETIMEDOUT;
            LOG_ERROR(g_log) << this << " recvfrom(" << m_sock << ", "
                << len << "): (" << m_receiveEvent.lastError << ")";
            throwExceptionFromLastError(m_receiveEvent.lastError, "WSARecvFrom");
        }
        LOG_VERBOSE(g_log) << this << " recvfrom(" << m_sock << ", "
            << len << "): " << m_receiveEvent.numberOfBytes << ", " << from;
        return m_receiveEvent.numberOfBytes;
    } else {
        DWORD sent;
        ASSERT(len <= 0xffffffff);
        if (WSARecvFrom(m_sock, (LPWSABUF)bufs, (DWORD)len, &sent, (LPDWORD)flags,
            from.name(), &namelen,
            NULL, NULL)) {
            LOG_ERROR(g_log) << this << " recvfromv(" << m_sock << ", "
                << len << "): (" << lastError() << ")";
            throwExceptionFromLastError("WSARecvFrom");
        }
        LOG_VERBOSE(g_log) << this << " recvfromv(" << m_sock << ", "
            << len << "): " << sent << ", " << from;
        return sent;
    }
#else
    ptr self = shared_from_this();
    msghdr msg;
    memset(&msg, 0, sizeof(msghdr));
    msg.msg_iov = bufs;
    msg.msg_iovlen = len;
    msg.msg_name = from.name();
    msg.msg_namelen = from.nameLen();
    int rc = ::recvmsg(m_sock, &msg, *flags);
    while (m_ioManager && rc == -1 && errno == EAGAIN) {
        m_ioManager->registerEvent(m_sock, IOManager::READ);
        if (m_cancelledReceive) {
            LOG_ERROR(g_log) << this << " recvfromv(" << m_sock << ", " << len
                << "): (" << m_cancelledReceive << ")";
            m_ioManager->cancelEvent(m_sock, IOManager::READ);
            Scheduler::getThis()->yieldTo();
            throwExceptionFromLastError(m_cancelledReceive, "recvmsg");
        }
        Timer::ptr timeout;
        if (m_receiveTimeout != ~0ull)
            timeout = m_ioManager->registerTimer(m_receiveTimeout, boost::bind(
                &Socket::cancelIo, this, IOManager::READ,
                boost::ref(m_cancelledReceive), ETIMEDOUT));
        Scheduler::getThis()->yieldTo();
        if (timeout)
            timeout->cancel();
        if (m_cancelledReceive) {
            LOG_ERROR(g_log) << this << " recvfromv(" << m_sock << ", "
                << len << "): (" << m_cancelledReceive << ")";
            throwExceptionFromLastError(m_cancelledReceive, "recvmsg");
        }
        rc = ::recvmsg(m_sock, &msg, *flags);
    }
    LOG_LEVEL(g_log, rc == -1 ? Log::ERROR : Log::VERBOSE) << this
        << " recvfromv(" << m_sock << ", " << len << "): "
        << rc << ", " << from << " (" << lastError() << ")";
    if (rc == -1) {
        throwExceptionFromLastError("recvmsg");
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
        throwExceptionFromLastError("getsockopt");
    }
}

void
Socket::setOption(int level, int option, const void *value, size_t len)
{
    if (setsockopt(m_sock, level, option, (const char*)value, (socklen_t)len)) {
        throwExceptionFromLastError("setsockopt");
    }
}

void
Socket::cancelAccept()
{
    ASSERT(m_ioManager);
#ifdef WINDOWS
    if (m_cancelledReceive)
        return;
    m_cancelledReceive = ERROR_OPERATION_ABORTED;
    if (pAcceptEx) {
        m_ioManager->cancelEvent((HANDLE)m_sock, &m_receiveEvent);
    } else {
        m_unregistered = m_ioManager->unregisterEvent(m_hEvent);
        m_scheduler->schedule(m_fiber);
    }
#else
    cancelIo(IOManager::READ, m_cancelledReceive, ECANCELED);
#endif 
}

void
Socket::cancelConnect()
{
    ASSERT(m_ioManager);
#ifdef WINDOWS
    if (m_cancelledSend)
        return;
    m_cancelledSend = ERROR_OPERATION_ABORTED;
    if (ConnectEx) {
        m_ioManager->cancelEvent((HANDLE)m_sock, &m_sendEvent);
    } else {
        m_unregistered = m_ioManager->unregisterEvent(m_hEvent);
        m_scheduler->schedule(m_fiber);
    }
#else
    cancelIo(IOManager::WRITE, m_cancelledSend, ECANCELED);
#endif 
}

void
Socket::cancelSend()
{
    ASSERT(m_ioManager);
#ifdef WINDOWS
    if (m_cancelledSend)
        return;
    m_cancelledSend = ERROR_OPERATION_ABORTED;
    m_ioManager->cancelEvent((HANDLE)m_sock, &m_sendEvent);
#else
    cancelIo(IOManager::WRITE, m_cancelledSend, ECANCELED);
#endif 
}

void
Socket::cancelReceive()
{
    ASSERT(m_ioManager);
#ifdef WINDOWS
    if (m_cancelledReceive)
        return;
    m_cancelledReceive = ERROR_OPERATION_ABORTED;
    m_ioManager->cancelEvent((HANDLE)m_sock, &m_receiveEvent);
#else
    cancelIo(IOManager::READ, m_cancelledReceive, ECANCELED);
#endif 
}

#ifndef WINDOWS
void
Socket::cancelIo(IOManager::Event event, error_t &cancelled, error_t error)
{
    ASSERT(error);
    if (cancelled)
        return;
    cancelled = error;
    m_ioManager->cancelEvent(m_sock, event);
}
#endif

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
        throwExceptionFromLastError("getpeername");
    }
    ASSERT(namelen <= result->nameLen());
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
        throwExceptionFromLastError("getsockname");
    }
    ASSERT(namelen <= result->nameLen());
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

std::vector<Address::ptr>
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
    std::string node;
    const char *service = NULL;
    // Check for [ipv6addr] (with optional :service)
    if (!host.empty() && host[0] == '[') {
        const char *endipv6 = (const char *)memchr(host.c_str() + 1, ']', host.size() - 1);
        if (endipv6) {
            if (*(endipv6 + 1) == ':') {
                service = endipv6 + 2;
            }
            node = host.substr(1, endipv6 - host.c_str() - 1);
        }
    }
    // Check for node:service
    if (node.empty()) {
        service = (const char*)memchr(host.c_str(), ':', host.size());
        if (service) {
            // More than 1 : means it's not node:service
            if (!memchr(service + 1, ':', host.c_str() + host.size() - service - 1)) {
                node = host.substr(0, service - host.c_str());
                ++service;
            } else {
                service = NULL;
            }
        }
    }
    if (node.empty())
        node = host;
    addrinfo *results, *next;
    if (getaddrinfo(node.c_str(), service, &hints, &results)) {
        throwExceptionFromLastError("getaddrinfo");
    }
    std::vector<Address::ptr> result;
    next = results;
    while (next) {
        Address::ptr addr;
        switch (next->ai_family) {
            case AF_INET:
                addr.reset(new IPv4Address(next->ai_socktype, next->ai_protocol));
                ASSERT(next->ai_addrlen <= (size_t)addr->nameLen());
                memcpy(addr->name(), next->ai_addr, next->ai_addrlen);
                break;
            case AF_INET6:
                addr.reset(new IPv6Address(next->ai_socktype, next->ai_protocol));
                ASSERT(next->ai_addrlen <= (size_t)addr->nameLen());
                memcpy(addr->name(), next->ai_addr, next->ai_addrlen);
                break;
            default:
                addr.reset(new UnknownAddress(next->ai_family, next->ai_socktype, next->ai_protocol));
                ASSERT(next->ai_addrlen <= (size_t)addr->nameLen());
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

std::ostream &
Address::insert(std::ostream &os) const
{
    return os << "(Unknown addr " << m_type << ")";
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

std::ostream &
IPv4Address::insert(std::ostream &os) const
{
    return os << (int)(sin.sin_addr.s_addr & 0xff) << '.'
        << (int)((sin.sin_addr.s_addr >> 8) & 0xff) << '.'
        << (int)((sin.sin_addr.s_addr >> 16) & 0xff) << '.'
        << (int)((sin.sin_addr.s_addr >> 24) & 0xff) << ':'
        << (int)htons(sin.sin_port);
}

IPv6Address::IPv6Address(int type, int protocol)
: IPAddress(type, protocol)
{
    sin.sin6_family = AF_INET6;
    sin.sin6_port = 0;
    in6_addr anyaddr = IN6ADDR_ANY_INIT;
    sin.sin6_addr = anyaddr;
}

std::ostream &
IPv6Address::insert(std::ostream &os) const
{
    std::ios_base::fmtflags flags = os.setf(std::ios_base::hex, std::ios_base::basefield);
    os << '[';
    unsigned short *addr = (unsigned short *)sin.sin6_addr.s6_addr;
    bool usedZeros = false;
    for (size_t i = 0; i < 8; ++i) {
        if (addr[i] == 0 && !usedZeros)
            continue;
        if (i != 0 && addr[i - 1] == 0 && !usedZeros) {
            os << ':';
            usedZeros = true;
        }
        if (i != 0)
            os << ':';
        os << (int)htons(addr[i]);
    }
    if (!usedZeros && addr[7] == 0)
        os << "::";
    
    os << "]:" << std::dec << (int)htons(sin.sin6_port);
    os.setf(flags, std::ios_base::basefield);
    return os;
}

UnknownAddress::UnknownAddress(int family, int type, int protocol)
: Address(type, protocol)
{
    sa.sa_family = family;
}

std::ostream &operator <<(std::ostream &os, const Address &addr)
{
    return addr.insert(os);
}
