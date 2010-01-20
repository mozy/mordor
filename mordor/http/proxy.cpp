// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "proxy.h"

#include "mordor/config.h"

namespace Mordor {
namespace HTTP {

static ConfigVar<std::string>::ptr g_httpProxy =
    Config::lookup("http.proxy", std::string(),
    "HTTP Proxy Server");

static URI defaultProxyCallback(const URI &uri)
{
    MORDOR_ASSERT(uri.schemeDefined());
    MORDOR_ASSERT(uri.scheme() == "http" || uri.scheme() == "https");
    try {
        URI result(g_httpProxy->val());
        if (result.authority.hostDefined())
            result.scheme(uri.scheme());
        result.path.segments.clear();
        result.path.type = URI::Path::RELATIVE;
        result.authority.userinfoDefined(false);
        // Avoid infinite recursion
        if (result == uri)
            return URI();
        return result;
    } catch (std::invalid_argument &)
    {
        return URI();
    }
}

ProxyConnectionBroker::ProxyConnectionBroker(ConnectionBroker::ptr parent,
    boost::function<URI (const URI &)> proxyForURIDg)
    : m_parent(parent),
      m_dg(proxyForURIDg)
{
    if (!m_dg)
        m_dg = &defaultProxyCallback;
}

std::pair<ClientConnection::ptr, bool>
ProxyConnectionBroker::getConnection(const URI &uri, bool forceNewConnection)
{
    URI proxy = m_dg(uri);
    if (!proxy.isDefined() || !(proxy.schemeDefined() && proxy.scheme() == "http"))
        return m_parent->getConnection(uri, forceNewConnection);
    return std::make_pair(m_parent->getConnection(proxy,
        forceNewConnection).first, true);
}

ProxyStreamBroker::ProxyStreamBroker(StreamBroker::ptr parent,
    RequestBroker::ptr requestBroker,
    boost::function<URI (const URI &)> proxyForURIDg)
    : StreamBrokerFilter(parent),
      m_requestBroker(requestBroker),
      m_dg(proxyForURIDg)
{
    if (!m_dg)
        m_dg = &defaultProxyCallback;
}

Stream::ptr
ProxyStreamBroker::getStream(const Mordor::URI &uri)
{
    URI proxy = m_dg(uri);
    if (!proxy.isDefined() || !(proxy.schemeDefined() && proxy.scheme() == "https"))
        return parent()->getStream(uri);
    std::ostringstream os;
    if (!uri.authority.hostDefined())
        MORDOR_THROW_EXCEPTION(std::invalid_argument("No host defined"));
    os << uri.authority.host() << ':';
    if (uri.authority.portDefined())
        os << uri.authority.port();
    else if (uri.scheme() == "http")
        os << "80";
    else if (uri.scheme() == "https")
        os << "443";
    else
        // TODO: can this be looked up using the system? (getaddrinfo)
        MORDOR_THROW_EXCEPTION(std::invalid_argument("Unknown protocol for proxying connection"));
    Request requestHeaders;
    requestHeaders.requestLine.method = CONNECT;
    requestHeaders.requestLine.uri = os.str();
    os.str("");
    os << proxy.authority.host();
    if (proxy.authority.portDefined())
        os << ':' << proxy.authority.port();
    requestHeaders.request.host = os.str();
    requestHeaders.general.connection.insert("Proxy-Connection");
    requestHeaders.general.proxyConnection.insert("Keep-Alive");
    ClientRequest::ptr request = m_requestBroker->request(requestHeaders, true);
    if (request->response().status.status == HTTP::OK) {
        return request->stream();
    } else {
        MORDOR_THROW_EXCEPTION(InvalidResponseException("Proxy connection failed",
            request));
    }
}

}}
