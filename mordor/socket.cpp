// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "socket.h"

#include <boost/bind.hpp>

#include "assert.h"
#include "exception.h"
#include "log.h"
#include "mordor/string.h"
#include "version.h"

#ifdef WINDOWS
#include <mswsock.h>

#include "runtime_linking.h"

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "mswsock")
#else
#include <fcntl.h>
#include <netdb.h>
#define closesocket close
#endif

namespace Mordor {

#ifdef WINDOWS

static LPFN_ACCEPTEX pAcceptEx;
static LPFN_GETACCEPTEXSOCKADDRS pGetAcceptExSockaddrs;
static LPFN_CONNECTEX ConnectEx;

namespace {

static struct Initializer {
    Initializer()
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

        GUID getAcceptExSockaddrsGuid = WSAID_GETACCEPTEXSOCKADDRS;
        WSAIoctl(sock,
                 SIO_GET_EXTENSION_FUNCTION_POINTER,
                 &getAcceptExSockaddrsGuid,
                 sizeof(GUID),
                 &pGetAcceptExSockaddrs,
                 sizeof(LPFN_GETACCEPTEXSOCKADDRS),
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
    ~Initializer()
    {
        WSACleanup();
    }
} g_init;

}
#endif

static Logger::ptr g_log = Log::lookup("mordor:socket");

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
        MORDOR_LOG_LEVEL(g_log, m_sock == -1 ? Log::ERROR : Log::DEBUG) << this
            << " socket(" << family << ", " << type << ", " << protocol
            << "): " << m_sock << " (" << lastError() << ")";
        if (m_sock == -1) {
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("socket");
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
    MORDOR_LOG_DEBUG(g_log) << this << " socket(" << family << ", " << type << ", "
        << protocol << "): " << m_sock << " (" << lastError() << ")";
    if (m_sock == -1) {
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("socket");
    }
#ifdef OSX
    unsigned int opt = 1;
    if (setsockopt(m_sock, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt)) == -1) {
        ::closesocket(m_sock);
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("setsockopt");
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
    MORDOR_LOG_DEBUG(g_log) << this << " socket(" << family << ", " << type << ", "
        << protocol << "): " << m_sock << " (" << lastError() << ")";
    if (m_sock == -1)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("socket");
#ifdef WINDOWS
    try {
        m_ioManager->registerFile((HANDLE)m_sock);
        m_skipCompletionPortOnSuccess =
            !!pSetFileCompletionNotificationModes((HANDLE)m_sock,
                FILE_SKIP_COMPLETION_PORT_ON_SUCCESS |
                FILE_SKIP_SET_EVENT_ON_HANDLE);
    } catch(...) {
        closesocket(m_sock);
        throw;
    }
#else
    if (fcntl(m_sock, F_SETFL, O_NONBLOCK) == -1) {
        ::closesocket(m_sock);
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("fcntl");
    }
#endif
#ifdef OSX
    unsigned int opt = 1;
    if (setsockopt(m_sock, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt)) == -1) {
        ::closesocket(m_sock);
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("setsockopt");
    }
#endif
}

Socket::~Socket()
{
    if (m_sock != -1) {
        int rc = ::closesocket(m_sock);
        MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::INFO) << this
            << " close(" << m_sock << "): (" << lastError() << ")";
    }
#ifdef WINDOWS
    if (m_ioManager && m_hEvent)
        CloseHandle(m_hEvent);
#endif
}

void
Socket::bind(const Address &addr)
{
    MORDOR_ASSERT(addr.family() == m_family);
    if (::bind(m_sock, addr.name(), addr.nameLen())) {
        error_t error = lastError();
        MORDOR_LOG_ERROR(g_log) << this << " bind(" << m_sock << ", " << addr
            << "): (" << error << ")";
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "bind");
    }
    MORDOR_LOG_DEBUG(g_log) << this << " bind(" << m_sock << ", " << addr << ")";
}

void
Socket::bind(Address::ptr addr)
{
    MORDOR_ASSERT(addr->family() == m_family);
    if (::bind(m_sock, addr->name(), addr->nameLen())) {
        error_t error = lastError();
        MORDOR_LOG_ERROR(g_log) << this << " bind(" << m_sock << ", " << *addr
            << "): (" << error << ")";
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "bind");
    }
    MORDOR_LOG_DEBUG(g_log) << this << " bind(" << m_sock << ", " << *addr << ")";
    m_localAddress = addr;
}

void
Socket::connect(const Address &to)
{
    MORDOR_ASSERT(to.family() == m_family);
    if (!m_ioManager) {
        if (::connect(m_sock, to.name(), to.nameLen())) {
            MORDOR_LOG_ERROR(g_log) << this << " connect(" << m_sock << ", " << to
                << "): (" << lastError() << ")";
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("connect");
        }
        MORDOR_LOG_INFO(g_log) << this << " connect(" << m_sock << ", " << to << ")";
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
                            MORDOR_LOG_ERROR(g_log) << this << " bind(" << m_sock
                                << ", 0.0.0.0:0): (" << lastError() << ")";
                            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("bind");
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
                            MORDOR_LOG_ERROR(g_log) << this << " bind(" << m_sock
                                << ", [::]:0): (" << lastError() << ")";
                            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("bind");
                        }
                        break;
                    }
                default:
                    MORDOR_NOTREACHED();
            }

            m_ioManager->registerEvent(&m_sendEvent);
            BOOL bRet = ConnectEx(m_sock, to.name(), to.nameLen(), NULL, 0, NULL, &m_sendEvent.overlapped);
            if (!bRet && GetLastError() != WSA_IO_PENDING) {
                MORDOR_LOG_ERROR(g_log) << this << " connect(" << m_sock
                    << ", " << to << "): (" << lastError() << ")";
                m_ioManager->unregisterEvent(&m_sendEvent);
                MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("ConnectEx");
            }
            if (m_skipCompletionPortOnSuccess && bRet) {
                m_ioManager->unregisterEvent(&m_sendEvent);
                m_sendEvent.overlapped.Internal = STATUS_SUCCESS;
            } else {
                if (m_cancelledSend) {
                    MORDOR_LOG_ERROR(g_log) << this << " connect(" << m_sock << ", " << to
                            << "): (" << m_cancelledSend << ")";
                    m_ioManager->cancelEvent((HANDLE)m_sock, &m_sendEvent);
                    Scheduler::getThis()->yieldTo();
                    MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledSend, "ConnectEx");
                }
                Timer::ptr timeout;
                if (m_sendTimeout != ~0ull)
                    timeout = m_ioManager->registerTimer(m_sendTimeout, boost::bind(
                        &IOManagerIOCP::cancelEvent, m_ioManager, (HANDLE)m_sock, &m_sendEvent));
                Scheduler::getThis()->yieldTo();
                if (timeout)
                    timeout->cancel();
            }
            DWORD error = pRtlNtStatusToDosError((NTSTATUS)m_sendEvent.overlapped.Internal);
            if (error == ERROR_OPERATION_ABORTED &&
                m_cancelledSend != ERROR_OPERATION_ABORTED)
                error = WSAETIMEDOUT;
            // WTF, Windows!?
            if (error == ERROR_SEM_TIMEOUT)
                error = WSAETIMEDOUT;
            MORDOR_LOG_LEVEL(g_log, error ? Log::ERROR : Log::INFO) << this
                << " connect(" << m_sock << ", " << to << "): (" << error << ")";
            if (error)
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "ConnectEx");
            setOption(SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
        } else {
            if (!m_hEvent) {
                m_hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
                if (!m_hEvent)
                    MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateEventW");
            }
            if (WSAEventSelect(m_sock, m_hEvent, FD_CONNECT))
                MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WSAEventSelect");
            if (!::connect(m_sock, to.name(), to.nameLen())) {
                MORDOR_LOG_INFO(g_log) << this << " connect(" << m_sock << ", "
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
                    MORDOR_LOG_ERROR(g_log) << this << " connect(" << m_sock << ", " << to
                            << "): (" << m_cancelledSend << ")";
                    if (!m_ioManager->unregisterEvent(m_hEvent))
                        Scheduler::getThis()->yieldTo();
                    MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledSend, "connect");
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
                    MORDOR_LOG_ERROR(g_log) << this << " connect(" << m_sock
                        << ", " << to << "): (" << m_cancelledSend << ")";
                    MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledSend, "connect");
                }
                ::connect(m_sock, to.name(), to.nameLen());
                DWORD lastError = GetLastError();
                if (lastError == WSAEISCONN)
                    lastError = ERROR_SUCCESS;
                MORDOR_LOG_LEVEL(g_log, lastError ? Log::ERROR : Log::INFO)
                    << this << " connect(" << m_sock << ", " << to
                    << "): (" << lastError << ")";
                if (lastError)
                    MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("connect");
            } else {
                MORDOR_LOG_ERROR(g_log) << this << " connect(" << m_sock << ", "
                    << to << "): (" << lastError() << ")";
                MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("connect");
            }
        }
#else
        if (!::connect(m_sock, to.name(), to.nameLen())) {
            MORDOR_LOG_INFO(g_log) << this << " connect(" << m_sock << ", " << to
                << ")";
            // Worked first time
            return;
        }
        if (errno == EINPROGRESS) {
            m_ioManager->registerEvent(m_sock, IOManager::WRITE);
            if (m_cancelledSend) {
                MORDOR_LOG_ERROR(g_log) << this << " connect(" << m_sock << ", " << to
                    << "): (" << m_cancelledSend << ")";
                m_ioManager->cancelEvent(m_sock, IOManager::WRITE);
                Scheduler::getThis()->yieldTo();
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledSend, "connect");
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
                MORDOR_LOG_ERROR(g_log) << this << " connect(" << m_sock << ", " << to
                    << "): (" << m_cancelledSend << ")";
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledSend, "connect");
            }
            int err;
            size_t size = sizeof(int);
            getOption(SOL_SOCKET, SO_ERROR, &err, &size);
            if (err != 0) {
                MORDOR_LOG_ERROR(g_log) << this << " connect(" << m_sock << ", " << to
                    << "): (" << err << ")";
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(err, "connect");
            }
            MORDOR_LOG_INFO(g_log) << this << " connect(" << m_sock << ", " << to
                << ")";
        } else {
            MORDOR_LOG_ERROR(g_log) << this << " connect(" << m_sock << ", " << to
                << "): (" << lastError() << ")";
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("connect");
        }
#endif
    }
}

void
Socket::listen(int backlog)
{
    int rc = ::listen(m_sock, backlog);
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::DEBUG) << this << " listen("
        << m_sock << ", " << backlog << "): " << rc << " (" << lastError()
        << ")";
    if (rc) {
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("listen");
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
        MORDOR_ASSERT(target.m_sock != -1);
    } else {
        MORDOR_ASSERT(target.m_sock == -1);
    }
#else
    MORDOR_ASSERT(target.m_sock == -1);
#endif
    MORDOR_ASSERT(target.m_family == m_family);
    MORDOR_ASSERT(target.m_protocol == m_protocol);
    if (!m_ioManager) {
        socket_t newsock = ::accept(m_sock, NULL, NULL);
        MORDOR_LOG_LEVEL(g_log, newsock == -1 ? Log::ERROR : Log::INFO)
            << this << " accept(" << m_sock << "): " << newsock << " ("
            << lastError() << ")";
        if (newsock == -1) {
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("accept");
        }
        target.m_sock = newsock;
    } else {
#ifdef WINDOWS
        if (pAcceptEx) {
            m_ioManager->registerEvent(&m_receiveEvent);
            unsigned char addrs[sizeof(SOCKADDR_STORAGE) * 2 + 16];
            DWORD bytes;
            BOOL ret = pAcceptEx(m_sock, target.m_sock, addrs, 0, sizeof(SOCKADDR_STORAGE) + 16, sizeof(SOCKADDR_STORAGE) + 16, &bytes,
                &m_receiveEvent.overlapped);
            if (!ret && GetLastError() != WSA_IO_PENDING) {
                MORDOR_LOG_ERROR(g_log) << this << " accept(" << m_sock << "):  ("
                    << lastError() << ")";
                m_ioManager->unregisterEvent(&m_receiveEvent);
                MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("AcceptEx");
            }
            if (m_skipCompletionPortOnSuccess && ret) {
                m_ioManager->unregisterEvent(&m_receiveEvent);
                m_receiveEvent.overlapped.Internal = STATUS_SUCCESS;
            } else {
                if (m_cancelledReceive) {
                    MORDOR_LOG_ERROR(g_log) << this << " accept(" << m_sock << "): ("
                        << m_cancelledReceive << ")";
                    m_ioManager->cancelEvent((HANDLE)m_sock, &m_receiveEvent);
                    Scheduler::getThis()->yieldTo();
                    MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledReceive, "AcceptEx");
                }
                Timer::ptr timeout;
                if (m_receiveTimeout != ~0ull)
                    timeout = m_ioManager->registerTimer(m_receiveTimeout, boost::bind(
                        &IOManagerIOCP::cancelEvent, m_ioManager, (HANDLE)m_sock, &m_receiveEvent));
                Scheduler::getThis()->yieldTo();
                if (timeout)
                    timeout->cancel();
            }
            DWORD error = pRtlNtStatusToDosError((NTSTATUS)m_receiveEvent.overlapped.Internal);
            if (error && error != ERROR_MORE_DATA) {
                if (error == ERROR_OPERATION_ABORTED &&
                    m_cancelledReceive != ERROR_OPERATION_ABORTED)
                    error = WSAETIMEDOUT;
                MORDOR_LOG_ERROR(g_log) << this << " accept(" << m_sock << "): ("
                    << error << ")";
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "AcceptEx");
            }
            sockaddr *localAddr = NULL, *remoteAddr = NULL;
            INT localAddrLen, remoteAddrLen;
            if (pGetAcceptExSockaddrs)
                pGetAcceptExSockaddrs(addrs, 0, sizeof(SOCKADDR_STORAGE) + 16,
                    sizeof(SOCKADDR_STORAGE) + 16, &localAddr, &localAddrLen,
                    &remoteAddr, &remoteAddrLen);
            if (remoteAddr)
                m_remoteAddress = Address::create(remoteAddr, remoteAddrLen, m_family, m_protocol);

            std::ostringstream os;
            if (remoteAddr)
                os << " (" << *m_remoteAddress << ")";
            MORDOR_LOG_INFO(g_log) << this << " accept(" << m_sock << "): "
                << target.m_sock << os.str();
            target.setOption(SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, &m_sock, sizeof(m_sock));
            target.m_ioManager->registerFile((HANDLE)target.m_sock);
            target.m_skipCompletionPortOnSuccess =
                !!pSetFileCompletionNotificationModes((HANDLE)target.m_sock,
                    FILE_SKIP_COMPLETION_PORT_ON_SUCCESS |
                    FILE_SKIP_SET_EVENT_ON_HANDLE);
        } else {
            if (!m_hEvent) {
                m_hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
                if (!m_hEvent)
                    MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateEventW");
            }
            if (WSAEventSelect(m_sock, m_hEvent, FD_ACCEPT))
                MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WSAEventSelect");
            socket_t newsock = ::accept(m_sock, NULL, NULL);
            if (newsock != -1) {
                MORDOR_LOG_INFO(g_log) << this << " accept(" << m_sock << "): "
                    << newsock;
                // Worked first time
                return;
            }
            if (GetLastError() == WSAEWOULDBLOCK) {
                m_ioManager->registerEvent(m_hEvent);
                m_fiber = Fiber::getThis();
                m_scheduler = Scheduler::getThis();
                if (m_cancelledReceive) {
                    MORDOR_LOG_ERROR(g_log) << this << " accept(" << m_sock << "): ("
                        << m_cancelledReceive << ")";
                    if (!m_ioManager->unregisterEvent(m_hEvent))
                        Scheduler::getThis()->yieldTo();
                    MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledReceive, "accept");
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
                    MORDOR_LOG_ERROR(g_log) << this << " accept(" << m_sock
                        << "): (" << m_cancelledReceive << ")";
                    MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledReceive, "accept");
                }
                newsock = ::accept(m_sock, NULL, NULL);
                MORDOR_LOG_LEVEL(g_log, GetLastError() ? Log::ERROR : Log::INFO)
                    << this << " accept(" << m_sock << "): " << newsock << " ("
                    << lastError() << ")";
                if (newsock == -1)
                    MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("accept");
            } else {
                MORDOR_LOG_ERROR(g_log) << this << " accept(" << m_sock << "): ("
                    << lastError() << ")";
                MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("accept");
            }
        }
#else
        int newsock = ::accept(m_sock, NULL, NULL);
        while (newsock == -1 && errno == EAGAIN) {
            m_ioManager->registerEvent(m_sock, IOManager::READ);
            if (m_cancelledReceive) {
                MORDOR_LOG_ERROR(g_log) << this << " accept(" << m_sock << "): ("
                    << m_cancelledReceive << ")";
                m_ioManager->cancelEvent(m_sock, IOManager::READ);
                Scheduler::getThis()->yieldTo();
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledReceive, "accept");
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
                MORDOR_LOG_ERROR(g_log) << this << " accept(" << m_sock
                    << "): (" << m_cancelledReceive << ")";
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledReceive, "accept");
            }
            newsock = ::accept(m_sock, NULL, NULL);
        }
        MORDOR_LOG_LEVEL(g_log, newsock == -1 ? Log::ERROR : Log::INFO)
            << this << " accept(" << m_sock << "): " << newsock
            << " (" << lastError() << ")";
        if (newsock == -1) {
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("accept");
        }
        if (fcntl(newsock, F_SETFL, O_NONBLOCK) == -1) {
            ::close(newsock);
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("fcntl");
        }
        target.m_sock = newsock;
#endif
    }
}

void
Socket::shutdown(int how)
{
    if(::shutdown(m_sock, how)) {
        MORDOR_LOG_ERROR(g_log) << this << " shutdown(" << m_sock << ", "
            << how << "): (" << lastError() << ")";
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("shutdown");
    }
    MORDOR_LOG_VERBOSE(g_log) << this << " shutdown(" << m_sock << ", "
        << how << ")";
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
        m_ioManager->registerEvent(&m_sendEvent);
        DWORD sent;
        int ret = WSASend(m_sock, &wsabuf, 1, &sent, flags,
            &m_sendEvent.overlapped, NULL);
        if (ret && GetLastError() != WSA_IO_PENDING) {
            MORDOR_LOG_ERROR(g_log) << this << " send(" << m_sock << ", " << len
                << "): (" << lastError() << ")";
            m_ioManager->unregisterEvent(&m_sendEvent);
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WSASend");
        }
        if (m_skipCompletionPortOnSuccess && ret == 0) {
            m_ioManager->unregisterEvent(&m_sendEvent);
        } else {
            if (m_cancelledSend) {
                MORDOR_LOG_ERROR(g_log) << this << " send(" << m_sock << ", " << len
                    << "): (" << m_cancelledSend << ")";
                m_ioManager->cancelEvent((HANDLE)m_sock, &m_sendEvent);
                Scheduler::getThis()->yieldTo();
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledSend, "WSASend");
            }
            Timer::ptr timeout;
            if (m_sendTimeout != ~0ull)
                timeout = m_ioManager->registerTimer(m_sendTimeout, boost::bind(
                    &IOManagerIOCP::cancelEvent, m_ioManager, (HANDLE)m_sock, &m_sendEvent));
            Scheduler::getThis()->yieldTo();
            if (timeout)
                timeout->cancel();
        }
        DWORD error = pRtlNtStatusToDosError((NTSTATUS)m_sendEvent.overlapped.Internal);
        if (error == ERROR_OPERATION_ABORTED &&
            m_cancelledSend != ERROR_OPERATION_ABORTED)
            error = WSAETIMEDOUT;
        if (error == ERROR_SEM_TIMEOUT)
            error = WSAETIMEDOUT;
        MORDOR_LOG_LEVEL(g_log, error ? Log::ERROR : Log::DEBUG) << this
            << " send(" << m_sock << ", " << len << "): "
            << m_sendEvent.overlapped.InternalHigh << " (" << error << ")";
        if (error)
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "WSASend");
        return m_sendEvent.overlapped.InternalHigh;
    } else
#endif
    {
        if (len > 0x7fffffff)
            len = 0x7fffffff;
        int rc = ::send(m_sock, (const char*)buf, (socklen_t)len, flags);
#ifndef WINDOWS
        while (m_ioManager && rc == -1 && errno == EAGAIN) {
            m_ioManager->registerEvent(m_sock, IOManager::WRITE);
            if (m_cancelledSend) {
                MORDOR_LOG_ERROR(g_log) << this << " send(" << m_sock << ", " << len
                    << "): (" << m_cancelledSend << ")";
                m_ioManager->cancelEvent(m_sock, IOManager::WRITE);
                Scheduler::getThis()->yieldTo();
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledSend, "send");
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
                MORDOR_LOG_ERROR(g_log) << this << " send(" << m_sock << ", "
                    << len << "): (" << m_cancelledSend << ")";
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledSend, "send");
            }
            rc = ::send(m_sock, buf, len, flags);
        }
#endif
        MORDOR_LOG_LEVEL(g_log, rc == -1 ? Log::ERROR : Log::DEBUG) << this
            << " send(" << m_sock << ", " << len << "): " << rc << " ("
            << lastError() << ")";
        if (rc == -1) {
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("send");
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
        m_ioManager->registerEvent(&m_sendEvent);
        MORDOR_ASSERT(len <= 0xffffffff);
        DWORD sent;
        int ret = WSASend(m_sock, (LPWSABUF)bufs, (DWORD)len, &sent, flags,
            &m_sendEvent.overlapped, NULL);
        if (ret && GetLastError() != WSA_IO_PENDING) {
            MORDOR_LOG_ERROR(g_log) << this << " sendv(" << m_sock << ", " << len
                << "): (" << lastError() << ")";
            m_ioManager->unregisterEvent(&m_sendEvent);
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WSASend");
        }
        if (m_skipCompletionPortOnSuccess && ret == 0) {
            m_ioManager->unregisterEvent(&m_sendEvent);
        } else {
            if (m_cancelledSend) {
                MORDOR_LOG_ERROR(g_log) << this << " sendv(" << m_sock << ", " << len
                    << "): (" << m_cancelledSend << ")";
                m_ioManager->cancelEvent((HANDLE)m_sock, &m_sendEvent);
                Scheduler::getThis()->yieldTo();
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledSend, "WSASend");
            }
            Timer::ptr timeout;
            if (m_sendTimeout != ~0ull)
                timeout = m_ioManager->registerTimer(m_sendTimeout, boost::bind(
                    &IOManagerIOCP::cancelEvent, m_ioManager, (HANDLE)m_sock, &m_sendEvent));
            Scheduler::getThis()->yieldTo();
            if (timeout)
                timeout->cancel();
        }
        DWORD error = pRtlNtStatusToDosError((NTSTATUS)m_sendEvent.overlapped.Internal);
        if (error == ERROR_OPERATION_ABORTED &&
            m_cancelledSend != ERROR_OPERATION_ABORTED)
            error = WSAETIMEDOUT;
        if (error == ERROR_SEM_TIMEOUT)
            error = WSAETIMEDOUT;
        MORDOR_LOG_LEVEL(g_log, error ? Log::ERROR : Log::DEBUG) << this
            << " sendv(" << m_sock << ", " << len << "): "
            << m_sendEvent.overlapped.InternalHigh << " (" << error << ")";
        if (error)
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "WSASend");
        return m_sendEvent.overlapped.InternalHigh;
    } else {
        DWORD sent;
        MORDOR_ASSERT(len <= 0xffffffff);
        if (WSASend(m_sock, (LPWSABUF)bufs, (DWORD)len, &sent, flags,
            NULL, NULL)) {
            MORDOR_LOG_ERROR(g_log) << this << " sendv(" << m_sock << ", "
                << len << "): (" << lastError() << ")";
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WSASend");
        }
        MORDOR_LOG_DEBUG(g_log) << this << " sendv(" << m_sock << ", "
            << len << "): " << sent;
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
        if (m_cancelledSend) {
            MORDOR_LOG_ERROR(g_log) << this << " send(" << m_sock << ", " << len
                << "): (" << m_cancelledSend << ")";
            m_ioManager->cancelEvent(m_sock, IOManager::WRITE);
            Scheduler::getThis()->yieldTo();
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledSend, "sendmsg");
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
            MORDOR_LOG_ERROR(g_log) << this << " sendv(" << m_sock << ", "
                    << len << "): (" << m_cancelledSend << ")";
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledSend, "sendmsg");
        }
        rc = ::sendmsg(m_sock, &msg, flags);
    }
    MORDOR_LOG_LEVEL(g_log, rc == -1 ? Log::ERROR : Log::DEBUG) << this
            << " sendv(" << m_sock << ", " << len << "): " << rc << " ("
            << lastError() << ")";
    if (rc == -1) {
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("sendmsg");
    }
    return rc;
#endif
}

size_t
Socket::sendTo(const void *buf, size_t len, int flags, const Address &to)
{
    MORDOR_ASSERT(to.family() == family());
#ifdef WINDOWS
    if (m_ioManager) {
        if (len > 0xfffffff)
            len = 0xffffffff;
        WSABUF wsabuf;
        wsabuf.buf = (char*)buf;
        wsabuf.len = (unsigned int)len;
        m_ioManager->registerEvent(&m_sendEvent);
        DWORD sent;
        int ret = WSASendTo(m_sock, &wsabuf, 1, &sent, flags,
            to.name(), to.nameLen(),
            &m_sendEvent.overlapped, NULL);
        if (ret && GetLastError() != WSA_IO_PENDING) {
            MORDOR_LOG_ERROR(g_log) << this << " sendto(" << m_sock << ", " << len
                << ", " << to << "): (" << lastError() << ")";
            m_ioManager->unregisterEvent(&m_sendEvent);
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WSASendTo");
        }
        if (m_skipCompletionPortOnSuccess && ret == 0) {
            m_ioManager->unregisterEvent(&m_sendEvent);
        } else {
            if (m_cancelledSend) {
                MORDOR_LOG_ERROR(g_log) << this << " sendto(" << m_sock << ", " << len
                    << ", " << to << "): (" << m_cancelledSend << ")";
                m_ioManager->cancelEvent((HANDLE)m_sock, &m_sendEvent);
                Scheduler::getThis()->yieldTo();
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledSend, "WSASendTo");
            }
            Timer::ptr timeout;
            if (m_sendTimeout != ~0ull)
                timeout = m_ioManager->registerTimer(m_sendTimeout, boost::bind(
                    &IOManagerIOCP::cancelEvent, m_ioManager, (HANDLE)m_sock, &m_sendEvent));
            Scheduler::getThis()->yieldTo();
            if (timeout)
                timeout->cancel();
        }
        DWORD error = pRtlNtStatusToDosError((NTSTATUS)m_sendEvent.overlapped.Internal);
        if (error == ERROR_OPERATION_ABORTED &&
            m_cancelledSend != ERROR_OPERATION_ABORTED)
            error = WSAETIMEDOUT;
        if (error == ERROR_SEM_TIMEOUT)
            error = WSAETIMEDOUT;
        MORDOR_LOG_LEVEL(g_log, error ? Log::ERROR : Log::DEBUG) << this
            << " sendto(" << m_sock << ", " << len << ", " << to << "): "
            << m_sendEvent.overlapped.InternalHigh << " (" << error << ")";
        if (error)
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "WSASendTo");
        return m_sendEvent.overlapped.InternalHigh;
    } else
#endif
    {
        int rc = ::sendto(m_sock, (const char*)buf, (socklen_t)len, flags, to.name(), to.nameLen());
#ifndef WINDOWS
        while (m_ioManager && rc == -1 && errno == EAGAIN) {
            m_ioManager->registerEvent(m_sock, IOManager::WRITE);
            if (m_cancelledSend) {
                MORDOR_LOG_ERROR(g_log) << this << " sendto(" << m_sock << ", " << len
                    << ", " << to << "): (" << m_cancelledSend << ")";
                m_ioManager->cancelEvent(m_sock, IOManager::WRITE);
                Scheduler::getThis()->yieldTo();
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledSend, "sendto");
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
                MORDOR_LOG_ERROR(g_log) << this << " sendto(" << m_sock << ", "
                    << len << ", " << to << "): (" << m_cancelledSend << ")";
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledSend, "sendto");
            }
            rc = ::sendto(m_sock, buf, len, flags, to.name(), to.nameLen());
        }
#endif
        MORDOR_LOG_LEVEL(g_log, rc == -1 ? Log::ERROR : Log::DEBUG) << this
            << " sendto(" << m_sock << ", " << len << ", " << to << "): "
            << rc << " (" << lastError() << ")";
        if (rc == -1) {
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("sendto");
        }
        return rc;
    }
}

size_t
Socket::sendTo(const iovec *bufs, size_t len, int flags, const Address &to)
{
    MORDOR_ASSERT(to.family() == family());
#ifdef WINDOWS
    if (m_ioManager) {
        m_ioManager->registerEvent(&m_sendEvent);
        MORDOR_ASSERT(len <= 0xffffffff);
        DWORD sent;
        int ret = WSASendTo(m_sock, (LPWSABUF)bufs, (DWORD)len, &sent, flags,
            to.name(), to.nameLen(),
            &m_sendEvent.overlapped, NULL);
        if (ret && GetLastError() != WSA_IO_PENDING) {
            MORDOR_LOG_ERROR(g_log) << this << " sendtov(" << m_sock << ", " << len
                << ", " << to << "): (" << lastError() << ")";
            m_ioManager->unregisterEvent(&m_sendEvent);
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WSASendTo");
        }
        if (m_skipCompletionPortOnSuccess && ret == 0) {
            m_ioManager->unregisterEvent(&m_sendEvent);
            m_sendEvent.overlapped.Internal = STATUS_SUCCESS;
            m_sendEvent.overlapped.InternalHigh = sent;
        } else {
            if (m_cancelledSend) {
                MORDOR_LOG_ERROR(g_log) << this << " sendtov(" << m_sock << ", " << len
                    << ", " << to << "): (" << m_cancelledSend << ")";
                m_ioManager->cancelEvent((HANDLE)m_sock, &m_sendEvent);
                Scheduler::getThis()->yieldTo();
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledSend, "WSASendTo");
            }
            Timer::ptr timeout;
            if (m_sendTimeout != ~0ull)
                timeout = m_ioManager->registerTimer(m_sendTimeout, boost::bind(
                    &IOManagerIOCP::cancelEvent, m_ioManager, (HANDLE)m_sock, &m_sendEvent));
            Scheduler::getThis()->yieldTo();
            if (timeout)
                timeout->cancel();
        }
        DWORD error = pRtlNtStatusToDosError((NTSTATUS)m_sendEvent.overlapped.Internal);
        if (error == ERROR_OPERATION_ABORTED &&
            m_cancelledSend != ERROR_OPERATION_ABORTED)
            error = WSAETIMEDOUT;
        if (error == ERROR_SEM_TIMEOUT)
            error = WSAETIMEDOUT;
        MORDOR_LOG_LEVEL(g_log, error ? Log::ERROR : Log::DEBUG) << this
            << " sendtov(" << m_sock << ", " << len << ", " << to << "): "
            << m_sendEvent.overlapped.InternalHigh << " (" << error << ")";
        if (error)
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "WSASendTo");
        return m_sendEvent.overlapped.InternalHigh;
    } else {
        DWORD sent;
        MORDOR_ASSERT(len <= 0xffffffff);
        if (WSASendTo(m_sock, (LPWSABUF)bufs, (DWORD)len, &sent, flags,
            to.name(), to.nameLen(),
            NULL, NULL)) {
            MORDOR_LOG_ERROR(g_log) << this << " sendtov(" << m_sock << ", "
                << len << ", " << to << "): (" << lastError() << ")";
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WSASendTo");
        }
        MORDOR_LOG_DEBUG(g_log) << this << " sendtov(" << m_sock << ", "
            << len << ", " << to << "): " << sent;
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
        if (m_cancelledSend) {
            MORDOR_LOG_ERROR(g_log) << this << " sendtov(" << m_sock << ", " << len
                << ", " << to << "): (" << m_cancelledSend << ")";
            m_ioManager->cancelEvent(m_sock, IOManager::WRITE);
            Scheduler::getThis()->yieldTo();
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledSend, "sendmsg");
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
            MORDOR_LOG_ERROR(g_log) << this << " sendtov(" << m_sock << ", "
                    << len << ", " << to << "): (" << m_cancelledSend << ")";
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledSend, "sendmsg");
        }
        rc = ::sendmsg(m_sock, &msg, flags);
    }
    MORDOR_LOG_LEVEL(g_log, rc == -1 ? Log::ERROR : Log::DEBUG) << this
            << " sendtov(" << m_sock << ", " << len << ", " << to << "): "
            << rc << " (" << lastError() << ")";
    if (rc == -1) {
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("sendmsg");
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
        m_ioManager->registerEvent(&m_receiveEvent);
        DWORD recvd;
        int ret = WSARecv(m_sock, &wsabuf, 1, &recvd, (LPDWORD)&flags,
            &m_receiveEvent.overlapped, NULL);
        if (ret && GetLastError() != WSA_IO_PENDING) {
            MORDOR_LOG_ERROR(g_log) << this << " recv(" << m_sock << ", "
                << len << "): (" << lastError() << ")";
            m_ioManager->unregisterEvent(&m_receiveEvent);
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WSARecv");
        }
        if (m_skipCompletionPortOnSuccess && ret == 0) {
            m_ioManager->unregisterEvent(&m_receiveEvent);
        } else {
            if (m_cancelledReceive) {
                MORDOR_LOG_ERROR(g_log) << this << " recv(" << m_sock << ", " << len
                    << "): (" << m_cancelledReceive << ")";
                m_ioManager->cancelEvent((HANDLE)m_sock, &m_receiveEvent);
                Scheduler::getThis()->yieldTo();
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledReceive, "WSARecv");
            }
            Timer::ptr timeout;
            if (m_receiveTimeout != ~0ull)
                timeout = m_ioManager->registerTimer(m_receiveTimeout, boost::bind(
                    &IOManagerIOCP::cancelEvent, m_ioManager, (HANDLE)m_sock, &m_receiveEvent));
            Scheduler::getThis()->yieldTo();
            if (timeout)
                timeout->cancel();
        }
        DWORD error = pRtlNtStatusToDosError((NTSTATUS)m_receiveEvent.overlapped.Internal);
        if (error == ERROR_OPERATION_ABORTED &&
            m_cancelledReceive != ERROR_OPERATION_ABORTED)
            error = WSAETIMEDOUT;
        if (error == ERROR_SEM_TIMEOUT)
            error = WSAETIMEDOUT;
        MORDOR_LOG_LEVEL(g_log, error ? Log::ERROR : Log::DEBUG) << this
            << " recv(" << m_sock << ", " << len << "): "
            << m_receiveEvent.overlapped.InternalHigh << " (" << error << ")";
        if (error)
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "WSARecv");
        return m_receiveEvent.overlapped.InternalHigh;
    } else
#endif
    {
        int rc = ::recv(m_sock, (char*)buf, (socklen_t)len, flags);
#ifndef WINDOWS
        while (m_ioManager && rc == -1 && errno == EAGAIN) {
            m_ioManager->registerEvent(m_sock, IOManager::READ);
            if (m_cancelledReceive) {
                MORDOR_LOG_ERROR(g_log) << this << " recv(" << m_sock << ", " << len
                    << "): (" << m_cancelledReceive << ")";
                m_ioManager->cancelEvent(m_sock, IOManager::READ);
                Scheduler::getThis()->yieldTo();
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledReceive, "recv");
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
                MORDOR_LOG_ERROR(g_log) << this << " recv(" << m_sock << ", "
                    << len << "): (" << m_cancelledReceive << ")";
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledReceive, "recv");
            }
            rc = ::recv(m_sock, buf, len, flags);
        }
#endif
        MORDOR_LOG_LEVEL(g_log, rc == -1 ? Log::ERROR : Log::DEBUG) << this
            << " recv(" << m_sock << ", " << len << "): " << rc << " ("
            << lastError() << ")";
        if (rc == -1) {
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("recv");
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
        MORDOR_ASSERT(len <= 0xffffffff);
        DWORD recvd;
        int ret = WSARecv(m_sock, (LPWSABUF)bufs, (DWORD)len, &recvd, (LPDWORD)&flags,
            &m_receiveEvent.overlapped, NULL);
        if (ret && GetLastError() != WSA_IO_PENDING) {
            MORDOR_LOG_ERROR(g_log) << this << " recvv(" << m_sock << ", "
                << len << "): (" << lastError() << ")";
            m_ioManager->unregisterEvent(&m_receiveEvent);
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WSARecv");
        }
        if (m_skipCompletionPortOnSuccess && ret == 0) {
            m_ioManager->unregisterEvent(&m_receiveEvent);
            m_receiveEvent.overlapped.Internal = STATUS_SUCCESS;
            m_receiveEvent.overlapped.InternalHigh = recvd;
        } else {
            if (m_cancelledReceive) {
                MORDOR_LOG_ERROR(g_log) << this << " recvv(" << m_sock << ", " << len
                    << "): (" << m_cancelledReceive << ")";
                m_ioManager->cancelEvent((HANDLE)m_sock, &m_receiveEvent);
                Scheduler::getThis()->yieldTo();
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledReceive, "WSARecv");
            }
            Timer::ptr timeout;
            if (m_receiveTimeout != ~0ull)
                timeout = m_ioManager->registerTimer(m_receiveTimeout, boost::bind(
                    &IOManagerIOCP::cancelEvent, m_ioManager, (HANDLE)m_sock, &m_receiveEvent));
            Scheduler::getThis()->yieldTo();
            if (timeout)
                timeout->cancel();
        }
        DWORD error = pRtlNtStatusToDosError((NTSTATUS)m_receiveEvent.overlapped.Internal);
        if (error == ERROR_OPERATION_ABORTED &&
            m_cancelledReceive != ERROR_OPERATION_ABORTED)
            error = WSAETIMEDOUT;
        if (error == ERROR_SEM_TIMEOUT)
            error = WSAETIMEDOUT;
        MORDOR_LOG_LEVEL(g_log, error ? Log::ERROR : Log::DEBUG) << this
            << " recvv(" << m_sock << ", " << len << "): "
            << m_receiveEvent.overlapped.InternalHigh << " (" << error << ")";
        if (error)
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "WSARecv");
        return m_receiveEvent.overlapped.InternalHigh;
    } else {
        DWORD received;
        MORDOR_ASSERT(len <= 0xffffffff);
        if (WSARecv(m_sock, (LPWSABUF)bufs, (DWORD)len, &received, (LPDWORD)&flags,
            NULL, NULL)) {
            MORDOR_LOG_ERROR(g_log) << this << " recvv(" << m_sock << ", "
                << len << "): (" << lastError() << ")";
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WSARecv");
        }
        MORDOR_LOG_DEBUG(g_log) << this << " recvv(" << m_sock << ", "
            << len << "): " << received;
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
        if (m_cancelledReceive) {
            MORDOR_LOG_ERROR(g_log) << this << " recvv(" << m_sock << ", " << len
                << "): (" << m_cancelledReceive << ")";
            m_ioManager->cancelEvent(m_sock, IOManager::READ);
            Scheduler::getThis()->yieldTo();
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledReceive, "recvmsg");
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
            MORDOR_LOG_ERROR(g_log) << this << " recvv(" << m_sock << ", "
                    << len << "): (" << m_cancelledReceive << ")";
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledReceive, "recvmsg");
        }
        rc = ::recvmsg(m_sock, &msg, flags);
    }
    MORDOR_LOG_LEVEL(g_log, rc == -1 ? Log::ERROR : Log::DEBUG) << this
            << " recvv(" << m_sock << ", " << len << "): " << rc << " ("
            << lastError() << ")";
    if (rc == -1) {
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("recvmsg");
    }
    return rc;
#endif
}

size_t
Socket::receiveFrom(void *buf, size_t len, int *flags, Address &from)
{
    MORDOR_ASSERT(from.family() == family());
#ifdef WINDOWS
    if (len > 0xffffffff)
        len = 0xffffffff;
    WSABUF wsabuf;
    wsabuf.buf = (char*)buf;
    wsabuf.len = (unsigned int)len;
    int namelen = from.nameLen();
    if (m_ioManager) {
        m_ioManager->registerEvent(&m_sendEvent);
        DWORD recvd;
        int ret = WSARecvFrom(m_sock, &wsabuf, 1, &recvd, (LPDWORD)flags,
            from.name(), &namelen,
            &m_receiveEvent.overlapped, NULL);
        if (ret && GetLastError() != WSA_IO_PENDING) {
            MORDOR_LOG_ERROR(g_log) << this << " recvfrom(" << m_sock << ", "
                << len << "): (" << lastError() << ")";
            m_ioManager->unregisterEvent(&m_receiveEvent);
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WSARecvFrom");
        }
        if (m_skipCompletionPortOnSuccess && ret == 0) {
            m_ioManager->unregisterEvent(&m_receiveEvent);
        } else {
            if (m_cancelledReceive) {
                MORDOR_LOG_ERROR(g_log) << this << " recvfrom(" << m_sock << ", " << len
                    << "): (" << m_cancelledReceive << ")";
                m_ioManager->cancelEvent((HANDLE)m_sock, &m_receiveEvent);
                Scheduler::getThis()->yieldTo();
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledReceive, "WSARecvFrom");
            }
            Timer::ptr timeout;
            if (m_receiveTimeout != ~0ull)
                timeout = m_ioManager->registerTimer(m_receiveTimeout, boost::bind(
                    &IOManagerIOCP::cancelEvent, m_ioManager, (HANDLE)m_sock, &m_receiveEvent));
            Scheduler::getThis()->yieldTo();
            if (timeout)
                timeout->cancel();
        }
        DWORD error = pRtlNtStatusToDosError((NTSTATUS)m_receiveEvent.overlapped.Internal);
        if (error == ERROR_OPERATION_ABORTED &&
            m_cancelledReceive != ERROR_OPERATION_ABORTED)
            error = WSAETIMEDOUT;
        if (error == ERROR_SEM_TIMEOUT)
            error = WSAETIMEDOUT;
        MORDOR_LOG_LEVEL(g_log, error ? Log::ERROR : Log::DEBUG) << this
            << " recvfrom(" << m_sock << ", " << len << "): "
            << m_receiveEvent.overlapped.InternalHigh << " (" << error << ")";
        if (error)
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "WSARecvFrom");
        return m_receiveEvent.overlapped.InternalHigh;
    } else {
        DWORD sent;
        if (WSARecvFrom(m_sock, &wsabuf, 1, &sent, (LPDWORD)flags,
            from.name(), &namelen,
            NULL, NULL)) {
            MORDOR_LOG_ERROR(g_log) << this << " recvfrom(" << m_sock << ", "
                << len << "): (" << lastError() << ")";
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WSARecvFrom");
        }
        MORDOR_LOG_DEBUG(g_log) << this << " recvfrom(" << m_sock << ", "
            << len << "): " << sent << ", " << from;
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
    msg.msg_name = from.name();
    msg.msg_namelen = from.nameLen();
    int rc = ::recvmsg(m_sock, &msg, *flags);
    while (m_ioManager && rc == -1 && errno == EAGAIN) {
        m_ioManager->registerEvent(m_sock, IOManager::READ);
        if (m_cancelledReceive) {
            MORDOR_LOG_ERROR(g_log) << this << " recvfrom(" << m_sock << ", " << len
                << "): (" << m_cancelledReceive << ")";
            m_ioManager->cancelEvent(m_sock, IOManager::READ);
            Scheduler::getThis()->yieldTo();
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledReceive, "recvmsg");
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
            MORDOR_LOG_ERROR(g_log) << this << " recvfrom(" << m_sock << ", "
                << len << "): (" << m_cancelledReceive << ")";
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledReceive, "recvmsg");
        }
        rc = ::recvmsg(m_sock, &msg, *flags);
    }
    MORDOR_LOG_LEVEL(g_log, rc == -1 ? Log::ERROR : Log::DEBUG) << this
        << " recvfrom(" << m_sock << ", " << len << "): "
        << rc << ", " << from << " (" << lastError() << ")";
    if (rc == -1) {
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("recvmsg");
    }
    *flags = msg.msg_flags;
    return rc;
#endif
}

size_t
Socket::receiveFrom(iovec *bufs, size_t len, int *flags, Address &from)
{
    MORDOR_ASSERT(from.family() == family());
#ifdef WINDOWS
    int namelen = from.nameLen();
    if (m_ioManager) {
        m_ioManager->registerEvent(&m_receiveEvent);
        MORDOR_ASSERT(len <= 0xffffffff);
        DWORD recvd;
        int ret = WSARecvFrom(m_sock, (LPWSABUF)bufs, (DWORD)len, &recvd, (LPDWORD)flags,
            from.name(), &namelen,
            &m_receiveEvent.overlapped, NULL);
        if (ret && GetLastError() != WSA_IO_PENDING) {
            MORDOR_LOG_ERROR(g_log) << this << " recvfromv(" << m_sock << ", " << len
                << "): (" << lastError() << ")";
            m_ioManager->unregisterEvent(&m_receiveEvent);
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WSARecvFrom");
        }
        if (m_skipCompletionPortOnSuccess && ret == 0) {
            m_ioManager->unregisterEvent(&m_receiveEvent);
        } else {
            if (m_cancelledReceive) {
                MORDOR_LOG_ERROR(g_log) << this << " recvfromv(" << m_sock << ", " << len
                    << "): (" << m_cancelledReceive << ")";
                m_ioManager->cancelEvent((HANDLE)m_sock, &m_receiveEvent);
                Scheduler::getThis()->yieldTo();
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledReceive, "WSARecvFrom");
            }
            Timer::ptr timeout;
            if (m_receiveTimeout != ~0ull)
                timeout = m_ioManager->registerTimer(m_receiveTimeout, boost::bind(
                    &IOManagerIOCP::cancelEvent, m_ioManager, (HANDLE)m_sock, &m_receiveEvent));
            Scheduler::getThis()->yieldTo();
            if (timeout)
                timeout->cancel();
        }
        DWORD error = pRtlNtStatusToDosError((NTSTATUS)m_receiveEvent.overlapped.Internal);
        if (error == ERROR_OPERATION_ABORTED &&
            m_cancelledReceive != ERROR_OPERATION_ABORTED)
            error = WSAETIMEDOUT;
        if (error == ERROR_SEM_TIMEOUT)
            error = ERROR_SEM_TIMEOUT;
        MORDOR_LOG_LEVEL(g_log, error ? Log::ERROR : Log::DEBUG) << this
            << " recvfromv(" << m_sock << ", " << len << "): "
            << m_receiveEvent.overlapped.InternalHigh << " (" << error << ")";
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "WSARecvFrom");
        return m_receiveEvent.overlapped.InternalHigh;
    } else {
        DWORD sent;
        MORDOR_ASSERT(len <= 0xffffffff);
        if (WSARecvFrom(m_sock, (LPWSABUF)bufs, (DWORD)len, &sent, (LPDWORD)flags,
            from.name(), &namelen,
            NULL, NULL)) {
            MORDOR_LOG_ERROR(g_log) << this << " recvfromv(" << m_sock << ", "
                << len << "): (" << lastError() << ")";
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WSARecvFrom");
        }
        MORDOR_LOG_DEBUG(g_log) << this << " recvfromv(" << m_sock << ", "
            << len << "): " << sent << ", " << from;
        return sent;
    }
#else
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
            MORDOR_LOG_ERROR(g_log) << this << " recvfromv(" << m_sock << ", " << len
                << "): (" << m_cancelledReceive << ")";
            m_ioManager->cancelEvent(m_sock, IOManager::READ);
            Scheduler::getThis()->yieldTo();
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledReceive, "recvmsg");
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
            MORDOR_LOG_ERROR(g_log) << this << " recvfromv(" << m_sock << ", "
                << len << "): (" << m_cancelledReceive << ")";
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledReceive, "recvmsg");
        }
        rc = ::recvmsg(m_sock, &msg, *flags);
    }
    MORDOR_LOG_LEVEL(g_log, rc == -1 ? Log::ERROR : Log::DEBUG) << this
        << " recvfromv(" << m_sock << ", " << len << "): "
        << rc << ", " << from << " (" << lastError() << ")";
    if (rc == -1) {
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("recvmsg");
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
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("getsockopt");
    }
}

void
Socket::setOption(int level, int option, const void *value, size_t len)
{
    if (setsockopt(m_sock, level, option, (const char*)value, (socklen_t)len)) {
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("setsockopt");
    }
}

void
Socket::cancelAccept()
{
    MORDOR_ASSERT(m_ioManager);
#ifdef WINDOWS
    if (m_cancelledReceive)
        return;
    m_cancelledReceive = ERROR_OPERATION_ABORTED;
    if (pAcceptEx) {
        m_ioManager->cancelEvent((HANDLE)m_sock, &m_receiveEvent);
    } else {
        m_unregistered = !!m_ioManager->unregisterEvent(m_hEvent);
        m_scheduler->schedule(m_fiber);
    }
#else
    cancelIo(IOManager::READ, m_cancelledReceive, ECANCELED);
#endif
}

void
Socket::cancelConnect()
{
    MORDOR_ASSERT(m_ioManager);
#ifdef WINDOWS
    if (m_cancelledSend)
        return;
    m_cancelledSend = ERROR_OPERATION_ABORTED;
    if (ConnectEx) {
        m_ioManager->cancelEvent((HANDLE)m_sock, &m_sendEvent);
    } else {
        m_unregistered = !!m_ioManager->unregisterEvent(m_hEvent);
        m_scheduler->schedule(m_fiber);
    }
#else
    cancelIo(IOManager::WRITE, m_cancelledSend, ECANCELED);
#endif
}

void
Socket::cancelSend()
{
    MORDOR_ASSERT(m_ioManager);
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
    MORDOR_ASSERT(m_ioManager);
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
    MORDOR_ASSERT(error);
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
    if (m_remoteAddress)
        return m_remoteAddress;
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
    if (getpeername(m_sock, result->name(), &namelen))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("getpeername");
    MORDOR_ASSERT(namelen <= result->nameLen());
    return m_remoteAddress = result;
}

Address::ptr
Socket::localAddress()
{
    if (m_localAddress)
        return m_localAddress;
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
    if (getsockname(m_sock, result->name(), &namelen))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("getsockname");
    MORDOR_ASSERT(namelen <= result->nameLen());
    return m_localAddress = result;
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
#ifdef WINDOWS
    addrinfoW hints, *results, *next;
#else
    addrinfo hints, *results, *next;
#endif
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
    int error;
#ifdef WINDOWS
    std::wstring serviceWStorage;
    const wchar_t *serviceW = NULL;
    if (service) {
        serviceWStorage = toUtf16(service);
        serviceW = serviceWStorage.c_str();
    }
    error = pGetAddrInfoW(toUtf16(node).c_str(), serviceW, &hints, &results);
#else
    error = getaddrinfo(node.c_str(), service, &hints, &results);
#endif
    switch (error) {
        case 0:
            break;
        case EAI_AGAIN:
            MORDOR_THROW_EXCEPTION(TemporaryNameServerFailureException()
                << errinfo_gaierror(error)
                << boost::errinfo_api_function("getaddrinfo"));
        case EAI_FAIL:
            MORDOR_THROW_EXCEPTION(PermanentNameServerFailureException()
                << errinfo_gaierror(error)
                << boost::errinfo_api_function("getaddrinfo"));
#if defined(WSANO_DATA) || defined(EAI_NODATA)
        case MORDOR_NATIVE(WSANO_DATA, EAI_NODATA):
            MORDOR_THROW_EXCEPTION(NoNameServerDataException()
                << errinfo_gaierror(error)
                << boost::errinfo_api_function("getaddrinfo"));
#endif
        case EAI_NONAME:
            MORDOR_THROW_EXCEPTION(HostNotFoundException()
                << errinfo_gaierror(error)
                << boost::errinfo_api_function("getaddrinfo"));
#ifdef EAI_ADDRFAMILY
        case EAI_ADDRFAMILY:
#endif
        case EAI_BADFLAGS:
        case EAI_FAMILY:
        case EAI_MEMORY:
        case EAI_SERVICE:
        case EAI_SOCKTYPE:
#ifdef EAI_SYSTEM
        case EAI_SYSTEM:
#endif
        default:
            MORDOR_THROW_EXCEPTION(NameLookupException()
                << errinfo_gaierror(error)
                << boost::errinfo_api_function("getaddrinfo"));
    }
    std::vector<Address::ptr> result;
    next = results;
    while (next) {
        result.push_back(create(next->ai_addr, (socklen_t)next->ai_addrlen,
            next->ai_socktype, next->ai_protocol));
        next = next->ai_next;
    }
#ifdef WINDOWS
    pFreeAddrInfoW(results);
#else
    freeaddrinfo(results);
#endif
    return result;
}

Address::ptr
Address::create(const sockaddr *name, socklen_t nameLen, int type, int protocol)
{
    MORDOR_ASSERT(name);
    Address::ptr result;
    switch (name->sa_family) {
        case AF_INET:
            result.reset(new IPv4Address(type, protocol));
            MORDOR_ASSERT(nameLen <= result->nameLen());
            memcpy(result->name(), name, nameLen);
            break;
        case AF_INET6:
            result.reset(new IPv6Address(type, protocol));
            MORDOR_ASSERT(nameLen <= result->nameLen());
            memcpy(result->name(), name, nameLen);
            break;
        default:
            result.reset(new UnknownAddress(name->sa_family, type, protocol));
            MORDOR_ASSERT(nameLen <= result->nameLen());
            memcpy(result->name(), name, nameLen);
            break;
    }
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

#ifndef WINDOWS
UnixAddress::UnixAddress(const std::string &path, int type, int protocol)
: Address(type, protocol)
{
    sun.sun_family = AF_UNIX;
    length = path.size() + 1;
#ifdef LINUX
    // Abstract namespace; leading NULL, but no trailing NULL
    if (!path.empty() && path[0] == '\0')
        --length;
#endif
    MORDOR_ASSERT(length <= sizeof(sun.sun_path));
    memcpy(sun.sun_path, path.c_str(), length);
    length += offsetof(sockaddr_un, sun_path);
}

std::ostream &
UnixAddress::insert(std::ostream &os) const
{
#ifdef LINUX
    if (length > offsetof(sockaddr_un, sun_path) &&
        sun.sun_path[0] == '\0')
        return os << "\\0" << std::string(sun.sun_path + 1,
            length - offsetof(sockaddr_un, sun_path) - 1);
#endif
    return os << sun.sun_path;
}
#endif

UnknownAddress::UnknownAddress(int family, int type, int protocol)
: Address(type, protocol)
{
    sa.sa_family = family;
}

std::ostream &operator <<(std::ostream &os, const Address &addr)
{
    return addr.insert(os);
}

}
