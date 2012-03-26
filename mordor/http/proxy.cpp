// Copyright (c) 2009 - Mozy, Inc.

#include "proxy.h"

#include "mordor/config.h"
#include "mordor/http/broker.h"
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
#include "mordor/sleep.h"
#endif

namespace Mordor {
namespace HTTP {

static ConfigVar<std::string>::ptr g_httpProxy =
    Config::lookup("http.proxy", std::string(),
    "HTTP Proxy Server");

static Logger::ptr proxyLog = Log::lookup("mordor:http:proxy");

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
        if(curProxy.empty())
            continue;
        std::string forScheme;
        size_t equals = curProxy.find('=');
        if (equals != std::string::npos) {
            forScheme = curProxy.substr(0, equals);
            curProxy = curProxy.substr(equals + 1);
        }
        std::transform(forScheme.begin(), forScheme.end(), forScheme.begin(),
            &tolower);
        if (!forScheme.empty() && forScheme != uri.scheme() &&
            forScheme != "socks")
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
            std::transform(scheme.begin(), scheme.end(), scheme.begin(),
                &tolower);
            if (forScheme.empty() && scheme != uri.scheme())
                continue;
            proxyUri.scheme(scheme);
        } else {
            if (forScheme == "socks")
                proxyUri.scheme("socks");
            else
                proxyUri.scheme(uri.scheme());
        }
        proxyUri.path.segments.clear();
        proxyUri.authority.userinfoDefined(false);
        proxyUri.queryDefined(false);
        proxyUri.fragmentDefined(false);
        result.push_back(proxyUri);
    }
    return result;
}

#ifdef WINDOWS

static std::vector<URI> proxyFromProxyInfo(const URI &uri,
    WINHTTP_PROXY_INFO &proxyInfo)
{
    std::vector<URI> result;
    std::string proxy, bypassList;
    try {
        if (proxyInfo.lpszProxy)
            proxy = toUtf8(proxyInfo.lpszProxy);
        if (proxyInfo.lpszProxyBypass)
            bypassList = toUtf8(proxyInfo.lpszProxyBypass);
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
    switch (proxyInfo.dwAccessType) {
        case WINHTTP_ACCESS_TYPE_NAMED_PROXY:
            return proxyFromList(uri, proxy, bypassList);
        case WINHTTP_ACCESS_TYPE_NO_PROXY:
            return result;
        default:
            MORDOR_NOTREACHED();
    }
}

std::vector<URI> proxyFromMachineDefault(const URI &uri)
{
    WINHTTP_PROXY_INFO proxyInfo;
    if (!pWinHttpGetDefaultProxyConfiguration(&proxyInfo))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WinHttpGetDefaultProxyConfiguration");
    return proxyFromProxyInfo(uri, proxyInfo);
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

// Use WPAD protocol to search for proxy configuration file (e.g. a pac script)
static std::string
autoDetectConfigUrl()
{
    std::string url;
    PWSTR autoConfigUrl = NULL;
    if (pWinHttpDetectAutoProxyConfigUrl(
            WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A,
            &autoConfigUrl
            )) {
        if (autoConfigUrl) {
            url = toUtf8(autoConfigUrl);
            GlobalFree((HGLOBAL)autoConfigUrl);
        }
    } else {
        // This is expected on any system where autodetect is enabled but no proxy is present
        MORDOR_LOG_DEBUG(proxyLog) << "WinHttpDetectAutoProxyConfigUrl found no proxy (" << Mordor::lastError() << ")";
    }
    return url;
}


ProxyCache::ProxyCache(const std::string &userAgent)
{
    m_hHttpSession = pWinHttpOpen(toUtf16(userAgent).c_str(),
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!m_hHttpSession && lastError() != ERROR_CALL_NOT_IMPLEMENTED)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WinHttpOpen");
}

ProxyCache::~ProxyCache()
{
    if (m_hHttpSession)
        pWinHttpCloseHandle(m_hHttpSession);
}

boost::mutex m_proxiesMutex;
bool ProxyCache::m_bAutoProxyFailed = false;
std::string ProxyCache::m_autoConfigUrl; // Autodetected pac Script, if any
std::set<std::string> ProxyCache::m_invalidProxies;


void
ProxyCache::resetDetectionResultCache()
{
    boost::mutex::scoped_lock lk(m_proxiesMutex);
    m_bAutoProxyFailed = false;
    m_autoConfigUrl.clear();
    m_invalidProxies.clear();
}

std::vector<URI>
ProxyCache::autoDetectProxy(const URI &uri, const std::string &pacScript)
{
    std::vector<URI> result;
    if (!m_hHttpSession)
        return result;

    {
        boost::mutex::scoped_lock lk(m_proxiesMutex);
        if (!pacScript.empty())
        {
           if (m_invalidProxies.find(pacScript) != m_invalidProxies.end())
                return result;
        } else if (m_bAutoProxyFailed) {
                return result;
        }
    }

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
        return proxyFromProxyInfo(uri, proxyInfo);
    }
    else
    {
        error_t error = Mordor::lastError();
        MORDOR_LOG_ERROR(proxyLog) << "WinHttpGetProxyForUrl: (" << error << ") : " << uri.toString() << " : " <<  pacScript ;
        {
            boost::mutex::scoped_lock lk(m_proxiesMutex);
            if (pacScript.empty())
                m_bAutoProxyFailed = true;
            else
                m_invalidProxies.insert(pacScript);
        }
    }
    return result;
}

std::vector<URI>
ProxyCache::proxyFromUserSettings(const URI &uri)
{
    ProxySettings settings = getUserProxySettings();
    std::vector<URI> result, temp;
    bool currentAutoProxyFailed;
    std::string currentAutoConfigUrl;
    {
        boost::mutex::scoped_lock lk(m_proxiesMutex);
        currentAutoProxyFailed = m_bAutoProxyFailed;
        currentAutoConfigUrl = m_autoConfigUrl;
    }
    if (settings.autoDetect && !currentAutoProxyFailed) {
        if (currentAutoConfigUrl.empty()) {
            std::string newAutoConfigUrl = autoDetectConfigUrl();
            {
                boost::mutex::scoped_lock lk(m_proxiesMutex);
                m_autoConfigUrl = newAutoConfigUrl;
                currentAutoConfigUrl = newAutoConfigUrl;
                // Detection may take a long time to fail on some networks
                // that have no proxy, so avoid calling it again
                m_bAutoProxyFailed = m_autoConfigUrl.empty();
                currentAutoProxyFailed = m_bAutoProxyFailed;
            }
        }
        if (!currentAutoProxyFailed)
            result = autoDetectProxy(uri, currentAutoConfigUrl);
    }
    if (!settings.pacScript.empty()) {
        temp = autoDetectProxy(uri, settings.pacScript);
        result.insert(result.end(), temp.begin(), temp.end());
    }
    temp = proxyFromList(uri, settings.proxy, settings.bypassList);
    result.insert(result.end(), temp.begin(), temp.end());
    return result;
}
#elif defined(OSX)
ProxyCache::ProxyCache(RequestBroker::ptr requestBroker)
    : m_requestBroker(requestBroker),
      m_pacThreadCancelled(false)
{
    m_dynamicStore = SCDynamicStoreCreate(NULL, NULL, NULL, NULL);
}

ProxyCache::~ProxyCache()
{
    // Shut down the PAC worker thread
    {
        boost::mutex::scoped_lock lk(m_pacMut);
        m_pacThreadCancelled = true;
    }
    m_pacCond.notify_one();
    m_pacThread.join();
}

std::vector<URI> ProxyCache::proxyFromCFArray(CFArrayRef proxies, CFURLRef targeturl,
    RequestBroker::ptr requestBroker,
    std::map<URI, ScopedCFRef<CFStringRef> > &cachedScripts)
{
    std::vector<URI> result;
    for (CFIndex i = 0; i < CFArrayGetCount(proxies); ++i) {
        CFDictionaryRef thisProxy =
            (CFDictionaryRef)CFArrayGetValueAtIndex(proxies, i);
        CFStringRef proxyType = (CFStringRef)CFDictionaryGetValue(thisProxy,
            kCFProxyTypeKey);
        if (!proxyType || CFGetTypeID(proxyType) != CFStringGetTypeID())
            continue;
        URI thisProxyUri;
        if (CFEqual(proxyType, kCFProxyTypeNone)) {
            result.push_back(URI());
        } else if (CFEqual(proxyType, kCFProxyTypeAutoConfigurationURL)) {
            CFURLRef cfurl = (CFURLRef)CFDictionaryGetValue(thisProxy,
                kCFProxyAutoConfigurationURLKey);
            if (!cfurl || CFGetTypeID(cfurl) != CFURLGetTypeID())
                continue;
            std::vector<URI> pacProxies = proxyFromPacScript(cfurl, targeturl,
                requestBroker, cachedScripts);
            result.insert(result.end(), pacProxies.begin(), pacProxies.end());
        } else if (CFEqual(proxyType, kCFProxyTypeHTTP)) {
            thisProxyUri.scheme("http");
        } else if (CFEqual(proxyType, kCFProxyTypeHTTPS)) {
            thisProxyUri.scheme("https");
        } else if (CFEqual(proxyType, kCFProxyTypeSOCKS)) {
            thisProxyUri.scheme("socks");
        }
        if (thisProxyUri.schemeDefined()) {
            CFStringRef proxyHost = (CFStringRef)CFDictionaryGetValue(
                thisProxy, kCFProxyHostNameKey);
            if (!proxyHost || CFGetTypeID(proxyHost) != CFStringGetTypeID())
                continue;
            CFNumberRef proxyPort = (CFNumberRef)CFDictionaryGetValue(
                thisProxy, kCFProxyPortNumberKey);
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

// Worker thread that handles PAC evaluation. This has to be done in a
// separate, fiber-free thread or the underlying JavaScript VM used in
// CFNetworkCopyProxiesForAutoConfigurationScript will crash trying to
// perform garbage collection.
void ProxyCache::runPacWorker()
{
    while(true) {
        boost::mutex::scoped_lock lk(m_pacMut);

        while(m_pacQueue.empty() && !m_pacThreadCancelled)
            m_pacCond.wait(lk);

        if(m_pacThreadCancelled)
            return;

        while(!m_pacQueue.empty()) {
            struct PacMessage *msg = m_pacQueue.front();
            m_pacQueue.pop();

            lk.unlock();

            CFErrorRef error;
            ScopedCFRef<CFArrayRef> proxies =
                CFNetworkCopyProxiesForAutoConfigurationScript(
                    msg->pacScript, msg->targeturl, &error);

            lk.lock();

            msg->result = proxies;
            msg->processed = true;
        }
    }
}

std::vector<URI> ProxyCache::proxyFromPacScript(CFURLRef cfurl, CFURLRef targeturl,
    RequestBroker::ptr requestBroker,
    std::map<URI, ScopedCFRef<CFStringRef> > &cachedScripts)
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
        ScopedCFRef<CFStringRef> pacScript;
        std::map<URI, ScopedCFRef<CFStringRef> >::iterator it =
            cachedScripts.find(uri);
        if (it != cachedScripts.end()) {
            pacScript = it->second;
        } else {
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
                pacStream.reset(new HTTPStream(uri, requestBroker));
            }
            pacStream.reset(new LimitedStream(pacStream, 1024 * 1024 + 1));
            if (transferStream(pacStream, localStream) >= 1024u * 1024u)
                return result;
            std::string localBuffer;
            localBuffer.resize((size_t)localStream.size());
            localStream.buffer().copyOut(&localBuffer[0], localBuffer.size());
            pacScript = CFStringCreateWithBytes(NULL,
                (const UInt8*)localBuffer.c_str(), localBuffer.size(),
                kCFStringEncodingUTF8, true);
            cachedScripts[uri] = pacScript;
        }

        // Start the PAC worker thread if not already running
        // by checking to see if the thread is "Not-a-Thread"
        if(boost::thread::id() == m_pacThread.get_id()) {
            m_pacThread = boost::thread(&ProxyCache::runPacWorker, this);
            MORDOR_LOG_DEBUG(proxyLog) << "PAC worker thread id : " << m_pacThread.get_id();
        }

        // Evaluate the PAC in the fiber-free worker thread so that
        // we don't crash due to the JavaScript VM's garbage collection.
        struct PacMessage msg;
        {
            boost::mutex::scoped_lock lk(m_pacMut);
            msg.pacScript = pacScript;
            msg.targeturl = targeturl;
            msg.result = NULL;
            msg.processed = false;
            m_pacQueue.push(&msg);
        }

        // Notify the worker thread that there is a new message in the queue
        m_pacCond.notify_one();

        // Wake up periodically to see if our PAC message has been processed
        while(true) {
            boost::mutex::scoped_lock lk(m_pacMut);
            if(msg.processed)
                break;
            lk.unlock();
            Mordor::sleep(1000);
        }

        if(!msg.result)
            return result;
        return proxyFromCFArray(msg.result, targeturl, requestBroker, cachedScripts);
    } catch (...) {
        return result;
    }
}

std::vector<URI>
ProxyCache::proxyFromSystemConfiguration(const URI &uri)
{
    std::vector<URI> result;

    std::string uristring = uri.toString();
    ScopedCFRef<CFURLRef> cfurl = CFURLCreateWithBytes(NULL,
        (const UInt8 *)uristring.c_str(),
        uristring.size(), kCFStringEncodingUTF8, NULL);
    if (!cfurl)
        return result;

    ScopedCFRef<CFDictionaryRef> proxySettings =
        SCDynamicStoreCopyProxies(m_dynamicStore);
    if (!proxySettings)
        return result;
    ScopedCFRef<CFArrayRef> proxies = CFNetworkCopyProxiesForURL(cfurl,
        proxySettings);
    if (!proxies)
        return result;
    return proxyFromCFArray(proxies, cfurl, m_requestBroker,
        m_cachedScripts);
}
#endif

boost::shared_ptr<Stream>
tunnel(RequestBroker::ptr requestBroker, const URI &proxy, const URI &target)
{
    MORDOR_ASSERT(proxy.schemeDefined());

    std::ostringstream os;
    if (!target.authority.hostDefined())
        MORDOR_THROW_EXCEPTION(std::invalid_argument("No host defined"));
    os << target.authority.host() << ':';
    if (target.authority.portDefined())
        os << target.authority.port();
    else if (target.scheme() == "http")
        os << "80";
    else if (target.scheme() == "https")
        os << "443";
    else
        // TODO: can this be looked up using the system? (getaddrinfo)
        MORDOR_THROW_EXCEPTION(std::invalid_argument("Unknown protocol for proxying connection"));
    Request requestHeaders;
    requestHeaders.requestLine.method = CONNECT;
    URI &requestUri = requestHeaders.requestLine.uri;
    requestUri.scheme("http");
    requestUri.authority = proxy.authority;
    requestUri.path.segments.push_back(std::string());
    requestUri.path.segments.push_back(os.str());
    requestHeaders.request.host = os.str();
    requestHeaders.general.connection.insert("Proxy-Connection");
    requestHeaders.general.proxyConnection.insert("Keep-Alive");
    ClientRequest::ptr request = requestBroker->request(requestHeaders);
    if (request->response().status.status == HTTP::OK)
        return request->stream();
    else
        MORDOR_THROW_EXCEPTION(InvalidResponseException("Proxy connection failed",
            request));
}

}}
