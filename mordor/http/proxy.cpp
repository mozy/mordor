// Copyright (c) 2009 - Mozy, Inc.

#include "proxy.h"
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
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

#ifdef WINDOWS
static std::ostream& operator<<(std::ostream& s,
                                const WINHTTP_PROXY_INFO& info);
#endif

static int compare(const std::string& s1, const std::string& s2);
static bool isMatch(const URI& uri, const std::string& pattern);

static ConfigVar<std::string>::ptr g_httpProxy =
    Config::lookup("http.proxy", std::string(),
    "HTTP Proxy Server");

static Logger::ptr proxyLog = Log::lookup("mordor:http:proxy");

std::vector<URI> proxyFromConfig(const URI &uri)
{
    return proxyFromList(uri, g_httpProxy->val());
}

std::vector<URI> proxyFromList(const URI &uri,
                               std::string proxy,
                               std::string bypassList)
{
    MORDOR_LOG_DEBUG(proxyLog) << "Finding proxy for URI " << uri.toString()
                               << ".";

    if (proxy.empty()) {
        MORDOR_LOG_DEBUG(proxyLog) << "Empty proxy string!";
        return std::vector<URI>();
    }

    // Split the proxy string on '!', and use the second part as the bypass
    // list. Note that the provided bypass list should be empty if the proxy
    // string includes a '!' character; see proxy.h.
    std::vector<std::string> parts;
    boost::split(parts, proxy, boost::is_any_of("!"));
    if (parts.size() > 1) {
        proxy = parts[0];
        if (bypassList.empty()) {
            bypassList = parts[1];
        }
    }

    // Does this URI match a pattern in the bypass list?
    std::vector<std::string> bypassEntries;
    boost::split(bypassEntries, bypassList, boost::is_any_of(";"));
    for (size_t i = 0; i < bypassEntries.size(); i++) {
        MORDOR_LOG_DEBUG(proxyLog) << "Considering bypass list entry \""
                                   << bypassEntries[i] << "\".";
        if (isMatch(uri, bypassEntries[i])) {
            MORDOR_LOG_DEBUG(proxyLog) << "Entry matched!";
            return std::vector<URI>();
        }
    }

    // Find the correct proxy for this URI scheme. We expect the list of proxies
    // to look like this:
    //
    //   list = proxy { ";" proxy }
    //   proxy = { scheme "=" } uri
    //
    // Proxies without a scheme are always included in the returned list. A
    // proxy with a scheme is only included if the scheme matches the scheme
    // of the provided uri.
    std::vector<URI> proxyList;
    std::vector<std::string> v;
    boost::split(v, proxy, boost::is_any_of(";"));
    for (size_t i = 0; i < v.size(); i++) {
        MORDOR_LOG_DEBUG(proxyLog) << "Considering proxy \"" << v[i] << "\".";

        // Match the scheme and the URI for each proxy in the list. See above.
        static const boost::regex e("(?:([a-zA-Z]+)=)?(.*)");
        boost::smatch m;
        if (!boost::regex_match(v[i], m, e)) {
            MORDOR_LOG_DEBUG(proxyLog) << "Invalid proxy \"" << v[i] << "\".";
            continue;
        }

        if (!m[1].matched || compare(m[1].str(), uri.scheme()) == 0) {
            // XXX The entry in the proxy list typically doesn't include the
            // scheme in the URI, so we use a regex here to parse it instead of
            // simply creating a Mordor::URI directly.
            std::string proxyURI = m[2].str();
            static const boost::regex uriRegex("(?:[a-zA-Z]+://)?(.*)");
            boost::smatch uriMatch;
            if (!boost::regex_match(proxyURI, uriMatch, uriRegex)) {
                MORDOR_LOG_DEBUG(proxyLog) << "Invalid proxy \"" << v[i]
                                           << "\".";
                continue;
            }

            std::ostringstream ss;
            ss << uri.scheme() << "://" << uriMatch[1].str();

            MORDOR_LOG_DEBUG(proxyLog) << "Proxy matches. Using proxy URI \""
                                       << ss.str() << "\".";
            proxyList.push_back(URI(ss.str()));
        }
    }

    return proxyList;
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

boost::mutex ProxyCache::s_cacheMutex;
bool ProxyCache::s_failedAutoDetect = false;
std::set<std::string> ProxyCache::s_invalidConfigURLs;

void ProxyCache::resetDetectionResultCache()
{
    boost::mutex::scoped_lock lock(s_cacheMutex);
    s_failedAutoDetect = false;
    s_invalidConfigURLs.clear();
}

bool ProxyCache::autoDetectProxy(const URI &uri, const std::string &pacScript,
                                 std::vector<URI> &proxyList)
{
    MORDOR_LOG_DEBUG(proxyLog) << "Auto-detecting proxy for URI \""
                               << uri.toString() << "\".";

    // We can't auto-detect or auto-configure without an HTTP session handle.
    if (!m_hHttpSession) {
        MORDOR_LOG_DEBUG(proxyLog) << "No HTTP session!";
        return false;
    }

    // Check to see we've already tried to process the provided PAC file URL,
    // and failed. Auto-detection and auto-configuration failures can take a
    // long time, so we want to avoid trying them repeatedly.
    {
        boost::mutex::scoped_lock lock(s_cacheMutex);
        if (pacScript.empty()) {
            if (s_failedAutoDetect) {
                MORDOR_LOG_DEBUG(proxyLog) << "Using cached auto-detection result.";
                return false;
            }
        } else {
            if (s_invalidConfigURLs.find(pacScript) != s_invalidConfigURLs.end()) {
                MORDOR_LOG_DEBUG(proxyLog) << "Using cached auto-config result.";
                return false;
            }
        }
    }


    WINHTTP_AUTOPROXY_OPTIONS options = {0};
    options.fAutoLogonIfChallenged = TRUE;
    std::wstring pacScriptW = toUtf16(pacScript);
    if (!pacScriptW.empty()) {
        options.dwFlags = WINHTTP_AUTOPROXY_CONFIG_URL;
        options.lpszAutoConfigUrl = pacScriptW.c_str();
    } else {
        options.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
        options.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP |
                                    WINHTTP_AUTO_DETECT_TYPE_DNS_A;
    }

    WINHTTP_PROXY_INFO info = {0};
    if (!pWinHttpGetProxyForUrl(m_hHttpSession,
                                toUtf16(uri.toString()).c_str(),
                                &options,
                                &info)) {
        // Record the fact that this PAC file URL failed in our cache, so that
        // we don't attempt to run it again.
        {
            boost::mutex::scoped_lock lock(s_cacheMutex);
            if (pacScript.empty()) {
                s_failedAutoDetect = true;
            } else {
                s_invalidConfigURLs.insert(pacScript);
            }
        }

        error_t error = Mordor::lastError();
        MORDOR_LOG_ERROR(proxyLog) << "WinHttpGetProxyForUrl: (" << error
                                   << ") : " << uri.toString() << " : "
                                   << pacScript ;
        return false;
    }

    MORDOR_LOG_DEBUG(proxyLog) << "Auto-detected proxy: " << std::endl << info;
    proxyList = proxyFromProxyInfo(uri, info);
    return true;
}

std::vector<URI> ProxyCache::proxyFromUserSettings(const URI &uri)
{
    ProxySettings settings = getUserProxySettings();

    if (settings.autoDetect) {
        std::vector<URI> proxyList;
        if (autoDetectProxy(uri, "", proxyList)) {
            return proxyList;
        }
    }

    if (!settings.pacScript.empty()) {
        std::vector<URI> proxyList;
        if (autoDetectProxy(uri, settings.pacScript, proxyList)) {
            return proxyList;
        }
    }

    return proxyFromList(uri, settings.proxy, settings.bypassList);
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

std::vector<URI> ProxyCache::proxyFromCFArray(CFArrayRef proxies, ScopedCFRef<CFURLRef> targeturl,
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

std::vector<URI> ProxyCache::proxyFromPacScript(CFURLRef cfurl, ScopedCFRef<CFURLRef> targeturl,
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

// Returns true if the given address matches the given bypass list entry.
bool isMatch(const URI& uri, const std::string& pattern)
{
    MORDOR_ASSERT(uri.schemeDefined());
    MORDOR_ASSERT(uri.authority.hostDefined());

    if (pattern.empty()) {
        return false;
    }

    // There is no standard describing what a valid bypass list entry looks
    // like, and slightly different formats are used by different operating
    // systems and applications.
    //
    // This function accepts entries in the following format:
    //
    //     entry = [scheme "://"] address [":" port];
    //     scheme = { alnum } ;
    //     address = { alnum "." } "*" { "." alnum } ;
    //     port = { digit }
    //
    // A wildcard character in the address matches any number of characters.
    // List entries with multiple wildcards are not supported. Note that this
    // function also doesn't support CIDR-style IP address ranges, which are
    // supported by some browsers and operating systems.
    boost::regex e("(?:([a-zA-Z]+)://)?(.*?)(?::(\\d+))?");
    boost::smatch m;
    if (!boost::regex_match(pattern, m, e)) {
        MORDOR_LOG_DEBUG(proxyLog) << "Invalid rule \"" << pattern << "\".";
        return false;
    }

    std::string patternScheme = m[1].str();
    std::string patternAddress = m[2].str();

    int patternPort = 0;
    try {
        if (m[3].matched) {
            patternPort = boost::lexical_cast<int>(m[3].str());
        }
    } catch(boost::bad_lexical_cast&) {
        MORDOR_LOG_DEBUG(proxyLog) << "Invalid rule \"" << pattern << "\".";
        return false;
    }

    // If a scheme is present in the rule, it must match the sheme in the
    // URI. If the rule doesn't specify a scheme, we consider it to match
    // all schemes.
    //
    // XXX Like all string comparisons, this is fraught with peril. We assume
    // that the provided strings are utf-8 encoded, and already normalized.
    //
    // XXX We also should not be using stricmp here, since it will not work on
    // multibyte characters.
    if (!patternScheme.empty() && compare(patternScheme, uri.scheme())) {
        // Scheme doesn't match.
        return false;
    }

    // As a special case, if the pattern is "<local>" and the address is a
    // hostname that doesn't contain any '.' characters, then we consider this
    // to be a match. This is for compatability with IE on Windows.
    std::string address = uri.authority.host();
    if (patternAddress == "<local>" && std::string::npos == address.find('.')) {
        return true;
    }

    boost::replace_all(patternAddress, ".", "\\.");
    boost::replace_all(patternAddress, "*", ".*");
    boost::regex hostRegex(patternAddress, boost::regex::perl | boost::regex::icase);
    if (!boost::regex_match(address, hostRegex)) {
        // Address doesn't match.
        return false;
    }

    if (0 != patternPort && patternPort != uri.authority.port()) {
        // Port doesn't match.
        return false;
    }

    return true;
}

int compare(const std::string& s1, const std::string& s2)
{
    return stricmp(s1.c_str(), s2.c_str());
}

#ifdef WINDOWS
std::ostream& operator<<(std::ostream& s, const WINHTTP_PROXY_INFO& info)
{
    std::string proxy = info.lpszProxy ? toUtf8(info.lpszProxy) : "(null)";
    std::string bypass = info.lpszProxyBypass ? toUtf8(info.lpszProxyBypass)
        : "(null)";
    s << "Access Type: " << info.dwAccessType << std::endl
      << "Proxy: " << proxy << std::endl
      << "Proxy Bypass: " << bypass << std::endl;
    return s;
}
#endif

} // namespace HTTP
} // namespace Mordor
