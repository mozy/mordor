#ifndef __MORDOR_HTTP_PROXY_H__
#define __MORDOR_HTTP_PROXY_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "broker.h"

namespace Mordor {
namespace HTTP {

// Functions to use for the proxyForURIDg

// The default; if you've done Config::loadFromEnvironment, this will use the
// HTTP_PROXY environment variable (passing it to proxyFromList)
URI proxyFromConfig(const URI &uri);
// This parses proxy and bypassList according to the WINHTTP_PROXY_INFO
// structure; additionally if bypassList is blank, it will look for a !
// in the proxy, and use that to separate the proxy from the
// bypassList
URI proxyFromList(const URI &uri, const std::string &proxy,
    const std::string &bypassList = std::string());

#ifdef WINDOWS
URI autoDetectProxy(const URI &uri,
    const std::string &pacScript = std::string(),
    const std::string &userAgent = std::string());
URI proxyFromMachineDefault(const URI &uri);

struct ProxySettings
{
    bool autoDetect;
    std::string pacScript;
    std::string proxy;
    std::string bypassList;
};

ProxySettings getUserProxySettings();
URI proxyFromUserSettings(const URI &uri);
#endif

class ProxyConnectionBroker : public ConnectionBroker
{
public:
    typedef boost::shared_ptr<ProxyConnectionBroker> ptr;

public:
    ProxyConnectionBroker(ConnectionBroker::ptr parent,
        boost::function<URI (const URI &)> proxyForURIDg = &proxyFromConfig);

    void fallbackOnFailure(bool fallback) { m_fallbackOnFailure = fallback; }

    std::pair<boost::shared_ptr<ClientConnection>, bool>
        getConnection(const URI &uri, bool forceNewConnection = false);

private:
    ConnectionBroker::ptr m_parent;
    boost::function<URI (const URI &)> m_dg;
    bool m_fallbackOnFailure;
};

class ProxyStreamBroker : public StreamBrokerFilter
{
public:
    typedef boost::shared_ptr<ProxyStreamBroker> ptr;

public:
    ProxyStreamBroker(StreamBroker::ptr parent,
        RequestBroker::ptr requestBroker,
        boost::function<URI (const URI &)> proxyForURIDg = &proxyFromConfig);

    void fallbackOnFailure(bool fallback) { m_fallbackOnFailure = fallback; }

    boost::shared_ptr<Stream> getStream(const URI &uri);

private:
    RequestBroker::ptr m_requestBroker;
    boost::function<URI (const URI &)> m_dg;
    bool m_fallbackOnFailure;
};

}}

#endif
