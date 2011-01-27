#ifndef __HTTP_BROKER_H__
#define __HTTP_BROKER_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <openssl/ssl.h>

#include "http.h"
#include "mordor/fibersynchronization.h"

namespace Mordor {

class IOManager;
class Scheduler;
class Socket;
class Stream;
class TimerManager;

namespace HTTP {

class ClientConnection;
class ClientRequest;
class RequestBroker;
class ServerConnection;
class ServerRequest;

class StreamBroker
{
public:
    typedef boost::shared_ptr<StreamBroker> ptr;
    typedef boost::weak_ptr<StreamBroker> weak_ptr;

public:
    virtual ~StreamBroker() {}

    virtual boost::shared_ptr<Stream> getStream(const URI &uri) = 0;
    virtual void cancelPending() {}
};

class StreamBrokerFilter : public StreamBroker
{
public:
    typedef boost::shared_ptr<StreamBrokerFilter> ptr;

public:
    StreamBrokerFilter(StreamBroker::ptr parent,
        StreamBroker::weak_ptr weakParent = StreamBroker::weak_ptr())
        : m_parent(parent),
          m_weakParent(weakParent)
    {}

    StreamBroker::ptr parent();
    void parent(StreamBroker::ptr parent)
    { m_parent = parent; m_weakParent.reset(); }
    void parent(StreamBroker::weak_ptr parent)
    { m_weakParent = parent; m_parent.reset(); }

    void cancelPending() { parent()->cancelPending(); }

private:
    StreamBroker::ptr m_parent;
    StreamBroker::weak_ptr m_weakParent;
};

class SocketStreamBroker : public StreamBroker
{
public:
    typedef boost::shared_ptr<SocketStreamBroker> ptr;

public:
    SocketStreamBroker(IOManager *ioManager = NULL, Scheduler *scheduler = NULL)
        : m_cancelled(false),
          m_ioManager(ioManager),
          m_scheduler(scheduler),
          m_connectTimeout(~0ull)
    {}

    void connectTimeout(unsigned long long timeout) { m_connectTimeout = timeout; }

    boost::shared_ptr<Stream> getStream(const URI &uri);
    void cancelPending();

private:
    boost::mutex m_mutex;
    bool m_cancelled;
    std::list<boost::shared_ptr<Socket> > m_pending;
    IOManager *m_ioManager;
    Scheduler *m_scheduler;
    unsigned long long m_connectTimeout;
};

class ConnectionBroker
{
public:
    typedef boost::shared_ptr<ConnectionBroker> ptr;
    typedef boost::weak_ptr<ConnectionBroker> weak_ptr;

public:
    virtual ~ConnectionBroker() {}

    virtual std::pair<boost::shared_ptr<ClientConnection>, bool>
        getConnection(const URI &uri, bool forceNewConnection = false) = 0;
};

struct PriorConnectionFailedException : virtual Exception {};

class ConnectionCache : public ConnectionBroker
{
public:
    typedef boost::shared_ptr<ConnectionCache> ptr;

public:
    ConnectionCache(StreamBroker::ptr streamBroker, TimerManager *timerManager = NULL)
        : m_streamBroker(streamBroker),
          m_connectionsPerHost(1u),
          m_closed(false),
          m_verifySslCertificate(false),
          m_verifySslCertificateHost(true),
          m_timerManager(timerManager),
          m_httpReadTimeout(~0ull),
          m_httpWriteTimeout(~0ull),
          m_idleTimeout(~0ull),
          m_sslReadTimeout(~0ull),
          m_sslWriteTimeout(~0ull),
          m_sslCtx(NULL)
    {}

    void connectionsPerHost(size_t connections) { m_connectionsPerHost = connections; }
    void httpReadTimeout(unsigned long long timeout) { m_httpReadTimeout = timeout; }
    void httpWriteTimeout(unsigned long long timeout) { m_httpWriteTimeout = timeout; }
    void idleTimeout(unsigned long long timeout) { m_idleTimeout = timeout; }
    void sslReadTimeout(unsigned long long timeout) { m_sslReadTimeout = timeout; }
    void sslWriteTimeout(unsigned long long timeout) { m_sslWriteTimeout = timeout; }
    void sslCtx(SSL_CTX *ctx) { m_sslCtx = ctx; }
    void verifySslCertificate(bool verify) { m_verifySslCertificate = verify; }
    void verifySslCertificateHost(bool verify) { m_verifySslCertificateHost = verify; }
    // Required to support any proxies
    void proxyForURI(boost::function<std::vector<URI> (const URI &)> proxyForURIDg)
    { m_proxyForURIDg = proxyForURIDg; }
    // Required to support HTTPS proxies
    void proxyRequestBroker(boost::shared_ptr<RequestBroker> broker)
    { m_proxyBroker = broker; }

    std::pair<boost::shared_ptr<ClientConnection>, bool>
        getConnection(const URI &uri, bool forceNewConnection = false);

    void closeIdleConnections();
    void abortConnections();

private:
    typedef std::list<boost::shared_ptr<ClientConnection> > ConnectionList;
    struct ConnectionInfo
    {
        ConnectionInfo(FiberMutex &mutex)
            : condition(mutex),
              lastFailedConnectionTimestamp(~0ull)
        {}

        ConnectionList connections;
        FiberCondition condition;
        unsigned long long lastFailedConnectionTimestamp;
    };
    typedef std::map<URI, boost::shared_ptr<ConnectionInfo> > CachedConnectionMap;

private:
    std::pair<boost::shared_ptr<ClientConnection>, bool>
        getConnectionViaProxyFromCache(const URI &uri, const URI &proxy);
    std::pair<boost::shared_ptr<ClientConnection>, bool>
        getConnectionViaProxy(const URI &uri, const URI &proxy,
        FiberMutex::ScopedLock &lock);
    void cleanOutDeadConns(CachedConnectionMap &conns);
    void addSSL(const URI &uri, boost::shared_ptr<Stream> &stream);
    void dropConnection(const URI &uri, const ClientConnection *connection);

private:
    FiberMutex m_mutex;
    StreamBroker::ptr m_streamBroker;
    size_t m_connectionsPerHost;

    CachedConnectionMap m_conns;
    bool m_closed, m_verifySslCertificate, m_verifySslCertificateHost;
    TimerManager *m_timerManager;
    unsigned long long m_httpReadTimeout, m_httpWriteTimeout, m_idleTimeout,
        m_sslReadTimeout, m_sslWriteTimeout;
    SSL_CTX *m_sslCtx;
    boost::function<std::vector<URI> (const URI &)> m_proxyForURIDg;
    boost::shared_ptr<RequestBroker> m_proxyBroker;
};

class MockConnectionBroker : public ConnectionBroker
{
private:
    typedef std::map<URI, boost::shared_ptr<ClientConnection> >
        ConnectionCache;
public:
    MockConnectionBroker(boost::function<void (const URI &uri,
            boost::shared_ptr<ServerRequest>)> dg,
        TimerManager *timerManager = NULL, unsigned long long readTimeout = ~0ull,
        unsigned long long writeTimeout = ~0ull)
        : m_dg(dg),
          m_timerManager(timerManager),
          m_readTimeout(readTimeout),
          m_writeTimeout(writeTimeout)
    {}

    std::pair<boost::shared_ptr<ClientConnection>, bool>
        getConnection(const URI &uri, bool forceNewConnection = false);

private:
    boost::function<void (const URI &uri, boost::shared_ptr<ServerRequest>)> m_dg;
    ConnectionCache m_conns;
    TimerManager *m_timerManager;
    unsigned long long m_readTimeout, m_writeTimeout;
};

class RequestBroker
{
public:
    typedef boost::shared_ptr<RequestBroker> ptr;
    typedef boost::weak_ptr<RequestBroker> weak_ptr;

public:
    virtual ~RequestBroker() {}

    virtual boost::shared_ptr<ClientRequest> request(Request &requestHeaders,
        bool forceNewConnection = false,
        boost::function<void (boost::shared_ptr<ClientRequest>)> bodyDg = NULL)
        = 0;
};

class RequestBrokerFilter : public RequestBroker
{
public:
    RequestBrokerFilter(RequestBroker::ptr parent,
        RequestBroker::weak_ptr weakParent = RequestBroker::weak_ptr())
        : m_parent(parent),
          m_weakParent(weakParent)
    {}

    RequestBroker::ptr parent();

    boost::shared_ptr<ClientRequest> request(Request &requestHeaders,
        bool forceNewConnection = false,
        boost::function<void (boost::shared_ptr<ClientRequest>)> bodyDg = NULL)
        = 0;

private:
    RequestBroker::ptr m_parent;
    RequestBroker::weak_ptr m_weakParent;
};

/// An exception coming from BaseRequestBroker will be tagged with CONNECTION
/// if the exception came while trying to establish the connection, HTTP
/// if the exception came specifically from HTTP communications, or no
/// ExceptionSource at all if it came from other code
enum ExceptionSource
{
    CONNECTION,
    HTTP
};
typedef boost::error_info<struct tag_source, ExceptionSource > errinfo_source;

class BaseRequestBroker : public RequestBroker
{
public:
    typedef boost::shared_ptr<BaseRequestBroker> ptr;

public:
    BaseRequestBroker(ConnectionBroker::ptr connectionBroker)
        : m_connectionBroker(connectionBroker)
    {}
    BaseRequestBroker(ConnectionBroker::weak_ptr connectionBroker)
        : m_weakConnectionBroker(connectionBroker)
    {}

    boost::shared_ptr<ClientRequest> request(Request &requestHeaders,
        bool forceNewConnection = false,
        boost::function<void (boost::shared_ptr<ClientRequest>)> bodyDg = NULL);

private:
    ConnectionBroker::ptr m_connectionBroker;
    ConnectionBroker::weak_ptr m_weakConnectionBroker;
};

/// Retries connection error and PriorRequestFailed errors
class RetryRequestBroker : public RequestBrokerFilter
{
public:
    typedef boost::shared_ptr<RetryRequestBroker> ptr;

public:
    RetryRequestBroker(RequestBroker::ptr parent,
        boost::function<bool (size_t)> delayDg = NULL)
        : RequestBrokerFilter(parent),
          m_delayDg(delayDg),
          mp_retries(NULL)
    {}

    void sharedRetryCounter(size_t *retries) { mp_retries = retries; }

    boost::shared_ptr<ClientRequest> request(Request &requestHeaders,
        bool forceNewConnection = false,
        boost::function<void (boost::shared_ptr<ClientRequest>)> bodyDg = NULL);

private:
    boost::function<bool (size_t)> m_delayDg;
    size_t *mp_retries;
};

struct CircularRedirectException : Exception
{
    CircularRedirectException(const URI &uri)
        : m_uri(uri)
    {}
    ~CircularRedirectException() throw() {}

    URI uri() { return m_uri; }

private:
    URI m_uri;
};

class RedirectRequestBroker : public RequestBrokerFilter
{
public:
    typedef boost::shared_ptr<RedirectRequestBroker> ptr;

public:
    RedirectRequestBroker(RequestBroker::ptr parent, size_t maxRedirects = 70)
        : RequestBrokerFilter(parent),
          m_maxRedirects(maxRedirects),
          m_handle301(true),
          m_handle302(true),
          m_handle307(true)
    {}

    void handlePermanentRedirect(bool handle) { m_handle301 = handle; }
    void handleFound(bool handle) { m_handle302 = handle; }
    void handleTemporaryRedirect(bool handle) { m_handle307 = handle; }

    boost::shared_ptr<ClientRequest> request(Request &requestHeaders,
        bool forceNewConnection = false,
        boost::function<void (boost::shared_ptr<ClientRequest>)> bodyDg = NULL);

private:
    size_t m_maxRedirects;
    bool m_handle301, m_handle302, m_handle307;
};

class UserAgentRequestBroker : public RequestBrokerFilter
{
public:
    UserAgentRequestBroker(RequestBroker::ptr parent,
        const ProductAndCommentList &userAgent)
        : RequestBrokerFilter(parent),
          m_userAgent(userAgent)
    {}

    boost::shared_ptr<ClientRequest> request(Request &requestHeaders,
        bool forceNewConnection = false,
        boost::function<void (boost::shared_ptr<ClientRequest>)> bodyDg = NULL);

private:
    ProductAndCommentList m_userAgent;
};

struct RequestBrokerOptions
{
    RequestBrokerOptions() :
        ioManager(NULL),
        scheduler(NULL),
        handleRedirects(true),
        timerManager(NULL),
        connectTimeout(~0ull),
        sslConnectReadTimeout(~0ull),
        sslConnectWriteTimeout(~0ull),
        httpReadTimeout(~0ull),
        httpWriteTimeout(~0ull),
        idleTimeout(~0ull),
        sslCtx(NULL),
        verifySslCertificate(false),
        verifySslCertificateHost(true)
    {}

    IOManager *ioManager;
    Scheduler *scheduler;
    boost::function<bool (size_t)> delayDg;
    bool handleRedirects;
    TimerManager *timerManager;
    unsigned long long connectTimeout;
    unsigned long long sslConnectReadTimeout;
    unsigned long long sslConnectWriteTimeout;
    unsigned long long httpReadTimeout;
    unsigned long long httpWriteTimeout;
    unsigned long long idleTimeout;
    boost::function<std::vector<URI> (const URI &)> proxyForURIDg;
    /// Required to enable https proxy support
    RequestBroker::ptr proxyRequestBroker;
    boost::function<bool (const URI &,
            boost::shared_ptr<ClientRequest> /* priorRequest = ClientRequest::ptr() */,
            std::string & /* scheme */, std::string & /* realm */,
            std::string & /* username */, std::string & /* password */,
            size_t /* attempts */)>
            getCredentialsDg, getProxyCredentialsDg;
    StreamBrokerFilter::ptr customStreamBrokerFilter;
    SSL_CTX *sslCtx;
    bool verifySslCertificate;
    bool verifySslCertificateHost;
    ProductAndCommentList userAgent;
};

std::pair<RequestBroker::ptr, ConnectionCache::ptr>
    createRequestBroker(const RequestBrokerOptions &options = RequestBrokerOptions());

/// @deprecated Use createRequestBroker instead
RequestBroker::ptr defaultRequestBroker(IOManager *ioManager = NULL,
                                        Scheduler *scheduler = NULL,
                                        ConnectionBroker::ptr *connBroker = NULL,
                                        boost::function<bool (size_t)> delayDg = NULL);
}}

#endif
