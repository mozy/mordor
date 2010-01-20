#ifndef __MORDOR_HTTP_PROXY_H__
#define __MORDOR_HTTP_PROXY_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "broker.h"

namespace Mordor {
namespace HTTP {

class ProxyConnectionBroker : public ConnectionBroker
{
public:
    ProxyConnectionBroker(ConnectionBroker::ptr parent,
        boost::function<URI (const URI &)> proxyForURIDg = NULL);

    std::pair<ClientConnection::ptr, bool>
        getConnection(const URI &uri, bool forceNewConnection = false);

private:
    ConnectionBroker::ptr m_parent;
    boost::function<URI (const URI &)> m_dg;
};

class ProxyStreamBroker : public StreamBrokerFilter
{
public:
    ProxyStreamBroker(StreamBroker::ptr parent,
        RequestBroker::ptr requestBroker,
        boost::function<URI (const URI &)> proxyForURIDg = NULL);

    Stream::ptr getStream(const URI &uri);

private:
    RequestBroker::ptr m_requestBroker;
    boost::function<URI (const URI &)> m_dg;
};

}}

#endif
