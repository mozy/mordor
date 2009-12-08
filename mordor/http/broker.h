#ifndef __HTTP_BROKER_H__
#define __HTTP_BROKER_H__
// Copyright (c) 2009 - Decho Corp.

#include "client.h"
#include "server.h"

namespace Mordor {
namespace HTTP {

class ConnectionBroker
{
public:
    typedef boost::shared_ptr<ConnectionBroker> ptr;

public:
    virtual std::pair<ClientConnection::ptr, bool>
        getConnection(const URI &uri, bool forceNewConnection = false) = 0;
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
