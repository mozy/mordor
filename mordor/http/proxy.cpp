// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "proxy.h"

#include "mordor/config.h"
#include "mordor/http/client.h"
#include "mordor/socket.h"

#ifdef WINDOWS
#include "mordor/runtime_linking.h"
#elif defined (OSX)
#include <SystemConfiguration/SystemConfiguration.h>
#include "mordor/util.h"
#endif

namespace Mordor {
namespace HTTP {

static ConfigVar<std::string>::ptr g_httpProxy =
    Config::lookup("http.proxy", std::string(),
    "HTTP Proxy Server");

std::vector<URI> proxyFromConfig(const URI &uri)
{
    return proxyFromList(uri, g_httpProxy->val());
}

std::vector<URI> proxyFromList(const URI &uri, const std::string &proxy,
    const std::string &bypassList)
{
    MORDOR_ASSERT(uri.schemeDefined());
    MORDOR_ASSERT(uri.scheme() == "http" || uri.scheme() == "https");
    MORDOR_ASSERT(uri.authority.hostDefined());

    std::vector<URI> result;
    std::string proxyLocal = proxy;
    std::string bypassListLocal = bypassList;
    if (bypassListLocal.empty()) {
        size_t bang = proxyLocal.find('!');
        if (bang != std::string::npos) {
            bypassListLocal = proxyLocal.substr(bang + 1);
            proxyLocal = proxyLocal.substr(0, bang);
        }
    }

    std::string host = uri.authority.host();
    std::vector<std::string> list = split(bypassListLocal, "; \t\r\n");
    for (std::vector<std::string>::iterator it = list.begin();
        it != list.end();
        ++it) {
        const std::string &proxy = *it;
        if (proxy.empty())
            continue;
        if (proxy == "<local>" && host.find('.') == std::string::npos)
            return result;
        if (proxy[0] == '*' && host.size() >= proxy.size() - 1 &&
            stricmp(proxy.c_str() + 1, host.c_str() + host.size() - proxy.size() - 1) == 0)
            return result;
        else if (stricmp(host.c_str(), proxy.c_str()) == 0)
            return result;
    }

    list = split(proxyLocal, "; \t\r\n");

    for (std::vector<std::string>::iterator it = list.begin();
        it != list.end();
        ++it) {
        std::string curProxy = *it;
        std::string forScheme;
        size_t equals = curProxy.find('=');
        if (equals != std::string::npos) {
            forScheme = curProxy.substr(0, equals);
            curProxy = curProxy.substr(equals + 1);
        }
        if (!forScheme.empty() && stricmp(forScheme.c_str(), uri.scheme().c_str()) != 0)
            continue;
        equals = curProxy.find("//");
        if (equals == std::string::npos)
            curProxy = "//" + curProxy;
        URI proxyUri;
        try {
            proxyUri = curProxy;
        } catch (std::invalid_argument &) {
            continue;
        }
        if (!proxyUri.authority.hostDefined())
            continue;
        if (proxyUri.schemeDefined()) {
            std::string scheme = proxyUri.scheme();
            std::transform(scheme.begin(), scheme.end(), scheme.begin(), &tolower);
            if (scheme != "http" && scheme != "https")
                continue;
            if (forScheme.empty() && scheme != uri.scheme())
                continue;
            proxyUri.scheme(scheme);
        } else {
            proxyUri.scheme(uri.scheme());
        }
        proxyUri.path.segments.clear();
        proxyUri.path.type = URI::Path::RELATIVE;
        proxyUri.authority.userinfoDefined(false);
        proxyUri.queryDefined(false);
        proxyUri.fragmentDefined(false);
        result.push_back(proxyUri);
    }
    return result;
}

#ifdef WINDOWS
std::vector<URI> autoDetectProxy(const URI &uri, const std::string &pacScript,
    const std::string &userAgent)
{
    WINHTTP_PROXY_INFO proxyInfo;
    HINTERNET hHttpSession = NULL;
    WINHTTP_AUTOPROXY_OPTIONS options;

    memset(&options, 0, sizeof(WINHTTP_AUTOPROXY_OPTIONS));
    memset(&proxyInfo, 0, sizeof(WINHTTP_PROXY_INFO));

    hHttpSession = pWinHttpOpen(toUtf16(userAgent).c_str(),
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hHttpSession) {
        if (GetLastError() == ERROR_CALL_NOT_IMPLEMENTED)
            return std::vector<URI>();
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WinHttpOpen");
    }

    std::wstring pacScriptW = toUtf16(pacScript);
    if (!pacScriptW.empty()) {
        options.dwFlags = WINHTTP_AUTOPROXY_CONFIG_URL;
        options.lpszAutoConfigUrl = pacScriptW.c_str();
    } else {
        options.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
        options.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP |
            WINHTTP_AUTO_DETECT_TYPE_DNS_A;
    }

    options.fAutoLogonIfChallenged = TRUE;
    std::vector<URI> result;
    if (pWinHttpGetProxyForUrl(hHttpSession, toUtf16(uri.toString()).c_str(),
        &options, &proxyInfo)) {
        try {
            std::string proxy, bypassList;
            result = proxyFromList(uri, toUtf8(proxyInfo.lpszProxy),
                toUtf8(proxyInfo.lpszProxyBypass));
        } catch (...) {
            if (proxyInfo.lpszProxy)
                GlobalFree(proxyInfo.lpszProxy);
            if (proxyInfo.lpszProxyBypass)
                GlobalFree(proxyInfo.lpszProxyBypass);
            pWinHttpCloseHandle(hHttpSession);
            throw;
        }
        if (proxyInfo.lpszProxy)
            GlobalFree(proxyInfo.lpszProxy);
        if (proxyInfo.lpszProxyBypass)
            GlobalFree(proxyInfo.lpszProxyBypass);
    }
    pWinHttpCloseHandle(hHttpSession);
    return result;
}

std::vector<URI> proxyFromMachineDefault(const URI &uri)
{
    WINHTTP_PROXY_INFO proxyInfo;
    if (!pWinHttpGetDefaultProxyConfiguration(&proxyInfo))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WinHttpGetDefaultProxyConfiguration");
    std::vector<URI> result;
    try {
        std::string proxy, bypassList;
        result = proxyFromList(uri, toUtf8(proxyInfo.lpszProxy),
            toUtf8(proxyInfo.lpszProxyBypass));
    } catch (...) {
        if (proxyInfo.lpszProxy)
            GlobalFree(proxyInfo.lpszProxy);
        if (proxyInfo.lpszProxyBypass)
            GlobalFree(proxyInfo.lpszProxyBypass);
        throw;
    }
    if (proxyInfo.lpszProxy)
        GlobalFree(proxyInfo.lpszProxy);
    if (proxyInfo.lpszProxyBypass)
        GlobalFree(proxyInfo.lpszProxyBypass);
    return result;
}

ProxySettings
getUserProxySettings()
{
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG proxyConfig;
    ProxySettings result;
    if (!pWinHttpGetIEProxyConfigForCurrentUser(&proxyConfig))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WinHttpGetIEProxyConfigForCurrentUser");
    try {
        result.autoDetect = !!proxyConfig.fAutoDetect;
        if (proxyConfig.lpszAutoConfigUrl)
            result.pacScript = toUtf8(proxyConfig.lpszAutoConfigUrl);
        if (proxyConfig.lpszProxy)
            result.proxy = toUtf8(proxyConfig.lpszProxy);
        if (proxyConfig.lpszProxyBypass)
            result.bypassList = toUtf8(proxyConfig.lpszProxyBypass);
    } catch (...) {
        if (proxyConfig.lpszAutoConfigUrl)
            GlobalFree(proxyConfig.lpszAutoConfigUrl);
        if (proxyConfig.lpszProxy)
            GlobalFree(proxyConfig.lpszProxy);
        if (proxyConfig.lpszProxyBypass)
            GlobalFree(proxyConfig.lpszProxyBypass);
        throw;
    }
    if (proxyConfig.lpszAutoConfigUrl)
        GlobalFree(proxyConfig.lpszAutoConfigUrl);
    if (proxyConfig.lpszProxy)
        GlobalFree(proxyConfig.lpszProxy);
    if (proxyConfig.lpszProxyBypass)
        GlobalFree(proxyConfig.lpszProxyBypass);
    return result;
}

std::vector<URI> proxyFromUserSettings(const URI &uri)
{
    ProxySettings settings = getUserProxySettings();
    if (settings.autoDetect)
        return autoDetectProxy(uri, settings.pacScript);
    return proxyFromList(uri, settings.proxy, settings.bypassList);
}
#elif defined(OSX)
std::vector<URI> proxyFromSystemConfiguration(const URI &uri)
{
    std::vector<URI> result;
    MORDOR_ASSERT(uri.schemeDefined());
    MORDOR_ASSERT(uri.scheme() == "http" || uri.scheme() == "https");

    const void *enableKey;
    const void *proxyKey;
    const void *portKey;
    if (uri.scheme() == "http") {
        enableKey = kSCPropNetProxiesHTTPEnable;
        proxyKey = kSCPropNetProxiesHTTPProxy;
        portKey = kSCPropNetProxiesHTTPPort;
    } else {
        enableKey = kSCPropNetProxiesHTTPSEnable;
        proxyKey = kSCPropNetProxiesHTTPSProxy;
        portKey = kSCPropNetProxiesHTTPSPort;
    }

    ScopedCFRef<CFDictionaryRef> proxyDict = SCDynamicStoreCopyProxies(NULL);
    if (!proxyDict)
        return result;
    CFNumberRef excludeSimpleHostnamesRef =
        (CFNumberRef)CFDictionaryGetValue(proxyDict, kSCPropNetProxiesExcludeSimpleHostnames);
    std::string host = uri.authority.host();
    int excludeSimpleHostnames;
    if (excludeSimpleHostnamesRef &&
        CFGetTypeID(excludeSimpleHostnamesRef) == CFNumberGetTypeID() &&
        CFNumberGetValue(excludeSimpleHostnamesRef, kCFNumberIntType, &excludeSimpleHostnames) &&
        excludeSimpleHostnames &&
        host.find('.') == std::string::npos)
        return result;
    CFArrayRef bypassList = (CFArrayRef)CFDictionaryGetValue(proxyDict, kSCPropNetProxiesExceptionsList);
    if (bypassList && CFGetTypeID(bypassList) == CFArrayGetTypeID()) {
        for (CFIndex i = 0; i < CFArrayGetCount(bypassList); ++i) {
            CFStringRef bypassRef = (CFStringRef)CFArrayGetValueAtIndex(bypassList, i);
            if (!bypassRef || CFGetTypeID(bypassRef) != CFStringGetTypeID())
                continue;
            std::string bypass = toUtf8(bypassRef);
            if (bypass.empty())
                continue;
            if (bypass[0] == '*' && host.size() >= bypass.size() - 1 &&
                stricmp(bypass.c_str() + 1, host.c_str() + host.size() - bypass.size() - 1) == 0)
                return URI();
            else if (stricmp(host.c_str(), bypass.c_str()) == 0)
                return URI();
        }
    }
    CFNumberRef enabledRef = (CFNumberRef)CFDictionaryGetValue(proxyDict, enableKey);
    if (!enabledRef || CFGetTypeID(enabledRef) != CFNumberGetTypeID())
        return result;
    int enabled = 0;
    if (!CFNumberGetValue(enabledRef, kCFNumberIntType, &enabled) || enabled == 0)
        return result;
    CFStringRef proxyHostRef = (CFStringRef)CFDictionaryGetValue(proxyDict, proxyKey);
    if (!proxyHostRef || CFGetTypeID(proxyHostRef) != CFStringGetTypeID())
        return result;
    CFNumberRef proxyPortRef = (CFNumberRef)CFDictionaryGetValue(proxyDict, portKey);
    int port = 0;
    if (proxyPortRef && CFGetTypeID(proxyPortRef) != CFNumberGetTypeID())
        CFNumberGetValue(proxyPortRef, kCFNumberIntType, &port);

    URI proxy;
    proxy.authority.host(toUtf8(proxyHostRef));
    proxy.scheme(uri.scheme());
    if (port != 0)
        proxy.authority.port(port);
    result.push_back(proxy);
    return result;
}
#endif

ProxyConnectionBroker::ProxyConnectionBroker(ConnectionBroker::ptr parent,
    boost::function<std::vector<URI> (const URI &)> proxyForURIDg)
    : m_parent(parent),
      m_dg(proxyForURIDg)
{
    MORDOR_ASSERT(m_dg);
}

std::pair<boost::shared_ptr<ClientConnection>, bool>
ProxyConnectionBroker::getConnection(const URI &uri, bool forceNewConnection)
{
    std::vector<URI> proxies = m_dg(uri);
    if (proxies.empty())
        return m_parent->getConnection(uri, forceNewConnection);
    std::vector<URI>::iterator it = proxies.begin();
    while (true) {
        try {
            URI &proxy = *it;
            if (!proxy.isDefined() || !(proxy.schemeDefined() && proxy.scheme() == "http"))
                return m_parent->getConnection(uri, forceNewConnection);
            return std::make_pair(m_parent->getConnection(proxy,
                forceNewConnection).first, true);
        } catch (SocketException &) {
            if (++it == proxies.end())
                throw;
        }
    }
}

ProxyStreamBroker::ProxyStreamBroker(StreamBroker::ptr parent,
    RequestBroker::ptr requestBroker,
    boost::function<std::vector<URI> (const URI &)> proxyForURIDg)
    : StreamBrokerFilter(parent),
      m_requestBroker(requestBroker),
      m_dg(proxyForURIDg)
{
    MORDOR_ASSERT(m_dg);
}

boost::shared_ptr<Stream>
ProxyStreamBroker::getStream(const Mordor::URI &uri)
{
    std::vector<URI> proxies = m_dg(uri);
    if (proxies.empty())
        return parent()->getStream(uri);
    std::vector<URI>::iterator it = proxies.begin();
    while (true) {
        try {
            URI &proxy = *it;
            // Don't use a proxy if they didn't provide one, the provided one has no
            // scheme, the proxy's scheme is not https, or we're trying to get a
            // connection to the proxy itself anyway
            if (!proxy.isDefined() || !proxy.schemeDefined() ||
                proxy.scheme() != "https" ||
                (uri.scheme() == "http" && uri.path.isEmpty() &&
                uri.authority == proxy.authority))
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
            URI &requestUri = requestHeaders.requestLine.uri;
            requestUri.scheme("http");
            requestUri.authority = proxy.authority;
            requestUri.path.type = URI::Path::ABSOLUTE;
            requestUri.path.segments.push_back(os.str());
            requestHeaders.request.host = os.str();
            requestHeaders.general.connection.insert("Proxy-Connection");
            requestHeaders.general.proxyConnection.insert("Keep-Alive");
            ClientRequest::ptr request = m_requestBroker->request(requestHeaders, true);
            if (request->response().status.status == HTTP::OK) {
                return request->stream();
            } else {
                if (++it == proxies.end())
                    MORDOR_THROW_EXCEPTION(InvalidResponseException("Proxy connection failed",
                        request));
            }
        } catch (SocketException &) {
            if (++it == proxies.end())
                throw;
        } catch (HTTP::Exception &) {
            if (++it == proxies.end())
                throw;
        } catch (UnexpectedEofException &) {
            if (++it == proxies.end())
                throw;
        }
    }
}

}}
