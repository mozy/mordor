#ifndef __HTTP_BROKER_H__
#define __HTTP_BROKER_H__
// Copyright (c) 2009 - Decho Corp.

#include <openssl/ssl.h>

#include "client.h"
#include "mordor/scheduler.h"
#include "server.h"

namespace Mordor {
namespace HTTP {

class StreamBroker
{
public:
    typedef boost::shared_ptr<StreamBroker> ptr;
    typedef boost::weak_ptr<StreamBroker> weak_ptr;

public:
    virtual ~StreamBroker() {}

    virtual Stream::ptr getStream(const URI &uri) = 0;
    virtual void cancelPending() {}
};

class StreamBrokerFilter : public StreamBroker
{
public:
    typedef boost::shared_ptr<StreamBrokerFilter> ptr;

public:
    StreamBrokerFilter(StreamBroker::ptr parent,
        StreamBroker::weak_ptr weakParent = StreamBroker::weak_ptr())
        : m_parent(parent),
          m_weakParent(weakParent)
    {}

    StreamBroker::ptr parent();
    void parent(StreamBroker::ptr parent)
    { m_parent = parent; m_weakParent.reset(); }
    void parent(StreamBroker::weak_ptr parent)
    { m_weakParent = parent; m_parent.reset(); }

    void cancelPending() { parent()->cancelPending(); }

private:
    StreamBroker::ptr m_parent;
    StreamBroker::weak_ptr m_weakParent;
};

class SocketStreamBroker : public StreamBroker
{
public:
    SocketStreamBroker(IOManager *ioManager = NULL, Scheduler *scheduler = NULL)
        : connectTimeout(~0ull),
          sendTimeout(~0ull),
          receiveTimeout(~0ull),
          m_cancelled(false),
          m_ioManager(ioManager),
          m_scheduler(scheduler)
    {}

    Stream::ptr getStream(const URI &uri);
    void cancelPending();

    unsigned long long connectTimeout, sendTimeout, receiveTimeout;

private:
    boost::mutex m_mutex;
    bool m_cancelled;
    std::list<Socket::ptr> m_pending;
    IOManager *m_ioManager;
    Scheduler *m_scheduler;
};

class SSLStreamBroker : public StreamBrokerFilter
{
public:
    SSLStreamBroker(StreamBroker::ptr parent,
        SSL_CTX *sslCtx = NULL, bool verifySslCert = false,
        bool verifySslCertHost = false)
        : StreamBrokerFilter(parent),
          m_sslCtx(sslCtx),
          m_verifySslCert(verifySslCert),
          m_verifySslCertHost(verifySslCertHost)
    {}

    Stream::ptr getStream(const URI &uri);

private:
    SSL_CTX *m_sslCtx;
    bool m_verifySslCert, m_verifySslCertHost;
};

class ConnectionBroker
{
public:
    typedef boost::shared_ptr<ConnectionBroker> ptr;

public:
    virtual ~ConnectionBroker() {}

    virtual std::pair<ClientConnection::ptr, bool>
        getConnection(const URI &uri, bool forceNewConnection = false) = 0;
};

class ConnectionCache : public ConnectionBroker
{
public:
    ConnectionCache(StreamBroker::ptr streamBroker,
        size_t connectionsPerHost = 1)
        : m_streamBroker(streamBroker),
          m_connectionsPerHost(connectionsPerHost)
    {}

    std::pair<ClientConnection::ptr, bool>
        getConnection(const URI &uri, bool forceNewConnection = false);

    void closeConnections();

private:
    FiberMutex m_mutex;
    StreamBroker::ptr m_streamBroker;
    size_t m_connectionsPerHost;

    typedef std::list<ClientConnection::ptr> ConnectionList;
    std::map<URI, std::pair<ConnectionList, boost::shared_ptr<FiberCondition> > > m_conns;
};

class MockConnectionBroker : public ConnectionBroker
{
private:
    typedef std::map<URI,
        std::pair<ClientConnection::ptr, ServerConnection::ptr> >
        ConnectionCache;
public:
    MockConnectionBroker(boost::function<void (const URI &uri, ServerRequest::ptr)> dg)
        : m_dg(dg)
    {}

    std::pair<ClientConnection::ptr, bool>
        getConnection(const URI &uri, bool forceNewConnection = false);

private:
    boost::function<void (const URI &uri, ServerRequest::ptr)> m_dg;
    ConnectionCache m_conns;
};

class RequestBroker
{
public:
    typedef boost::shared_ptr<RequestBroker> ptr;
    typedef boost::weak_ptr<RequestBroker> weak_ptr;

public:
    virtual ~RequestBroker() {}

    virtual ClientRequest::ptr request(Request &requestHeaders,
        bool forceNewConnection = false,
        boost::function<void (ClientRequest::ptr)> bodyDg = NULL) = 0;
};

class RequestBrokerFilter : public RequestBroker
{
public:
    RequestBrokerFilter(RequestBroker::ptr parent,
        RequestBroker::weak_ptr weakParent = RequestBroker::weak_ptr())
        : m_parent(parent),
          m_weakParent(weakParent)
    {}

    RequestBroker::ptr parent();

    ClientRequest::ptr request(Request &requestHeaders,
        bool forceNewConnection = false,
        boost::function<void (ClientRequest::ptr)> bodyDg = NULL) = 0;

private:
    RequestBroker::ptr m_parent;
    RequestBroker::weak_ptr m_weakParent;
};

class BaseRequestBroker : public RequestBroker
{
public:
    BaseRequestBroker(ConnectionBroker::ptr connectionBroker)
        : m_connectionBroker(connectionBroker)
    {}

    ClientRequest::ptr request(Request &requestHeaders,
        bool forceNewConnection = false,
        boost::function<void (ClientRequest::ptr)> bodyDg = NULL);

private:
    ConnectionBroker::ptr m_connectionBroker;
};

struct CircularRedirectException : Exception
{
    CircularRedirectException(const URI &uri)
        : m_uri(uri)
    {}
    ~CircularRedirectException() throw() {}

    URI uri() { return m_uri; }

private:
    URI m_uri;
};

class RedirectRequestBroker : public RequestBrokerFilter
{
public:
    RedirectRequestBroker(RequestBroker::ptr parent, size_t maxRedirects = 70)
        : RequestBrokerFilter(parent),
          m_maxRedirects(maxRedirects),
          m_handle301(true),
          m_handle302(true),
          m_handle307(true)
    {}

    void handlePermanentRedirect(bool handle) { m_handle301 = handle; }
    void handleFound(bool handle) { m_handle302 = handle; }
    void handleTemporaryRedirect(bool handle) { m_handle307 = handle; }

    ClientRequest::ptr request(Request &requestHeaders,
        bool forceNewConnection = false,
        boost::function<void (ClientRequest::ptr)> bodyDg = NULL);

private:
    size_t m_maxRedirects;
    bool m_handle301, m_handle302, m_handle307;
};

class UserAgentRequestBroker : public RequestBrokerFilter
{
public:
    UserAgentRequestBroker(RequestBroker::ptr parent,
        const ProductAndCommentList &userAgent)
        : RequestBrokerFilter(parent),
          m_userAgent(userAgent)
    {}

    ClientRequest::ptr request(Request &requestHeaders,
        bool forceNewConnection = false,
        boost::function<void (ClientRequest::ptr)> bodyDg = NULL);

private:
    ProductAndCommentList m_userAgent;
};

RequestBroker::ptr defaultRequestBroker(IOManager *ioManager = NULL,
                                        Scheduler *scheduler = NULL,
                                        ConnectionBroker::ptr *connBroker = NULL);

}}

#endif
