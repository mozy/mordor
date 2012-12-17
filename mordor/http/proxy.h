#ifndef __MORDOR_HTTP_PROXY_H__
#define __MORDOR_HTTP_PROXY_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/uri.h"

#ifdef WINDOWS
#include <winhttp.h>
#elif defined (OSX)
#include <queue>
#include <boost/thread.hpp>
#include <SystemConfiguration/SystemConfiguration.h>
#include "mordor/util.h"
#include "mordor/http/broker.h"
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
std::vector<URI> proxyFromList(const URI &uri,
                               std::string proxy,
                               std::string bypassList = std::string());

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

    // Use the user's global proxy settings (which may specify a
    // proxy server, url to a configuration script or autodetection)
    std::vector<URI> proxyFromUserSettings(const URI &uri);

    // Determine the Proxy URIs, if any, to use to reach the specified uri
    // If no pacScript url is specified the system will attempt to autodetect one
    bool autoDetectProxy(const URI &uri, const std::string &pacScript,
                         std::vector<URI> &proxyList);

    // Depending on the settings the proxyFromUserSettings() method
    // may attempt to autodetect a proxy.
    // This autodetection can be slow, so the results are cached.
    // This method should be called if network configuration changes are
    // detected to force a new discovery
    static void resetDetectionResultCache();

private:
    HINTERNET m_hHttpSession;

    // The follow members are static so that the cache state can be shared by
    // all of the ProxyCache instances. This cache is invalided by calling
    // resetDetectionResultCache(), defined above.
    //
    // The lock held when accessing the static member variables.
    static boost::mutex s_cacheMutex;

    // True if WPAD failed.
    static bool s_failedAutoDetect;

    // The list of failed PAC file URLs.
    static std::set<std::string> s_invalidConfigURLs;
};

#elif defined (OSX)

class ProxyCache
{
public:
    ProxyCache(boost::shared_ptr<RequestBroker> requestBroker);
    ~ProxyCache();

    std::vector<URI> proxyFromSystemConfiguration(const URI &uri);

private:
    ScopedCFRef<SCDynamicStoreRef> m_dynamicStore;
        boost::shared_ptr<RequestBroker> m_requestBroker;
    std::map<URI, ScopedCFRef<CFStringRef> > m_cachedScripts;

    struct PacMessage {
        ScopedCFRef<CFStringRef> pacScript;
        ScopedCFRef<CFURLRef> targeturl;
        ScopedCFRef<CFArrayRef> result;
        bool processed;
    };

    boost::thread m_pacThread;
    boost::condition_variable m_pacCond;
    boost::mutex m_pacMut;
    std::queue<PacMessage*> m_pacQueue;
    bool m_pacThreadCancelled;

    void runPacWorker();

    std::vector<URI> proxyFromPacScript(CFURLRef cfurl, ScopedCFRef<CFURLRef> targeturl,
        RequestBroker::ptr requestBroker,
        std::map<URI, ScopedCFRef<CFStringRef> > &cachedScripts);
    std::vector<URI> proxyFromCFArray(CFArrayRef proxies, ScopedCFRef<CFURLRef> targeturl,
        RequestBroker::ptr requestBroker,
        std::map<URI, ScopedCFRef<CFStringRef> > &cachedScripts);
};
#endif

/// Establish a tunnel via an HTTPS proxy
///
/// @note This is *broken* if the ConnectionCache this RequestBroker is using
///       attempts to re-use the connection.  We can't set forceNewConnection,
///       because that would break NTLM authentication, and ConnectionCache
///       hasn't been improved yet to do allowPipelining instead of
///       forceNewConnection
boost::shared_ptr<Stream>
tunnel(boost::shared_ptr<RequestBroker> requestBroker, const URI &proxy,
    const URI &target);

}}

#endif
