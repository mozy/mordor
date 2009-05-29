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
typedef WSABUF iovec;
#define SHUT_RD SD_RECEIVE
#define SHUT_WR SD_SEND
#define SHUT_RDWR SD_BOTH
#endif

struct Address;

class Socket : boost::noncopyable
{
private:
    Socket(IOManager *ioManager, int family, int type, int protocol, int initialize);
public:
    Socket(int family, int type, int protocol = 0);
    Socket(IOManager *ioManager, int family, int type, int protocol = 0);
    ~Socket();

    void bind(const Address *addr);
    void connect(const Address *to);
    void listen(int backlog = SOMAXCONN);

    Socket *accept();
    void accept(Socket *target);
    void shutdown(int how = SHUT_RDWR);
    void close();

    void getOption(int level, int option, void *result, size_t *len);
    void setOption(int level, int option, const void *value, size_t len);

    size_t send(const void *buf, size_t len, int flags = 0);
    size_t send(const iovec *bufs, size_t len, int flags = 0);
    size_t sendTo(const void *buf, size_t len, int flags, const Address *to);
    size_t sendTo(const iovec *bufs, size_t len, int flags, const Address *to);

    size_t receive(void *buf, size_t len, int flags = 0);
    size_t receive(iovec *bufs, size_t len, int flags = 0);
    size_t receiveFrom(void *buf, size_t len, int *flags, Address *from);
    size_t receiveFrom(iovec *bufs, size_t len, int *flags, Address *from);

    Address *emptyAddress();
    Address *remoteAddress();
    Address *localAddress();

    int family() { return m_family; }
    int type();
    int protocol() { return m_protocol; }

private:
    int m_sock, m_family, m_protocol;
    IOManager *m_ioManager;
    AsyncEvent m_sendEvent, m_receiveEvent;
};

struct Address
{
protected:
    Address(int type, int protocol = 0);
public:
    static std::vector<boost::shared_ptr<Address> >
        lookup(const std::string& host, int family = AF_UNSPEC,
            int type = 0, int protocol = 0);

    Socket *createSocket();
    Socket *createSocket(IOManager *ioManager);

    int family() const { return name()->sa_family; }
    int type() const { return m_type; }
    int protocol() const { return m_protocol; }
    virtual const sockaddr *name() const = 0;
    virtual sockaddr *name() = 0;
    virtual int nameLen() const = 0;

private:
    int m_type, m_protocol;
};

struct IPv4Address : public Address
{
public:
    IPv4Address(int type = 0, int protocol = 0);
    //IPv4Address(const std::string& addr, int type = 0, int protocol = 0);
    //IPv4Address(const std::string& addr, unsigned short port, int type = 0, int protocol = 0);

    unsigned short port() const { return ntohs(sin.sin_port); }
    void port(unsigned short p) { sin.sin_port = htons(p); }

    const sockaddr *name() const { return (sockaddr*)&sin; }
    sockaddr *name() { return (sockaddr*)&sin; }
    int nameLen() const { return sizeof(sockaddr_in); }
private:
    sockaddr_in sin;
};

struct IPv6Address : public Address
{
public:
    IPv6Address(int type = 0, int protocol = 0);
    //IPv6Address(const std::string& addr, int type = 0, int protocol = 0);
    //IPv6Address(const std::string& addr, unsigned short port, int type = 0, int protocol = 0);

    unsigned short port() const { return ntohs(sin.sin6_port); }
    void port(unsigned short p) { sin.sin6_port = htons(p); }

    const sockaddr *name() const { return (sockaddr*)&sin; }
    sockaddr *name() { return (sockaddr*)&sin; }
    int nameLen() const { return sizeof(sockaddr_in6); }
private:
    sockaddr_in6 sin;
};

struct UnknownAddress : public Address
{
public:
    UnknownAddress(int family, int type = 0, int protocol = 0);

    const sockaddr *name() const { return &sa; }
    sockaddr *name() { return &sa; }
    int nameLen() const { return sizeof(sockaddr); }
private:
    sockaddr sa;
};

#endif
