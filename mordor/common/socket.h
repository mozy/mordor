#ifndef __SOCKET_H__
#define __SOCKET_H__
// Copyright (c) 2009 - Decho Corp.

#include <vector>

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include "iomanager.h"
#include "version.h"

#ifdef WINDOWS
#include <ws2tcpip.h>
struct iovec
{
    union
    {
        WSABUF wsabuf;
        struct {
            u_long iov_len;
            void *iov_base;
        };
    };
};
#define SHUT_RD SD_RECEIVE
#define SHUT_WR SD_SEND
#define SHUT_RDWR SD_BOTH
typedef SOCKET socket_t;
#else
#include <sys/socket.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
typedef int socket_t;
#endif

struct Address;

class Socket : public boost::enable_shared_from_this<Socket>, boost::noncopyable
{
public:
    typedef boost::shared_ptr<Socket> ptr;
private:
    Socket(IOManager *ioManager, int family, int type, int protocol, int initialize);
public:
    Socket(int family, int type, int protocol = 0);
    Socket(IOManager &ioManager, int family, int type, int protocol = 0);
    ~Socket();

    unsigned long long receiveTimeout() { return m_receiveTimeout; }
    void receiveTimeout(unsigned long long us) { m_receiveTimeout = us; }
    unsigned long long sendTimeout() { return m_sendTimeout; }
    void sendTimeout(unsigned long long us) { m_sendTimeout = us; }

    void bind(const Address &addr);
    void bind(const boost::shared_ptr<Address> addr)
    { bind(*addr.get()); }
    void connect(const Address &to);
    void connect(const boost::shared_ptr<Address> addr)
    { connect(*addr.get()); }
    void listen(int backlog = SOMAXCONN);

    Socket::ptr accept();
    void accept(Socket &target);
    void shutdown(int how = SHUT_RDWR);
    void close();

    void getOption(int level, int option, void *result, size_t *len);
    void setOption(int level, int option, const void *value, size_t len);

    size_t send(const void *buf, size_t len, int flags = 0);
    size_t send(const iovec *bufs, size_t len, int flags = 0);
    size_t sendTo(const void *buf, size_t len, int flags, const Address &to);
    size_t sendTo(const void *buf, size_t len, int flags, const boost::shared_ptr<Address> to)
    { return sendTo(buf, len, flags, *to.get()); }
    size_t sendTo(const iovec *bufs, size_t len, int flags, const Address &to);
    size_t sendTo(const iovec *bufs, size_t len, int flags, const boost::shared_ptr<Address> to)
    { return sendTo(bufs, len, flags, *to.get()); }

    size_t receive(void *buf, size_t len, int flags = 0);
    size_t receive(iovec *bufs, size_t len, int flags = 0);
    size_t receiveFrom(void *buf, size_t len, int *flags, Address &from);
    size_t receiveFrom(iovec *bufs, size_t len, int *flags, Address &from);

    boost::shared_ptr<Address> emptyAddress();
    boost::shared_ptr<Address> remoteAddress();
    boost::shared_ptr<Address> localAddress();

    int family() { return m_family; }
    int type();
    int protocol() { return m_protocol; }

private:
#ifndef WINDOWS
    void cancelIo(IOManager::Event event, bool &cancelled);
#endif

private:
    socket_t m_sock;
    int m_family, m_protocol;
    IOManager *m_ioManager;
    unsigned long long m_receiveTimeout, m_sendTimeout;
#ifdef WINDOWS
    AsyncEvent m_sendEvent, m_receiveEvent;
#endif
};

struct Address
{
public:
    typedef boost::shared_ptr<Address> ptr;
protected:
    Address(int type, int protocol = 0);
public:
    virtual ~Address() {}

    static std::vector<ptr>
        lookup(const std::string& host, int family = AF_UNSPEC,
            int type = 0, int protocol = 0);

    Socket::ptr createSocket();
    Socket::ptr createSocket(IOManager &ioManager);

    int family() const { return name()->sa_family; }
    int type() const { return m_type; }
    int protocol() const { return m_protocol; }
    virtual const sockaddr *name() const = 0;
    virtual sockaddr *name() = 0;
    virtual socklen_t nameLen() const = 0;
    virtual std::ostream & insert(std::ostream &os) const;

private:
    int m_type, m_protocol;
};

struct IPAddress : public Address
{
protected:
    IPAddress(int type = 0, int protocol = 0);
public:
    virtual unsigned short port() const = 0;
    virtual void port(unsigned short p) = 0;
};

struct IPv4Address : public IPAddress
{
public:
    IPv4Address(int type = 0, int protocol = 0);
    //IPv4Address(const std::string& addr, int type = 0, int protocol = 0);
    //IPv4Address(const std::string& addr, unsigned short port, int type = 0, int protocol = 0);

    unsigned short port() const { return ntohs(sin.sin_port); }
    void port(unsigned short p) { sin.sin_port = htons(p); }

    const sockaddr *name() const { return (sockaddr*)&sin; }
    sockaddr *name() { return (sockaddr*)&sin; }
    socklen_t nameLen() const { return sizeof(sockaddr_in); }

    std::ostream & insert(std::ostream &os) const;
private:
    sockaddr_in sin;
};

struct IPv6Address : public IPAddress
{
public:
    IPv6Address(int type = 0, int protocol = 0);
    //IPv6Address(const std::string& addr, int type = 0, int protocol = 0);
    //IPv6Address(const std::string& addr, unsigned short port, int type = 0, int protocol = 0);

    unsigned short port() const { return ntohs(sin.sin6_port); }
    void port(unsigned short p) { sin.sin6_port = htons(p); }

    const sockaddr *name() const { return (sockaddr*)&sin; }
    sockaddr *name() { return (sockaddr*)&sin; }
    socklen_t nameLen() const { return sizeof(sockaddr_in6); }

    std::ostream & insert(std::ostream &os) const;
private:
    sockaddr_in6 sin;
};

struct UnknownAddress : public Address
{
public:
    UnknownAddress(int family, int type = 0, int protocol = 0);

    const sockaddr *name() const { return &sa; }
    sockaddr *name() { return &sa; }
    socklen_t nameLen() const { return sizeof(sockaddr); }
private:
    sockaddr sa;
};

std::ostream &operator <<(std::ostream &os, const Address &addr);

#endif
