#ifndef __MORDOR_HTTP_PROXY_H__
#define __MORDOR_HTTP_PROXY_H__
// Copyright (c) 2009 - Decho Corporation

#include "mordor/uri.h"

#ifdef WINDOWS
#include <winhttp.h>
#elif defined (OSX)
#include <SystemConfiguration/SystemConfiguration.h>
#include "mordor/util.h"
#endif

namespace Mordor {

class Stream;

namespace HTTP {

class RequestBroker;

// Functions to use for the proxyForURIDg

// The default; if you've done Config::loadFromEnvironment, this will use the
// HTTP_PROXY environment variable (passing it to proxyFromList)
std::vector<URI> proxyFromConfig(const URI &uri);
// This parses proxy and bypassList according to the WINHTTP_PROXY_INFO
// structure; additionally if bypassList is blank, it will look for a !
// in the proxy, and use that to separate the proxy from the
// bypassList
std::vector<URI> proxyFromList(const URI &uri, const std::string &proxy,
    const std::string &bypassList = std::string());

#ifdef WINDOWS
std::vector<URI> proxyFromMachineDefault(const URI &uri);

struct ProxySettings
{
    bool autoDetect;
    std::string pacScript;
    std::string proxy;
    std::string bypassList;
};

ProxySettings getUserProxySettings();

class ProxyCache
{
public:
    ProxyCache(const std::string &userAgent = std::string());
    ~ProxyCache();

    std::vector<URI> proxyFromUserSettings(const URI &uri);
    std::vector<URI> autoDetectProxy(const URI &uri,
        const std::string &pacScript = std::string());

private:
    HINTERNET m_hHttpSession;
};
#elif defined (OSX)
class ProxyCache
{
public:
    ProxyCache(boost::shared_ptr<RequestBroker> requestBroker);

    std::vector<URI> proxyFromSystemConfiguration(const URI &uri);

private:
    ScopedCFRef<SCDynamicStoreRef> m_dynamicStore;
	boost::shared_ptr<RequestBroker> m_requestBroker;
    std::map<URI, ScopedCFRef<CFStringRef> > m_cachedScripts;
};
#endif

/// Establish a tunnel via an HTTPS proxy
boost::shared_ptr<Stream>
tunnel(boost::shared_ptr<RequestBroker> requestBroker, const URI &proxy,
    const URI &target);

}}

#endif
