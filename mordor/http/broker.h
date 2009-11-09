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

    virtual std::pair<Stream::ptr, bool> getStream(const URI &uri) = 0;
    virtual void cancelPending() {}
};

class SocketStreamBroker : public StreamBroker
{
public:
    SocketStreamBroker(IOManager *ioManager = NULL, Scheduler *scheduler = NULL,
        SSL_CTX *sslCtx = NULL, bool verifySslCert = false,
        bool verifySslCertHost = false)
        : connectTimeout(~0ull),
          sendTimeout(~0ull),
          receiveTimeout(~0ull),
          m_cancelled(false),
          m_ioManager(ioManager),
          m_scheduler(scheduler),
          m_sslCtx(sslCtx),
          m_verifySslCert(verifySslCert),
          m_verifySslCertHost(verifySslCertHost)
    {}

    std::pair<Stream::ptr, bool> getStream(const URI &uri);
    void cancelPending();

    unsigned long long connectTimeout, sendTimeout, receiveTimeout;

private:
    boost::mutex m_mutex;
    bool m_cancelled;
    std::list<Socket::ptr> m_pending;
    IOManager *m_ioManager;
    Scheduler *m_scheduler;
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

    typedef std::list<std::pair<ClientConnection::ptr, bool> > ConnectionList;
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

}}

#endif
