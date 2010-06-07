// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "proxy.h"

#include "mordor/config.h"
#include "mordor/http/client.h"
#include "mordor/socket.h"

#ifdef WINDOWS
#include "mordor/runtime_linking.h"
#elif defined (OSX)
#include <CoreServices/CoreServices.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include "mordor/util.h"
#include "mordor/streams/file.h"
#include "mordor/streams/http.h"
#include "mordor/streams/limited.h"
#include "mordor/streams/memory.h"
#include "mordor/streams/transfer.h"
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

ProxyCache::ProxyCache(const std::string &userAgent)
{
    m_hHttpSession = pWinHttpOpen(toUtf16(userAgent).c_str(),
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!m_hHttpSession && GetLastError() != ERROR_CALL_NOT_IMPLEMENTED)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WinHttpOpen");
}

ProxyCache::~ProxyCache()
{
    if (m_hHttpSession)
        pWinHttpCloseHandle(m_hHttpSession);
}

std::vector<URI>
ProxyCache::autoDetectProxy(const URI &uri, const std::string &pacScript)
{
    std::vector<URI> result;
    if (!m_hHttpSession)
        return result;

    WINHTTP_PROXY_INFO proxyInfo;
    WINHTTP_AUTOPROXY_OPTIONS options;

    memset(&options, 0, sizeof(WINHTTP_AUTOPROXY_OPTIONS));
    memset(&proxyInfo, 0, sizeof(WINHTTP_PROXY_INFO));

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
    if (pWinHttpGetProxyForUrl(m_hHttpSession, toUtf16(uri.toString()).c_str(),
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
            throw;
        }
        if (proxyInfo.lpszProxy)
            GlobalFree(proxyInfo.lpszProxy);
        if (proxyInfo.lpszProxyBypass)
            GlobalFree(proxyInfo.lpszProxyBypass);
    }
    return result;
}

std::vector<URI>
ProxyCache::proxyFromUserSettings(const URI &uri)
{
    ProxySettings settings = getUserProxySettings();
    if (settings.autoDetect || !settings.pacScript.empty())
        return autoDetectProxy(uri, settings.pacScript);
    return proxyFromList(uri, settings.proxy, settings.bypassList);
}
#elif defined(OSX)

static std::vector<URI> proxyFromPacScript(CFURLRef cfurl, CFURLRef targeturl);

static std::vector<URI> proxyFromCFArray(CFArrayRef proxies, CFURLRef targeturl)
{
    std::vector<URI> result;
    for (CFIndex i = 0; i < CFArrayGetCount(proxies); ++i) {
        CFDictionaryRef thisProxy = (CFDictionaryRef)CFArrayGetValueAtIndex(proxies, i);
        CFStringRef proxyType = (CFStringRef)CFDictionaryGetValue(thisProxy, kCFProxyTypeKey);
        if (!proxyType || CFGetTypeID(proxyType) != CFStringGetTypeID())
            continue;
        URI thisProxyUri;
        if (CFEqual(proxyType, kCFProxyTypeNone)) {
            result.push_back(URI());
        } else if (CFEqual(proxyType, kCFProxyTypeAutoConfigurationURL)) {
            CFURLRef cfurl = (CFURLRef)CFDictionaryGetValue(thisProxy, kCFProxyAutoConfigurationURLKey);
            if (!cfurl || CFGetTypeID(cfurl) != CFURLGetTypeID())
                continue;
            std::vector<URI> pacProxies = proxyFromPacScript(cfurl, targeturl);
            result.insert(result.end(), pacProxies.begin(), pacProxies.end());
        } else if (CFEqual(proxyType, kCFProxyTypeHTTP)) {
            thisProxyUri.scheme("http");
        } else if (CFEqual(proxyType, kCFProxyTypeHTTPS)) {
            thisProxyUri.scheme("https");
        }
        if (thisProxyUri.schemeDefined()) {
            CFStringRef proxyHost = (CFStringRef)CFDictionaryGetValue(thisProxy, kCFProxyHostNameKey);
            if (!proxyHost || CFGetTypeID(proxyHost) != CFStringGetTypeID())
                continue;
            CFNumberRef proxyPort = (CFNumberRef)CFDictionaryGetValue(thisProxy, kCFProxyPortNumberKey);
            int port = -1;
            if (proxyPort && CFGetTypeID(proxyPort) == CFNumberGetTypeID())
                CFNumberGetValue(proxyPort, kCFNumberIntType, &port);
            thisProxyUri.authority.host(toUtf8(proxyHost));
            if (port != -1)
                thisProxyUri.authority.port(port);
            result.push_back(thisProxyUri);
        }
    }
    return result;
}

// THIS IS CURRENTLY BLOCKING
static std::vector<URI> proxyFromPacScript(CFURLRef cfurl, CFURLRef targeturl)
{
    std::vector<URI> result;
    CFStringRef string = CFURLGetString(cfurl);
    URI uri;
    try {
        uri = toUtf8(string);
    } catch (std::invalid_argument &) {
        return result;
    }
    if (!uri.schemeDefined())
        return result;
    if (uri.scheme() != "http" && uri.scheme() != "https" &&
        uri.scheme() != "file")
        return result;

    try {
        Stream::ptr pacStream;
        MemoryStream localStream;
        if (uri.scheme() == "file") {
            if (uri.authority.hostDefined() &&
                uri.authority.host() != "localhost" &&
                !uri.authority.host().empty())
                return result;
            std::ostringstream os;
            for (std::vector<std::string>::const_iterator it = uri.path.segments.begin();
                it != uri.path.segments.end();
                ++it)
                os << '/' << *it;
            pacStream.reset(new FileStream(os.str(), FileStream::READ));
        } else {
            RequestBroker::ptr requestBroker = defaultRequestBroker();
            pacStream.reset(new HTTPStream(uri, requestBroker));
        }
        pacStream.reset(new LimitedStream(pacStream, 1024 * 1024 + 1));
        if (transferStream(pacStream, localStream) >= 1024u * 1024u)
            return result;
        std::string localBuffer;
        localBuffer.resize((size_t)localStream.size());
        localStream.buffer().copyOut(&localBuffer[0], localBuffer.size());
        ScopedCFRef<CFStringRef> pacScript = CFStringCreateWithBytes(NULL,
            (const UInt8*)localBuffer.c_str(), localBuffer.size(),
            kCFStringEncodingUTF8, true);
        ScopedCFRef<CFErrorRef> error;
        ScopedCFRef<CFArrayRef> pacProxies =
            CFNetworkCopyProxiesForAutoConfigurationScript(pacScript, targeturl,
            &error);
        if (!pacProxies)
            return result;
        return proxyFromCFArray(pacProxies, targeturl);
    } catch (...) {
        return result;
    }
}

std::vector<URI> proxyFromSystemConfiguration(const URI &uri)
{
    std::vector<URI> result;

    std::string uristring = uri.toString();
    ScopedCFRef<CFURLRef> cfurl = CFURLCreateWithBytes(NULL,
        (const UInt8 *)uristring.c_str(),
        uristring.size(), kCFStringEncodingUTF8, NULL);
    if (!cfurl)
        return result;

    ScopedCFRef<CFDictionaryRef> proxySettings = SCDynamicStoreCopyProxies(NULL);
    if (!proxySettings)
        return result;
    ScopedCFRef<CFArrayRef> proxies = CFNetworkCopyProxiesForURL(cfurl,
        proxySettings);
    if (!proxies)
        return result;
    return proxyFromCFArray(proxies, cfurl);
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
