#ifndef __MORDOR_HTTP_PROXY_H__
#define __MORDOR_HTTP_PROXY_H__
// Copyright (c) 2009 - Decho Corp.

#include "broker.h"

namespace Mordor {
namespace HTTP {

class ProxyConnectionBroker : public ConnectionBroker
{
public:
    ProxyConnectionBroker(ConnectionBroker::ptr parent,
        boost::function<URI (const URI &)> proxyForURIDg);

    std::pair<ClientConnection::ptr, bool>
        getConnection(const URI &uri, bool forceNewConnection = false);

private:
    ConnectionBroker::ptr m_parent;
    boost::function<URI (const URI &)> m_dg;
};

class ProxyStreamBroker : public StreamBroker
{
public:
    ProxyStreamBroker(StreamBroker::ptr parent,
        boost::function<URI (const URI &)> proxyForURIDg,
        RequestBroker::ptr requestBroker);

    Stream::ptr getStream(const URI &uri);
    void cancelPending() { m_parent->cancelPending(); }

private:
    StreamBroker::ptr m_parent;
    RequestBroker::ptr m_requestBroker;
    boost::function<URI (const URI &)> m_dg;
};

}}

#endif
