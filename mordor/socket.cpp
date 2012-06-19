// Copyright (c) 2009 - Mozy, Inc.

#include "socket.h"

#include <boost/bind.hpp>

#include "assert.h"
#include "config.h"
#include "fiber.h"
#include "iomanager.h"
#include "string.h"
#include "version.h"

#ifdef WINDOWS
#include <mswsock.h>
#include <IPHlpApi.h>

#include "runtime_linking.h"

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "mswsock")
#pragma comment(lib, "iphlpapi")
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#define closesocket close
#endif

namespace Mordor {

#ifdef WINDOWS
static ConfigVar<bool>::ptr g_useConnectEx =
        Config::lookup("socket.useconnectex", true, "Use WinSock2 ConnectEx API when available");
static ConfigVar<bool>::ptr g_useAcceptEx =
        Config::lookup("socket.useacceptex", true, "Use WinSock2 AcceptEx API when available");
#endif

namespace {
enum Family
{
    UNSPECIFIED = AF_UNSPEC,
    IP4 = AF_INET,
    IP6 = AF_INET6
};
enum Type
{
    STREAM = SOCK_STREAM,
    DATAGRAM = SOCK_DGRAM
};
enum Protocol
{
    ANY = 0,
    TCP = IPPROTO_TCP,
    UDP = IPPROTO_UDP
};

std::ostream &operator <<(std::ostream &os, Family family)
{
    switch (family)
    {
        case UNSPECIFIED:
            return os << "AF_UNSPEC";
        case IP4:
            return os << "AF_INET";
        case IP6:
            return os << "AF_INET6";
        default:
            return os << (int)family;
    }
}

std::ostream &operator <<(std::ostream &os, Type type)
{
    switch (type)
    {
        case STREAM:
            return os << "SOCK_STREAM";
        case DATAGRAM:
            return os << "SOCK_DGRAM";
        default:
            return os << (int)type;
    }
}

std::ostream &operator <<(std::ostream &os, Protocol protocol)
{
    switch (protocol)
    {
        case TCP:
            return os << "IPPROTO_TCP";
        case UDP:
            return os << "IPPROTO_UDP";
        default:
            return os << (int)protocol;
    }
}
}

#ifdef WINDOWS

static LPFN_ACCEPTEX pAcceptEx = 0;
static LPFN_GETACCEPTEXSOCKADDRS pGetAcceptExSockaddrs = 0;
static LPFN_CONNECTEX ConnectEx = 0;

static const size_t MAX_INTERFACE_BUFFER_SIZE = 131072;

namespace {

static struct Initializer {
    Initializer()
    {
        WSADATA wd;
        WSAStartup(MAKEWORD(2,2), &wd);

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
static int g_iosPortIndex;

namespace {
static struct IOSInitializer {
    IOSInitializer()
    {
        g_iosPortIndex = std::ios_base::xalloc();
    }
} g_iosInit;
}

Socket::Socket(IOManager *ioManager, int family, int type, int protocol, int initialize)
: m_sock(-1),
  m_family(family),
  m_protocol(protocol),
  m_ioManager(ioManager),
  m_receiveTimeout(~0ull),
  m_sendTimeout(~0ull),
  m_cancelledSend(0),
  m_cancelledReceive(0),
#ifdef WINDOWS
  m_hEvent(NULL),
  m_scheduler(NULL),
#endif
  m_isConnected(false),
  m_isRegisteredForRemoteClose(false)
{
    // Windows accepts type == 0 as implying SOCK_STREAM; other OS's aren't so
    // lenient
    MORDOR_ASSERT(type != 0);
#ifdef WINDOWS
    m_useAcceptEx = g_useAcceptEx->val();   //remember the setting for the entire life of the socket
    m_useConnectEx = g_useConnectEx->val(); //in case it is changed in the registry
    if (m_useAcceptEx && pAcceptEx && m_ioManager) {
        m_sock = socket(family, type, protocol);
        MORDOR_LOG_LEVEL(g_log, m_sock == -1 ? Log::ERROR : Log::DEBUG) << this
            << " socket(" << (Family)family << ", " << (Type)type << ", "
            << (Protocol)protocol << "): " << m_sock << " (" << lastError()
            << ")";
        if (m_sock == -1)
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("socket");
    }
#endif
}

Socket::Socket(int family, int type, int protocol)
: m_sock(-1),
  m_family(family),
  m_protocol(protocol),
  m_ioManager(NULL),
  m_isConnected(false),
  m_isRegisteredForRemoteClose(false)
{
#ifdef WINDOWS
    m_useAcceptEx = g_useAcceptEx->val();   //remember the setting for the entire life of the socket
    m_useConnectEx = g_useConnectEx->val(); //in case it is changed in the registry
#endif
    // Windows accepts type == 0 as implying SOCK_STREAM; other OS's aren't so
    // lenient
    MORDOR_ASSERT(type != 0);
    m_sock = socket(family, type, protocol);
    MORDOR_LOG_DEBUG(g_log) << this << " socket(" << (Family)family << ", "
        << (Type)type << ", " << (Protocol)protocol << "): " << m_sock << " ("
        << lastError() << ")";
    if (m_sock == -1)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("socket");
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
  m_cancelledReceive(0),
#ifdef WINDOWS
  m_hEvent(NULL),
  m_scheduler(NULL),
#endif
  m_isConnected(false),
  m_isRegisteredForRemoteClose(false)
{
#ifdef WINDOWS
    m_useAcceptEx = g_useAcceptEx->val();   //remember the setting for the entire life of the socket
    m_useConnectEx = g_useConnectEx->val(); //in case it is changed in the registry
#endif
    // Windows accepts type == 0 as implying SOCK_STREAM; other OS's aren't so
    // lenient
    MORDOR_ASSERT(type != 0);
    m_sock = socket(family, type, protocol);
    MORDOR_LOG_DEBUG(g_log) << this << " socket(" << (Family)family << ", "
        << (Type)type << ", " << (Protocol)protocol << "): " << m_sock << " ("
        << lastError() << ")";
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
#ifdef WINDOWS
    if (m_ioManager && m_hEvent) {
        if (m_isRegisteredForRemoteClose) {
            m_ioManager->unregisterEvent(m_hEvent);
            WSAEventSelect(m_sock, m_hEvent, 0);
        }
        CloseHandle(m_hEvent);
    }
#else
    if (m_isRegisteredForRemoteClose)
        m_ioManager->unregisterEvent(m_sock, IOManager::CLOSE);
#endif
    if (m_sock != -1) {
        int rc = ::closesocket(m_sock);
        if (rc) {
            MORDOR_LOG_ERROR(g_log) << this << " close(" << m_sock << "): ("
                << lastError() << ")";
        } else {
            MORDOR_LOG_INFO(g_log) << this << " close(" << m_sock << ")";
        }
    }
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
    localAddress();
}

void
Socket::bind(Address::ptr addr)
{
    bind(*addr);
}

void
Socket::connect(const Address &to)
{

    MORDOR_ASSERT(to.family() == m_family);
    if (!m_ioManager) {
        if (::connect(m_sock, to.name(), to.nameLen())) {
            MORDOR_LOG_ERROR(g_log) << this << " connect(" << m_sock << ", "
                << to << "): (" << lastError() << ")";
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("connect");
        }
        MORDOR_LOG_INFO(g_log) << this << " connect(" << m_sock << ", " << to << ")";
    } else {
#ifdef WINDOWS
        if (m_useConnectEx && ConnectEx) {
            if (!m_localAddress) {
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
                            MORDOR_LOG_DEBUG(g_log) << this << " bind(" << m_sock
                                << ", 0.0.0.0:0)";
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
                            MORDOR_LOG_DEBUG(g_log) << this << " bind(" << m_sock
                                << ", [::]:0)";
                            break;
                        }
                    default:
                        MORDOR_NOTREACHED();
                }
            }

            m_ioManager->registerEvent(&m_sendEvent);
            BOOL bRet = ConnectEx(m_sock, to.name(), to.nameLen(), NULL, 0, NULL, &m_sendEvent.overlapped);
            if (!bRet && lastError() != WSA_IO_PENDING) {
                if (lastError() == WSAEINVAL) {
                    m_ioManager->unregisterEvent(&m_sendEvent);
                    // Some LSPs are *borken* (I'm looking at you, bmnet.dll),
                    // and don't properly support ConnectEx (and AcceptEx).  In
                    // that case, go to how we work on Windows 2000 without
                    // ConnectEx at all
                    goto suckylsp;
                }
                MORDOR_LOG_ERROR(g_log) << this << " ConnectEx(" << m_sock
                    << ", " << to << "): (" << lastError() << ")";
                m_ioManager->unregisterEvent(&m_sendEvent);
                MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("ConnectEx");
            }
            if (m_skipCompletionPortOnSuccess && bRet) {
                m_ioManager->unregisterEvent(&m_sendEvent);
                m_sendEvent.overlapped.Internal = STATUS_SUCCESS;
            } else {
                if (m_cancelledSend) {
                    MORDOR_LOG_ERROR(g_log) << this << " ConnectEx(" << m_sock << ", " << to
                            << "): (" << m_cancelledSend << ")";
                    m_ioManager->cancelEvent((HANDLE)m_sock, &m_sendEvent);
                    Scheduler::yieldTo();
                    MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledSend, "ConnectEx");
                }
                Timer::ptr timeout;
                if (m_sendTimeout != ~0ull)
                    timeout = m_ioManager->registerTimer(m_sendTimeout, boost::bind(
                        &IOManager::cancelEvent, m_ioManager, (HANDLE)m_sock, &m_sendEvent));
                Scheduler::yieldTo();

                if (timeout)
                    timeout->cancel();
            }
            error_t error = pRtlNtStatusToDosError((NTSTATUS)m_sendEvent.overlapped.Internal);
            if (error == ERROR_OPERATION_ABORTED &&
                m_cancelledSend != ERROR_OPERATION_ABORTED)
                error = WSAETIMEDOUT;
            // WTF, Windows!?
            if (error == ERROR_SEM_TIMEOUT)
                error = WSAETIMEDOUT;
            if (error) {
                MORDOR_LOG_ERROR(g_log) << this << " ConnectEx(" << m_sock
                    << ", " << to << "): (" << error << ")";
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "ConnectEx");
            }
            MORDOR_LOG_INFO(g_log) << this << " ConnectEx(" << m_sock << ", "
                << to << ")";
            setOption(SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
        } else {
suckylsp:
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
            if (lastError() == WSAEWOULDBLOCK) {
                m_ioManager->registerEvent(m_hEvent);
                m_fiber = Fiber::getThis();
                m_scheduler = Scheduler::getThis();
                m_unregistered = false;
                if (m_cancelledSend) {
                    MORDOR_LOG_ERROR(g_log) << this << " connect(" << m_sock << ", " << to
                            << "): (" << m_cancelledSend << ")";
                    if (!m_ioManager->unregisterEvent(m_hEvent))
                        Scheduler::yieldTo();
                    MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledSend, "connect");
                }
                Timer::ptr timeout;
                if (m_sendTimeout != ~0ull)
                    timeout = m_ioManager->registerTimer(m_sendTimeout,
                        boost::bind(&Socket::cancelIo, this,
                            boost::ref(m_cancelledSend), WSAETIMEDOUT));
                Scheduler::yieldTo();

                m_fiber.reset();
                m_scheduler = NULL;
                if (timeout)
                    timeout->cancel();
                // The timeout expired, but the event fired before we could
                // cancel it, so we got scheduled twice
                if (m_cancelledSend && !m_unregistered)
                    Scheduler::yieldTo();
                if (m_cancelledSend) {
                    MORDOR_LOG_ERROR(g_log) << this << " connect(" << m_sock
                        << ", " << to << "): (" << m_cancelledSend << ")";
                    MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledSend, "connect");
                }
                // get the result of the connect operation
                WSANETWORKEVENTS events;
                if (0 != WSAEnumNetworkEvents(m_sock, NULL, &events))
                    MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WSAEnumNetworkEvents");
                // fall back to general socket error if no connect error recorded
                error_t error = (events.lNetworkEvents & FD_CONNECT) ?
                    events.iErrorCode[FD_CONNECT_BIT] :
                    getOption<int>(SOL_SOCKET, SO_ERROR);
                MORDOR_LOG_LEVEL(g_log, error ? Log::ERROR : Log::INFO)
                    << this << " connect(" << m_sock << ", " << to
                    << "): (" << error << ")";
                if (error)
                    MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "connect");
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
                Scheduler::yieldTo();
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledSend, "connect");
            }
            Timer::ptr timeout;
            if (m_sendTimeout != ~0ull)
                timeout = m_ioManager->registerTimer(m_sendTimeout, boost::bind(
                    &Socket::cancelIo, this, IOManager::WRITE,
                    boost::ref(m_cancelledSend), ETIMEDOUT));
            Scheduler::yieldTo();
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
        m_isConnected = true;
        if (!m_onRemoteClose.empty())
            registerForRemoteClose();
    }
}

void
Socket::listen(int backlog)
{
    int rc = ::listen(m_sock, backlog);
    MORDOR_LOG_LEVEL(g_log, rc ? Log::ERROR : Log::DEBUG) << this << " listen("
        << m_sock << ", " << backlog << "): " << rc << " (" << lastError()
        << ")";
    if (rc)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("listen");
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
    if (m_useAcceptEx && pAcceptEx && m_ioManager) {
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
        if (newsock == -1) {
            MORDOR_LOG_ERROR(g_log) << this << " accept(" << m_sock << "): "
                << newsock << " (" << lastError() << ")";
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("accept");
        }
        target.m_sock = newsock;
        MORDOR_LOG_INFO(g_log) << this << " accept(" << m_sock << "): "
                << newsock << " (" << *target.remoteAddress() << ')';
    } else {
#ifdef WINDOWS
        if (m_useAcceptEx && pAcceptEx) {
            m_ioManager->registerEvent(&m_receiveEvent);
            unsigned char addrs[sizeof(SOCKADDR_STORAGE) * 2 + 16];
            DWORD bytes;
            BOOL ret = pAcceptEx(m_sock, target.m_sock, addrs, 0, sizeof(SOCKADDR_STORAGE) + 16, sizeof(SOCKADDR_STORAGE) + 16, &bytes,
                &m_receiveEvent.overlapped);
            if (!ret && lastError() != WSA_IO_PENDING) {
                if (lastError() == WSAENOTSOCK) {
                    m_ioManager->unregisterEvent(&m_receiveEvent);
                    // See comment in similar line in connect()
                    goto suckylsp;
                }
                MORDOR_LOG_ERROR(g_log) << this << " AcceptEx(" << m_sock << "):  ("
                    << lastError() << ")";
                m_ioManager->unregisterEvent(&m_receiveEvent);
                MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("AcceptEx");
            }
            if (m_skipCompletionPortOnSuccess && ret) {
                m_ioManager->unregisterEvent(&m_receiveEvent);
                m_receiveEvent.overlapped.Internal = STATUS_SUCCESS;
            } else {
                if (m_cancelledReceive) {
                    MORDOR_LOG_ERROR(g_log) << this << " AcceptEx(" << m_sock << "): ("
                        << m_cancelledReceive << ")";
                    m_ioManager->cancelEvent((HANDLE)m_sock, &m_receiveEvent);
                    Scheduler::yieldTo();
                    MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledReceive, "AcceptEx");
                }
                Timer::ptr timeout;
                if (m_receiveTimeout != ~0ull)
                    timeout = m_ioManager->registerTimer(m_receiveTimeout, boost::bind(
                        &IOManager::cancelEvent, m_ioManager, (HANDLE)m_sock, &m_receiveEvent));
                Scheduler::yieldTo();
                if (timeout)
                    timeout->cancel();
            }
            error_t error = pRtlNtStatusToDosError((NTSTATUS)m_receiveEvent.overlapped.Internal);
            if (error && error != ERROR_MORE_DATA) {
                if (error == ERROR_OPERATION_ABORTED &&
                    m_cancelledReceive != ERROR_OPERATION_ABORTED)
                    error = WSAETIMEDOUT;
                MORDOR_LOG_ERROR(g_log) << this << " AcceptEx(" << m_sock << "): ("
                    << error << ")";
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "AcceptEx");
            }
            sockaddr *localAddr = NULL, *remoteAddr = NULL;
            INT localAddrLen, remoteAddrLen;
            if (m_useAcceptEx && pGetAcceptExSockaddrs)
                pGetAcceptExSockaddrs(addrs, 0, sizeof(SOCKADDR_STORAGE) + 16,
                    sizeof(SOCKADDR_STORAGE) + 16, &localAddr, &localAddrLen,
                    &remoteAddr, &remoteAddrLen);
            if (remoteAddr)
                target.m_remoteAddress = Address::create(remoteAddr, remoteAddrLen);

            std::ostringstream os;
            MORDOR_LOG_INFO(g_log) << this << " AcceptEx(" << m_sock << "): "
                << target.m_sock << " (" << *target.remoteAddress() << ')';
            target.setOption(SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, &m_sock, sizeof(m_sock));
            target.m_ioManager->registerFile((HANDLE)target.m_sock);
            target.m_skipCompletionPortOnSuccess =
                !!pSetFileCompletionNotificationModes((HANDLE)target.m_sock,
                    FILE_SKIP_COMPLETION_PORT_ON_SUCCESS |
                    FILE_SKIP_SET_EVENT_ON_HANDLE);
        } else {
suckylsp:
            if (!m_hEvent) {
                m_hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
                if (!m_hEvent)
                    MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateEventW");
            }
            if (!ResetEvent(m_hEvent))
                MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("ResetEvent");
            if (WSAEventSelect(m_sock, m_hEvent, FD_ACCEPT))
                MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WSAEventSelect");
            socket_t newsock = ::accept(m_sock, NULL, NULL);
            if (newsock != -1) {
                // Worked first time
            } else if (lastError() == WSAEWOULDBLOCK) {
                m_ioManager->registerEvent(m_hEvent);
                m_fiber = Fiber::getThis();
                m_scheduler = Scheduler::getThis();
                if (m_cancelledReceive) {
                    MORDOR_LOG_ERROR(g_log) << this << " accept(" << m_sock << "): ("
                        << m_cancelledReceive << ")";
                    if (!m_ioManager->unregisterEvent(m_hEvent))
                        Scheduler::yieldTo();
                    MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledReceive, "accept");
                }
                m_unregistered = false;
                Timer::ptr timeout;
                if (m_receiveTimeout != ~0ull)
                    timeout = m_ioManager->registerTimer(m_sendTimeout,
                        boost::bind(&Socket::cancelIo, this,
                        boost::ref(m_cancelledReceive), WSAETIMEDOUT));
                Scheduler::yieldTo();
                m_fiber.reset();
                m_scheduler = NULL;
                if (timeout)
                    timeout->cancel();
                // The timeout expired, but the event fired before we could
                // cancel it, so we got scheduled twice
                if (m_cancelledReceive && !m_unregistered)
                    Scheduler::yieldTo();
                if (m_cancelledReceive) {
                    MORDOR_LOG_ERROR(g_log) << this << " accept(" << m_sock
                        << "): (" << m_cancelledReceive << ")";
                    MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledReceive, "accept");
                }
                newsock = ::accept(m_sock, NULL, NULL);
                if (newsock == -1) {
                    MORDOR_LOG_ERROR(g_log) << this << " accept(" << m_sock
                        << "): " << newsock << " (" << lastError() << ")";
                    MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("accept");
                }
            } else {
                MORDOR_LOG_ERROR(g_log) << this << " accept(" << m_sock << "): ("
                    << lastError() << ")";
                MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("accept");
            }
            try {
                m_ioManager->registerFile((HANDLE)newsock);
            } catch(...) {
                closesocket(newsock);
                throw;
            }
            if (target.m_sock != -1)
                ::closesocket(target.m_sock);
            target.m_sock = newsock;
            MORDOR_LOG_INFO(g_log) << this << " accept(" << m_sock << "): "
                << newsock << " (" << *target.remoteAddress() << ')';
            target.m_skipCompletionPortOnSuccess =
                !!pSetFileCompletionNotificationModes((HANDLE)newsock,
                    FILE_SKIP_COMPLETION_PORT_ON_SUCCESS |
                    FILE_SKIP_SET_EVENT_ON_HANDLE);
        }
#else
        int newsock;
        error_t error;
        do {
            newsock = ::accept(m_sock, NULL, NULL);
            error = errno;
        } while (newsock == -1 && error == EINTR);
        while (newsock == -1 && error == EAGAIN) {
            m_ioManager->registerEvent(m_sock, IOManager::READ);
            if (m_cancelledReceive) {
                MORDOR_LOG_ERROR(g_log) << this << " accept(" << m_sock << "): ("
                    << m_cancelledReceive << ")";
                m_ioManager->cancelEvent(m_sock, IOManager::READ);
                Scheduler::yieldTo();
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledReceive, "accept");
            }
            Timer::ptr timeout;
            if (m_receiveTimeout != ~0ull)
                timeout = m_ioManager->registerTimer(m_receiveTimeout, boost::bind(
                    &Socket::cancelIo, this, IOManager::READ,
                    boost::ref(m_cancelledReceive), ETIMEDOUT));
            Scheduler::yieldTo();
            if (timeout)
                timeout->cancel();
            if (m_cancelledReceive) {
                MORDOR_LOG_ERROR(g_log) << this << " accept(" << m_sock
                    << "): (" << m_cancelledReceive << ")";
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(m_cancelledReceive, "accept");
            }
            do {
                newsock = ::accept(m_sock, NULL, NULL);
                error = errno;
            } while (newsock == -1 && error == EINTR);
        }
        if (newsock == -1) {
            MORDOR_LOG_ERROR(g_log) << this << " accept(" << m_sock << "): "
                << newsock << " (" << error << ")";
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("accept");
        }
        if (fcntl(newsock, F_SETFL, O_NONBLOCK) == -1) {
            ::close(newsock);
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("fcntl");
        }
        target.m_sock = newsock;
        MORDOR_LOG_INFO(g_log) << this << " accept(" << m_sock << "): "
            << newsock << " (" << *target.remoteAddress() << ')';
#endif
        target.m_isConnected = true;
        if (!target.m_onRemoteClose.empty())
            target.registerForRemoteClose();
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
    if (m_isRegisteredForRemoteClose) {
#ifdef WINDOWS
        m_ioManager->unregisterEvent(m_hEvent);
        WSAEventSelect(m_sock, m_hEvent, 0);
#else
        m_ioManager->unregisterEvent(m_sock, IOManager::CLOSE);
#endif
        m_isRegisteredForRemoteClose = false;
    }
    m_isConnected = false;
    MORDOR_LOG_VERBOSE(g_log) << this << " shutdown(" << m_sock << ", "
        << how << ")";
}

#define MORDOR_SOCKET_LOG(result, error)                                        \
    if (g_log->enabled(result == -1 ? Log::ERROR : Log::DEBUG)) {               \
        LogEvent event = g_log->log(result == -1 ? Log::ERROR : Log::DEBUG,     \
            __FILE__, __LINE__);                                                \
        event.os() << this << " " << api << "(" << m_sock << ", "              \
            << length;                                                          \
        if (isSend && address)                                                  \
            event.os() << ", " << *address;                                     \
        event.os() << "): ";                                                    \
        if (result == -1)                                                       \
            event.os() << "(" << error << ")";                                  \
        else                                                                    \
            event.os() << result;                                               \
        if (result != -1 && !isSend && address)                                 \
            event.os() << ", " << *address;                                     \
    }

template <bool isSend>
size_t
Socket::doIO(iovec *buffers, size_t length, int &flags, Address *address)
{
#if !defined(WINDOWS) && !defined(OSX)
    flags |= MSG_NOSIGNAL;
#endif

#ifdef WINDOWS
    const char *api = isSend ? (address ? "WSASendTo" : "WSASend") :
        (address ? "WSARecvFrom" : "WSARecv");
#else
    const char *api = isSend ? "sendmsg" : "recvmsg";
#endif
    error_t &cancelled = isSend ? m_cancelledSend : m_cancelledReceive;
    unsigned long long &timeout = isSend ? m_sendTimeout : m_receiveTimeout;

#ifdef WINDOWS
    DWORD bufferCount = (DWORD)std::min<size_t>(length, 0xffffffff);
    AsyncEvent &event = isSend ? m_sendEvent : m_receiveEvent;
    OVERLAPPED *overlapped = m_ioManager ? &event.overlapped : NULL;

    if (m_ioManager) {
        if (cancelled) {
            MORDOR_SOCKET_LOG(-1, cancelled);
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(cancelled, api);
        }
        m_ioManager->registerEvent(&event);
    }

    DWORD transferred;
    int result;
    INT addrLen;
    if (address) {
        addrLen = address->nameLen();
        result = isSend ? WSASendTo(m_sock,
            (LPWSABUF)buffers,
            bufferCount,
            &transferred,
            flags,
            address->name(),
            address->nameLen(),
            overlapped,
            NULL) : WSARecvFrom(m_sock,
            (LPWSABUF)buffers,
            bufferCount,
            &transferred,
            (LPDWORD)&flags,
            address->name(),
            &addrLen,
            overlapped,
            NULL);
    } else {
        result = isSend ? WSASend(m_sock,
            (LPWSABUF)buffers,
            bufferCount,
            &transferred,
            flags,
            overlapped,
            NULL) : WSARecv(m_sock,
            (LPWSABUF)buffers,
            bufferCount,
            &transferred,
            (LPDWORD)&flags,
            overlapped,
            NULL);
    }

    if (result) {
        if (!m_ioManager || lastError() != WSA_IO_PENDING) {
            MORDOR_SOCKET_LOG(-1, lastError());
            if (m_ioManager)
                m_ioManager->unregisterEvent(&event);
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API(api);
        }
    }

    if (m_ioManager) {
        if (m_skipCompletionPortOnSuccess && result == 0) {
            m_ioManager->unregisterEvent(&event);
        } else {
            Timer::ptr timer;
            if (timeout != ~0ull)
                timer = m_ioManager->registerTimer(timeout, boost::bind(
                    &IOManager::cancelEvent, m_ioManager, (HANDLE)m_sock,
                    &event));
            Scheduler::yieldTo();
            if (timer)
                timer->cancel();
        }
        error_t error = pRtlNtStatusToDosError(
            (NTSTATUS)event.overlapped.Internal);
        if (error == ERROR_OPERATION_ABORTED &&
            cancelled != ERROR_OPERATION_ABORTED)
            error = WSAETIMEDOUT;
        if (error == ERROR_SEM_TIMEOUT)
            error = WSAETIMEDOUT;
        result = error ? -1 : (int)event.overlapped.InternalHigh;
        MORDOR_SOCKET_LOG(result, error);
        if (error)
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, api);
        return result;
    }
    MORDOR_SOCKET_LOG(transferred, lastError());
    return transferred;
#else
    msghdr msg;
    memset(&msg, 0, sizeof(msghdr));
    msg.msg_iov = buffers;
    msg.msg_iovlen = length;
    if (address) {
        msg.msg_name = (sockaddr *)address->name();
        msg.msg_namelen = address->nameLen();
    }
    IOManager::Event event = isSend ? IOManager::WRITE : IOManager::READ;
    if (m_ioManager) {
        if (cancelled) {
            MORDOR_SOCKET_LOG(-1, cancelled);
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(cancelled, api);
        }
    }
    int rc;
    error_t error;
    do {
        rc = isSend ? sendmsg(m_sock, &msg, flags) : recvmsg(m_sock, &msg, flags);
        error = errno;
    } while (rc == -1 && error == EINTR);
    while (m_ioManager && rc == -1 && error == EAGAIN) {
        m_ioManager->registerEvent(m_sock, event);
        Timer::ptr timer;
        if (timeout != ~0ull)
            timer = m_ioManager->registerTimer(timeout, boost::bind(
                &Socket::cancelIo, this, event, boost::ref(cancelled),
                ETIMEDOUT));
        Scheduler::yieldTo();
        if (timer)
            timer->cancel();
        if (cancelled) {
            MORDOR_SOCKET_LOG(-1, cancelled);
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(cancelled, api);
        }
        do {
            rc = isSend ? sendmsg(m_sock, &msg, flags) : recvmsg(m_sock, &msg, flags);
            error = errno;
        } while (rc == -1 && error == EINTR);
    }
    MORDOR_SOCKET_LOG(rc, error);
    if (rc == -1)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API(api);
    if (!isSend)
        flags = msg.msg_flags;
    return rc;
#endif
}

size_t
Socket::send(const void *buffer, size_t length, int flags)
{
    iovec buffers;
    buffers.iov_base = (void *)buffer;
    buffers.iov_len = (u_long)std::min<size_t>(length, 0xffffffff);
    return doIO<true>(&buffers, 1, flags);
}

size_t
Socket::send(const iovec *buffers, size_t length, int flags)
{
    return doIO<true>((iovec *)buffers, length, flags);
}

size_t
Socket::sendTo(const void *buffer, size_t length, int flags, const Address &to)
{
    iovec buffers;
    buffers.iov_base = (void *)buffer;
    buffers.iov_len = (u_long)std::min<size_t>(length, 0xffffffff);
    return doIO<true>(&buffers, 1, flags, (Address *)&to);
}

size_t
Socket::sendTo(const iovec *buffers, size_t length, int flags,
               const Address &to)
{
    return doIO<true>((iovec *)buffers, length, flags, (Address *)&to);
}

size_t
Socket::receive(void *buffer, size_t length, int *flags)
{
    iovec buffers;
    buffers.iov_base = buffer;
    buffers.iov_len = (u_long)std::min<size_t>(length, 0xffffffff);
    int flagStorage = 0;
    if (!flags)
        flags = &flagStorage;
    return doIO<false>(&buffers, 1, *flags);
}

size_t
Socket::receive(iovec *buffers, size_t length, int *flags)
{
    int flagStorage = 0;
    if (!flags)
        flags = &flagStorage;
    return doIO<false>(buffers, length, *flags);
}

size_t
Socket::receiveFrom(void *buffer, size_t length, Address &from, int *flags)
{
    iovec buffers;
    buffers.iov_base = buffer;
    buffers.iov_len = (u_long)std::min<size_t>(length, 0xffffffff);
    int flagStorage = 0;
    if (!flags)
        flags = &flagStorage;
    return doIO<false>(&buffers, 1, *flags, &from);
}

size_t
Socket::receiveFrom(iovec *buffers, size_t length, Address &from, int *flags)
{
    int flagStorage = 0;
    if (!flags)
        flags = &flagStorage;
    return doIO<false>(buffers, length, *flags, &from);
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
        error_t error = lastError();
        MORDOR_LOG_ERROR(g_log) << this << " setsockopt(" << m_sock << ", "
            << level << ", " << option << "): (" << error << ")";
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("setsockopt");
    }
    MORDOR_LOG_DEBUG(g_log) << this << " setsockopt(" << m_sock << ", "
        << level << ", " << option << "): 0";
}

void
Socket::cancelAccept()
{
    MORDOR_ASSERT(m_ioManager);

#ifdef WINDOWS
    if (m_cancelledReceive)
        return;
    m_cancelledReceive = ERROR_OPERATION_ABORTED;
    if (m_useAcceptEx && pAcceptEx) {
        m_ioManager->cancelEvent((HANDLE)m_sock, &m_receiveEvent);
    }
    if (m_hEvent && m_scheduler && m_fiber) {
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
    if (m_useConnectEx && ConnectEx) {
        m_ioManager->cancelEvent((HANDLE)m_sock, &m_sendEvent);
    }
    if (m_hEvent && m_scheduler && m_fiber) {
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

#ifdef WINDOWS
void
Socket::cancelIo(error_t &cancelled, error_t error)
{
    MORDOR_ASSERT(error);
    if (cancelled)
        return;
    cancelled = error;
    if (m_hEvent && m_scheduler && m_fiber) {
        m_unregistered = !!m_ioManager->unregisterEvent(m_hEvent);
        m_scheduler->schedule(m_fiber);
    }
}
#else
void
Socket::cancelIo(int event, error_t &cancelled, error_t error)
{
    MORDOR_ASSERT(error);
    if (cancelled)
        return;
    cancelled = error;
    m_ioManager->cancelEvent(m_sock, (IOManager::Event)event);
}
#endif

Address::ptr
Socket::emptyAddress()
{
    switch (m_family) {
        case AF_INET:
            return Address::ptr(new IPv4Address());
        case AF_INET6:
            return Address::ptr(new IPv6Address());
        default:
            return Address::ptr(new UnknownAddress(m_family));
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
            result.reset(new IPv4Address());
            break;
        case AF_INET6:
            result.reset(new IPv6Address());
            break;
        default:
            result.reset(new UnknownAddress(m_family));
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
            result.reset(new IPv4Address());
            break;
        case AF_INET6:
            result.reset(new IPv6Address());
            break;
        default:
            result.reset(new UnknownAddress(m_family));
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

boost::signals2::connection
Socket::onRemoteClose(const boost::signals2::slot<void ()> &slot)
{
    boost::signals2::connection result = m_onRemoteClose.connect(slot);
    if (m_isConnected && !m_isRegisteredForRemoteClose)
        registerForRemoteClose();
    return result;
}

void
Socket::callOnRemoteClose(weak_ptr self)
{
    ptr strongSelf = self.lock();
    if (strongSelf)
        strongSelf->m_onRemoteClose();
}

void
Socket::registerForRemoteClose()
{
#ifdef WINDOWS
    // listen for the close event
    if (!m_hEvent) {
        m_hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!m_hEvent)
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateEventW");
    }
    if (!ResetEvent(m_hEvent))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("ResetEvent");
    if (WSAEventSelect(m_sock, m_hEvent, FD_CLOSE))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WSAEventSelect");
    m_ioManager->registerEvent(m_hEvent, boost::bind(&Socket::callOnRemoteClose,
        weak_ptr(shared_from_this())));
#else
    m_ioManager->registerEvent(m_sock, IOManager::CLOSE,
        boost::bind(&Socket::callOnRemoteClose, weak_ptr(shared_from_this())));
#endif
    m_isRegisteredForRemoteClose = true;
}

static void throwGaiException(int error)
{
    switch (error) {
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
        case EAI_FAMILY:
            MORDOR_THROW_EXCEPTION(OperationNotSupportedException()
                << errinfo_gaierror(error)
                << boost::errinfo_api_function("getaddrinfo"));
        case EAI_BADFLAGS:
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
}

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
        if (stricmp(service, "socks") == 0)
            serviceWStorage = L"1080";
        else
            serviceWStorage = toUtf16(service);
        serviceW = serviceWStorage.c_str();
    }
    error = pGetAddrInfoW(toUtf16(node).c_str(), serviceW, &hints, &results);
#else
    error = getaddrinfo(node.c_str(), service, &hints, &results);
#endif
    if (error) {
        MORDOR_LOG_ERROR(g_log) << "getaddrinfo(" << host << ", "
            << (Family)family << ", " << (Type)type << "): (" << error << ")";
        throwGaiException(error);
    }

    std::vector<Address::ptr> result;
    next = results;
    while (next) {
        result.push_back(create(next->ai_addr, (socklen_t)next->ai_addrlen));
        next = next->ai_next;
    }
#ifdef WINDOWS
    pFreeAddrInfoW(results);
#else
    freeaddrinfo(results);
#endif
    return result;
}

template <class T>
static unsigned int countBits(T value)
{
    unsigned int result = 0;
    // See http://www-graphics.stanford.edu/~seander/bithacks.html#CountBitsSetKernighan
    for (; value; ++result)
        value &= value - 1;
    return result;
}

std::multimap<std::string, std::pair<Address::ptr, unsigned int> >
Address::getInterfaceAddresses(int family)
{
    std::multimap<std::string, std::pair<Address::ptr, unsigned int> >
        result;
#ifdef WINDOWS
    std::vector<char> buf(15 * 1024);
    IP_ADAPTER_ADDRESSES *addresses = (IP_ADAPTER_ADDRESSES *)&buf[0];
    ULONG size = (ULONG)buf.size();
    ULONG error = pGetAdaptersAddresses(AF_UNSPEC, 0, NULL, addresses, &size);
    while (ERROR_BUFFER_OVERFLOW == error && buf.size() < MAX_INTERFACE_BUFFER_SIZE)
    {
        buf.resize(size);
        addresses = (IP_ADAPTER_ADDRESSES *)&buf[0];
        error = pGetAdaptersAddresses(AF_UNSPEC, 0, NULL, addresses, &size);
    }
    if (error && error != ERROR_CALL_NOT_IMPLEMENTED)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "GetAdaptersAddresses");
    // Either doesn't exist, or doesn't include netmask info to construct broadcast addr
    if (error == ERROR_CALL_NOT_IMPLEMENTED ||
        addresses->FirstUnicastAddress->Length < sizeof(IP_ADAPTER_UNICAST_ADDRESS_LH)) {
        PIP_ADAPTER_INFO addresses2 = (PIP_ADAPTER_INFO)&buf[0];
        size = (ULONG)buf.size();
        error = GetAdaptersInfo(addresses2, &size);
        while (ERROR_BUFFER_OVERFLOW == error && buf.size() < MAX_INTERFACE_BUFFER_SIZE)
        {
            buf.resize(size);
            addresses2 = (PIP_ADAPTER_INFO)&buf[0];
            error = GetAdaptersInfo(addresses2, &size);
        }
        if (error)
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, "PIP_ADAPTER_INFO");
        for (; addresses2; addresses2 = addresses2->Next) {
            std::string iface(addresses2->AdapterName);
            if (family != AF_INET && family != AF_UNSPEC)
                continue;
            IP_ADDR_STRING *address = &addresses2->IpAddressList;
            for (; address; address = address->Next) {
                sockaddr_in addr;
                memset(&addr, 0, sizeof(sockaddr_in));
                addr.sin_family = AF_INET;
                addr.sin_addr.s_addr = inet_addr(address->IpAddress.String);
                unsigned int mask = inet_addr(address->IpMask.String);
                result.insert(std::make_pair(iface, std::make_pair(
                    Address::create((sockaddr *)&addr,
                    sizeof(sockaddr_in)), countBits(mask))));
            }
        }

        return result;
    }

    for (; addresses; addresses = addresses->Next) {
        std::string iface(addresses->AdapterName);
        IP_ADAPTER_UNICAST_ADDRESS *address = addresses->FirstUnicastAddress;
        for (; address; address = address->Next) {
            if (family != AF_UNSPEC &&
                family != address->Address.lpSockaddr->sa_family)
                continue;
            Address::ptr addr = Address::create(address->Address.lpSockaddr,
                address->Address.iSockaddrLength);
            unsigned int prefixLength = ~0u;
            if (address->Address.lpSockaddr->sa_family == AF_INET &&
                address->OnLinkPrefixLength <= 32) {
                prefixLength = address->OnLinkPrefixLength;
            } else if (address->Address.lpSockaddr->sa_family == AF_INET6 &&
                address->OnLinkPrefixLength <= 128) {
                prefixLength = address->OnLinkPrefixLength;
            }

            result.insert(std::make_pair(iface,
                std::make_pair(addr, prefixLength)));
        }
    }
    return result;
#else
    struct ifaddrs *next, *results;
    if (getifaddrs(&results) != 0)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("getifaddrs");
    try {
        for (next = results; next; next = next->ifa_next) {
            Address::ptr address, baddress;
            unsigned int prefixLength = ~0u;
            if (family != AF_UNSPEC && family != next->ifa_addr->sa_family)
                continue;
            switch (next->ifa_addr->sa_family) {
                case AF_INET:
                {
                    address = create(next->ifa_addr, sizeof(sockaddr_in));
                    unsigned int netmask =
                        ((sockaddr_in *)next->ifa_netmask)->sin_addr.s_addr;
                    prefixLength = countBits(netmask);
                    break;
                }
                case AF_INET6:
                {
                    address = create(next->ifa_addr, sizeof(sockaddr_in6));
                    prefixLength = 0;
                    in6_addr &netmask = ((sockaddr_in6 *)next->ifa_netmask)->sin6_addr;
                    for (size_t i = 0; i < 16; ++i)
                        prefixLength += countBits(netmask.s6_addr[i]);
                    break;
                }
                default:
                    break;
            }
            if (address)
                result.insert(std::make_pair(next->ifa_name,
                    std::make_pair(address, prefixLength)));
        }
    } catch (...) {
        freeifaddrs(results);
        throw;
    }
    freeifaddrs(results);
    return result;
#endif
}

std::vector<std::pair<Address::ptr, unsigned int> >
Address::getInterfaceAddresses(const std::string &iface, int family)
{
    std::vector<std::pair<Address::ptr, unsigned int> > result;
    if (iface.empty() || iface == "*") {
        if (family == AF_INET || family == AF_UNSPEC)
            result.push_back(
                std::make_pair(Address::ptr(new IPv4Address()), 0u));
        if (family == AF_INET6 || family == AF_UNSPEC)
            result.push_back(
                std::make_pair(Address::ptr(new IPv6Address()), 0u));
        return result;
    }
    typedef std::multimap<std::string, std::pair<Address::ptr, unsigned int> >
        AddressesMap;
    AddressesMap interfaces = getInterfaceAddresses(family);
    std::pair<AddressesMap::iterator, AddressesMap::iterator> its =
        interfaces.equal_range(iface);
    for (; its.first != its.second; ++its.first)
        result.push_back(its.first->second);
    return result;
}

Address::ptr
Address::create(const sockaddr *name, socklen_t nameLen)
{
    MORDOR_ASSERT(name);
    Address::ptr result;
    switch (name->sa_family) {
        case AF_INET:
            result.reset(new IPv4Address());
            MORDOR_ASSERT(nameLen <= result->nameLen());
            memcpy(result->name(), name, nameLen);
            break;
        case AF_INET6:
            result.reset(new IPv6Address());
            MORDOR_ASSERT(nameLen <= result->nameLen());
            memcpy(result->name(), name, nameLen);
            break;
        default:
            result.reset(new UnknownAddress(name->sa_family));
            MORDOR_ASSERT(nameLen <= result->nameLen());
            memcpy(result->name(), name, nameLen);
            break;
    }
    return result;
}

Address::ptr
Address::clone()
{
    return create(name(), nameLen());
}

Socket::ptr
Address::createSocket(int type, int protocol)
{
    return Socket::ptr(new Socket(family(), type, protocol));
}

Socket::ptr
Address::createSocket(IOManager &ioManager, int type, int protocol)
{
    return Socket::ptr(new Socket(ioManager, family(), type, protocol));
}

std::ostream &
Address::insert(std::ostream &os) const
{
    return os << "(Unknown addr " << family() << ")";
}

bool
Address::operator<(const Address &rhs) const
{
    socklen_t minimum = (std::min)(nameLen(), rhs.nameLen());
    int result = memcmp(name(), rhs.name(), minimum);
    if (result < 0)
        return true;
    else if (result > 0)
        return false;
    if (nameLen() < rhs.nameLen())
        return true;
    return false;
}

bool
Address::operator==(const Address &rhs) const
{
    return nameLen() == rhs.nameLen() &&
        memcmp(name(), rhs.name(), nameLen()) == 0;
}

bool Address::operator!=(const Address &rhs) const
{
    return !(*this == rhs);
}

std::vector<IPAddress::ptr>
IPAddress::lookup(const std::string &host, int family, int type, int protocol,
                  int port)
{
    std::vector<Address::ptr> addrResult = Address::lookup(host, family, type,
            protocol);
    std::vector<ptr> result;
    result.reserve(addrResult.size());
    for (std::vector<Address::ptr>::const_iterator it(addrResult.begin());
         it != addrResult.end();
         ++it) {
        ptr addr = boost::dynamic_pointer_cast<IPAddress>(*it);
        if (addr) {
            if (port >= 0) addr->port(port);
            result.push_back(addr);
        }
    }
    return result;
}

IPAddress::ptr
IPAddress::clone()
{
    return boost::static_pointer_cast<IPAddress>(Address::clone());
}

IPAddress::ptr
IPAddress::create(const char *address, unsigned short port)
{
    addrinfo hints, *results;
    memset(&hints, 0, sizeof(addrinfo));
    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_family = AF_UNSPEC;
    int error = getaddrinfo(address, NULL, &hints, &results);
    if (error == EAI_NONAME) {
        MORDOR_THROW_EXCEPTION(std::invalid_argument("address"));
    } else if (error) {
        MORDOR_LOG_ERROR(g_log) << "getaddrinfo(" << address
            << ", AI_NUMERICHOST): (" << error << ")";
        throwGaiException(error);
    }
    try {
        IPAddress::ptr result = boost::static_pointer_cast<IPAddress>(
            Address::create(results->ai_addr, (socklen_t)results->ai_addrlen));
        result->port(port);
        freeaddrinfo(results);
        return result;
    } catch (...) {
        freeaddrinfo(results);
        throw;
    }
}

IPv4Address::IPv4Address(unsigned int address, unsigned short port)
{
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = byteswapOnLittleEndian(port);
    sin.sin_addr.s_addr = byteswapOnLittleEndian(address);
}

#ifndef WINDOWS
static int pinet_pton(int af, const char *src, void *dst)
{
    return inet_pton(af, src, dst);
}
#endif

IPv4Address::IPv4Address(const char *address, unsigned short port)
{
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = byteswapOnLittleEndian(port);
    int result = pinet_pton(AF_INET, address, &sin.sin_addr);
    if (result == 0)
        MORDOR_THROW_EXCEPTION(std::invalid_argument("address"));
    if (result < 0)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("inet_pton");
}

// Returns an integer with bits 1s
template <class T>
static T createMask(unsigned int bits)
{
    MORDOR_ASSERT(bits <= sizeof(T) * 8);
    return (1 << (sizeof(T) * 8 - bits)) - 1;
}

IPv4Address::ptr
IPv4Address::broadcastAddress(unsigned int prefixLength)
{
    MORDOR_ASSERT(prefixLength <= 32);
    sockaddr_in baddr(sin);
    baddr.sin_addr.s_addr |= byteswapOnLittleEndian(
        createMask<unsigned int>(prefixLength));
    return boost::static_pointer_cast<IPv4Address>(
        Address::create((const sockaddr *)&baddr, sizeof(sockaddr_in)));
}

IPv4Address::ptr
IPv4Address::networkAddress(unsigned int prefixLength)
{
    MORDOR_ASSERT(prefixLength <= 32);
    sockaddr_in baddr(sin);
    baddr.sin_addr.s_addr &= byteswapOnLittleEndian(
        ~createMask<unsigned int>(prefixLength));
    return boost::static_pointer_cast<IPv4Address>(
        Address::create((const sockaddr *)&baddr, sizeof(sockaddr_in)));
}

IPv4Address::ptr
IPv4Address::createSubnetMask(unsigned int prefixLength)
{
    MORDOR_ASSERT(prefixLength <= 32);
    sockaddr_in subnet;
    memset(&subnet, 0, sizeof(sockaddr_in));
    subnet.sin_family = AF_INET;
    subnet.sin_addr.s_addr = byteswapOnLittleEndian(
        ~createMask<unsigned int>(prefixLength));
    return boost::static_pointer_cast<IPv4Address>(
        Address::create((const sockaddr *)&subnet, sizeof(sockaddr_in)));
}

std::ostream &
IPv4Address::insert(std::ostream &os) const
{
    int addr = byteswapOnLittleEndian(sin.sin_addr.s_addr);
    os << ((addr >> 24) & 0xff) << '.'
        << ((addr >> 16) & 0xff) << '.'
        << ((addr >> 8) & 0xff) << '.'
        << (addr & 0xff);
    // "on" is 0, so that it's the default
    if (!os.iword(g_iosPortIndex))
        os << ':' << byteswapOnLittleEndian(sin.sin_port);
    return os;
}

IPv6Address::IPv6Address()
{
    memset(&sin, 0, sizeof(sockaddr_in6));
    sin.sin6_family = AF_INET6;
}

IPv6Address::IPv6Address(const unsigned char address[16], unsigned short port)
{
    memset(&sin, 0, sizeof(sockaddr_in6));
    sin.sin6_family = AF_INET6;
    sin.sin6_port = byteswapOnLittleEndian(port);
    memcpy(&sin.sin6_addr.s6_addr, address, 16);
}

IPv6Address::IPv6Address(const char *address, unsigned short port)
{
    memset(&sin, 0, sizeof(sockaddr_in6));
    sin.sin6_family = AF_INET6;
    sin.sin6_port = byteswapOnLittleEndian(port);
    int result = pinet_pton(AF_INET6, address, &sin.sin6_addr);
    if (result == 0)
        MORDOR_THROW_EXCEPTION(std::invalid_argument("address"));
    if (result < 0)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("inet_pton");
}

IPv6Address::ptr
IPv6Address::broadcastAddress(unsigned int prefixLength)
{
    MORDOR_ASSERT(prefixLength <= 128);
    sockaddr_in6 baddr(sin);
    baddr.sin6_addr.s6_addr[prefixLength / 8] |=
        createMask<unsigned char>(prefixLength % 8);
    for (unsigned int i = prefixLength / 8 + 1; i < 16; ++i)
        baddr.sin6_addr.s6_addr[i] = 0xffu;
    return boost::static_pointer_cast<IPv6Address>(
        Address::create((const sockaddr *)&baddr, sizeof(sockaddr_in6)));
}

IPv6Address::ptr
IPv6Address::networkAddress(unsigned int prefixLength)
{
    MORDOR_ASSERT(prefixLength <= 128);
    sockaddr_in6 baddr(sin);
    baddr.sin6_addr.s6_addr[prefixLength / 8] &=
        ~createMask<unsigned char>(prefixLength % 8);
    for (unsigned int i = prefixLength / 8 + 1; i < 16; ++i)
        baddr.sin6_addr.s6_addr[i] = 0x00u;
    return boost::static_pointer_cast<IPv6Address>(
        Address::create((const sockaddr *)&baddr, sizeof(sockaddr_in6)));
}

IPv6Address::ptr
IPv6Address::createSubnetMask(unsigned int prefixLength)
{
    MORDOR_ASSERT(prefixLength <= 128);
    sockaddr_in6 subnet;
    memset(&subnet, 0, sizeof(sockaddr_in6));
    subnet.sin6_family = AF_INET6;
    subnet.sin6_addr.s6_addr[prefixLength / 8] =
        ~createMask<unsigned char>(prefixLength % 8);
    for (unsigned int i = 0; i < prefixLength / 8; ++i)
        subnet.sin6_addr.s6_addr[i] = 0xffu;
    return boost::static_pointer_cast<IPv6Address>(
        Address::create((const sockaddr *)&subnet, sizeof(sockaddr_in6)));
}

std::ostream &
IPv6Address::insert(std::ostream &os) const
{
    std::ios_base::fmtflags flags = os.setf(std::ios_base::hex, std::ios_base::basefield);
    bool includePort = !os.iword(g_iosPortIndex);
    if (includePort)
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
        os << (int)byteswapOnLittleEndian(addr[i]);
    }
    if (!usedZeros && addr[7] == 0)
        os << "::";

    if (includePort)
        os << "]:" << std::dec << (int)byteswapOnLittleEndian(sin.sin6_port);
    os.setf(flags, std::ios_base::basefield);
    return os;
}

#ifndef WINDOWS
UnixAddress::UnixAddress(const std::string &path)
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

UnknownAddress::UnknownAddress(int family)
{
    sa.sa_family = family;
}

std::ostream &operator <<(std::ostream &os, const Address &addr)
{
    return addr.insert(os);
}

bool
operator <(const Address::ptr &lhs, const Address::ptr &rhs)
{
    if (!lhs || !rhs)
        return rhs;
    return *lhs < *rhs;
}

std::ostream &includePort(std::ostream &os)
{
    os.iword(g_iosPortIndex) = 0;
    return os;
}

std::ostream &excludePort(std::ostream &os)
{
    os.iword(g_iosPortIndex) = 1;
    return os;
}

}
