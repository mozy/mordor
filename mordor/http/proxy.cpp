// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "proxy.h"

namespace Mordor {
namespace HTTP {

ProxyConnectionBroker::ProxyConnectionBroker(ConnectionBroker::ptr parent,
    boost::function<URI (const URI &)> proxyForURIDg)
    : m_parent(parent),
      m_dg(proxyForURIDg)
{}

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
    boost::function<URI (const URI &)> proxyForURIDg,
    RequestBroker::ptr requestBroker)
    : m_parent(parent),
      m_requestBroker(requestBroker),
      m_dg(proxyForURIDg)
{}

Stream::ptr
ProxyStreamBroker::getStream(const Mordor::URI &uri)
{
    URI proxy = m_dg(uri);
    if (!proxy.isDefined() || !(proxy.schemeDefined() && proxy.scheme() == "https"))
        return m_parent->getStream(uri);
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
