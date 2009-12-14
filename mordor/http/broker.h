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
    StreamBrokerFilter(StreamBroker::ptr parent)
        : m_parent(parent)
    {}

    StreamBroker::ptr parent() const { return m_parent; }
    void parent(StreamBroker::ptr parent) { m_parent = parent; }

    void cancelPending() { m_parent->cancelPending(); }

private:
    StreamBroker::ptr m_parent;
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

public:
    virtual ~RequestBroker() {}

    virtual ClientRequest::ptr request(Request &requestHeaders,
        bool forceNewConnection = false) = 0;

    /// @return If the request should be re-tried
    /// @pre request->hasRequestBody()
    virtual bool checkResponse(ClientRequest::ptr request,
        Request &requestHeaders) { return false; }
};

class RequestBrokerFilter : public RequestBroker
{
public:
    RequestBrokerFilter(RequestBroker::ptr parent)
        : m_parent(parent)
    {}

    ClientRequest::ptr request(Request &requestHeaders,
        bool forceNewConnection = false)
    { return m_parent->request(requestHeaders, forceNewConnection); }

    bool checkResponse(ClientRequest::ptr request, Request &requestHeaders)
    { return m_parent->checkResponse(request, requestHeaders); }

private:
    RequestBroker::ptr m_parent;
};

class BaseRequestBroker : public RequestBroker
{
public:
    BaseRequestBroker(ConnectionBroker::ptr connectionBroker)
        : m_connectionBroker(connectionBroker)
    {}

    ClientRequest::ptr request(Request &requestHeaders,
        bool forceNewConnection = false);

private:
    ConnectionBroker::ptr m_connectionBroker;
};

struct CircularRedirectException : Exception
{
    CircularRedirectException(const URI &uri)
        : m_uri(uri)
    {}

    URI uri() { return m_uri; }

private:
    URI m_uri;
};

class RedirectRequestBroker : public RequestBrokerFilter
{
public:
    RedirectRequestBroker(RequestBroker::ptr parent)
        : RequestBrokerFilter(parent)
    {}

    ClientRequest::ptr request(Request &requestHeaders,
        bool forceNewConnection = false);
    bool checkResponse(ClientRequest::ptr request, Request &requestHeaders);
};

RequestBroker::ptr defaultRequestBroker(IOManager *ioManager = NULL,
                                        Scheduler *scheduler = NULL,
                                        ConnectionBroker::ptr &connBroker =
                                        ConnectionBroker::ptr());

}}

#endif
