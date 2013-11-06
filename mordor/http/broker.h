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
          m_connectTimeout(~0ull),
          m_filterNetworkCallback(NULL)
    {}

    void connectTimeout(unsigned long long timeout) { m_connectTimeout = timeout; }

    // Resolve the uri to its IP address, create a socket, then connect
    boost::shared_ptr<Stream> getStream(const URI &uri);
    void cancelPending();

    void networkFilterCallback(boost::function<void (boost::shared_ptr<Socket>)> fnCallback)
    {  m_filterNetworkCallback = fnCallback; }

private:
    boost::mutex m_mutex;
    bool m_cancelled;
    std::list<boost::shared_ptr<Socket> > m_pending; // Multiple connections may be attempted when getaddrinfo returns multiple addresses
    IOManager *m_ioManager;
    Scheduler *m_scheduler;
    unsigned long long m_connectTimeout;
    boost::function<void (boost::shared_ptr<Socket>)> m_filterNetworkCallback;
};

class ConnectionBroker
{
public:
    typedef boost::shared_ptr<ConnectionBroker> ptr;
    typedef boost::weak_ptr<ConnectionBroker> weak_ptr;

public:
    ConnectionBroker():
          m_verifySslCertificate(false),
          m_verifySslCertificateHost(true),
          m_httpReadTimeout(~0ull),
          m_httpWriteTimeout(~0ull),
          m_idleTimeout(~0ull),
          m_sslReadTimeout(~0ull),
          m_sslWriteTimeout(~0ull),
          m_sslCtx(NULL),
          m_timerManager(NULL)
    {}
    virtual ~ConnectionBroker() {}

    virtual std::pair<boost::shared_ptr<ClientConnection>, bool>
        getConnection(const URI &uri, bool forceNewConnection = false) = 0;

    void httpReadTimeout(unsigned long long timeout) { m_httpReadTimeout = timeout; }
    void httpWriteTimeout(unsigned long long timeout) { m_httpWriteTimeout = timeout; }
    void idleTimeout(unsigned long long timeout) { m_idleTimeout = timeout; }
    void sslReadTimeout(unsigned long long timeout) { m_sslReadTimeout = timeout; }
    void sslWriteTimeout(unsigned long long timeout) { m_sslWriteTimeout = timeout; }
    void sslCtx(SSL_CTX *ctx) { m_sslCtx = ctx; }
    void verifySslCertificate(bool verify) { m_verifySslCertificate = verify; }
    void verifySslCertificateHost(bool verify) { m_verifySslCertificateHost = verify; }

protected:
    void addSSL(const URI &uri, boost::shared_ptr<Stream> &stream);

protected:
    bool m_verifySslCertificate, m_verifySslCertificateHost;
    unsigned long long m_httpReadTimeout, m_httpWriteTimeout, m_idleTimeout,
        m_sslReadTimeout, m_sslWriteTimeout;
    SSL_CTX * m_sslCtx;
    TimerManager *m_timerManager;
};

struct PriorConnectionFailedException : virtual Exception {};

// The ConnectionCache holds all connections associated with a RequestBroker.
// This is not a global cache of all connections - each RequestBroker instance
// will have its own.
//
// It permits a single RequestBroker to maintain multiple connections to
// multiple servers at the same time.  Connections are held active in this cache
// so that multiple requests can be performed over a single connection and
// the cache will take care of automatically reopening connections after they
// close when a new request arrives.
// It understands how to use proxies, but relies on the caller provided callback
// to determine the proxy rules for each request.
//
// Although exposed by createRequestBroker(), normal clients will not manipulate
// the ConnectionCache directly, apart from calling abortConnections or closeIdleConnections
class ConnectionCache : public boost::enable_shared_from_this<ConnectionCache>, public ConnectionBroker
{
public:
    typedef boost::shared_ptr<ConnectionCache> ptr;
    typedef boost::weak_ptr<ConnectionCache> weak_ptr;

protected:
    ConnectionCache(StreamBroker::ptr streamBroker, TimerManager *timerManager = NULL)
        : m_streamBroker(streamBroker),
          m_connectionsPerHost(1u),
          m_closed(false)
    {
        m_timerManager = timerManager;
    }

public:
    static ConnectionCache::ptr create(StreamBroker::ptr streamBroker, TimerManager *timerManager = NULL)
    { return ConnectionCache::ptr(new ConnectionCache(streamBroker, timerManager)); }

    // Specify the maximum number of seperate connections to allow to a specific host (or proxy)
    // at a time
    void connectionsPerHost(size_t connections) { m_connectionsPerHost = connections; }

    // Get number of active connections
    size_t getActiveConnections();

    // Proxy support requires this callback.  It is expected to return an
    // array of candidate Proxy servers to handle the requested URI.
    // If none are returned the request will be performed directly
    void proxyForURI(boost::function<std::vector<URI> (const URI &)> proxyForURIDg)
    { m_proxyForURIDg = proxyForURIDg; }

    // Required to support HTTPS proxies
    void proxyRequestBroker(boost::shared_ptr<RequestBroker> broker)
    { m_proxyBroker = broker; }

    // Get the connection associated with a URI.  An existing one may be reused,
    // or a new one established.
    std::pair<boost::shared_ptr<ClientConnection>, bool /*is proxy connection*/>
        getConnection(const URI &uri, bool forceNewConnection = false);

    void closeIdleConnections();

    // Cancel all connections, even the active ones.
    // Clients should expect OperationAbortedException, and PriorRequestFailedException
    // to be thrown if requests are active
    void abortConnections();

private:
    typedef std::list<boost::shared_ptr<ClientConnection> > ConnectionList;

    // Tracks active connections to a particular host
    // e.g. there might be 5 active connections to http://example.com
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

    // Table of active connections for each scheme+host
    // e.g. if a single RequestBroker is connected to two servers at the same time
    // or doing both http and https requests then this will contain multiple entries
    typedef std::map<URI, boost::shared_ptr<ConnectionInfo> > CachedConnectionMap;

private:
    std::pair<boost::shared_ptr<ClientConnection>, bool>
        getConnectionViaProxyFromCache(const URI &uri, const URI &proxy);
    std::pair<boost::shared_ptr<ClientConnection>, bool>
        getConnectionViaProxy(const URI &uri, const URI &proxy,
        FiberMutex::ScopedLock &lock);
    void cleanOutDeadConns(CachedConnectionMap &conns);
    void dropConnection(weak_ptr self, const URI &uri, const ClientConnection *connection);

private:
    FiberMutex m_mutex;
    StreamBroker::ptr m_streamBroker;
    size_t m_connectionsPerHost;

    CachedConnectionMap m_conns;
    bool m_closed;
    boost::function<std::vector<URI> (const URI &)> m_proxyForURIDg;
    boost::shared_ptr<RequestBroker> m_proxyBroker;
};

// The ConnectionNoCache has no support for proxies.
class ConnectionNoCache : public ConnectionBroker
{
public:
    typedef boost::shared_ptr<ConnectionNoCache> ptr;

    ConnectionNoCache(StreamBroker::ptr streamBroker, TimerManager *timerManager = NULL)
        : m_streamBroker(streamBroker)
    {
        m_timerManager = timerManager;
    }

    // Get a new connection associated with a URI. Do not cache it.
    std::pair<boost::shared_ptr<ClientConnection>, bool /*is proxy connection*/>
        getConnection(const URI &uri, bool forceNewConnection = false);

private:
    StreamBroker::ptr m_streamBroker;
};

// Mock object useful for unit tests.  Rather than
// making a real network call, each request will be
// processed directly by the provided callback
class MockConnectionBroker : public ConnectionBroker
{
private:
    typedef std::map<URI, boost::shared_ptr<ClientConnection> >
        ConnectionCache; // warning - not the same as class ConnectionCache
public:
    MockConnectionBroker(boost::function<void (const URI &uri,
            boost::shared_ptr<ServerRequest>)> dg,
        TimerManager *timerManager = NULL, unsigned long long readTimeout = ~0ull,
        unsigned long long writeTimeout = ~0ull)
        : m_dg(dg)
    {
        m_timerManager = timerManager;
    }

    std::pair<boost::shared_ptr<ClientConnection>, bool /*is proxy connection*/>
        getConnection(const URI &uri, bool forceNewConnection = false);

private:
    boost::function<void (const URI &uri, boost::shared_ptr<ServerRequest>)> m_dg;
    ConnectionCache m_conns;
};

// Abstract base-class for all the RequestBroker objects.
// RequestBrokers are typically instantiated by calling createRequestBroker().
// RequestBrokers abstract the actual network connection. Reusing a connection
// for multiple requests is achieved by reusing a RequestBroker.
class RequestBroker
{
public:
    typedef boost::shared_ptr<RequestBroker> ptr;
    typedef boost::weak_ptr<RequestBroker> weak_ptr;

public:
    virtual ~RequestBroker() {}

    // Perform a request.  The caller should prepare the requestHeaders
    // as much as they like, and the chain of RequestBrokerFilters will
    // also potentially make further adjustments
    //
    // Tip: Typically the full URI for the destination should be set in
    // requestHeaders.requestLine.uri, even though the scheme and authority are
    // not sent in the first line of the HTTP request. Also fill in
    // the requestHeaders.request.host.
    virtual boost::shared_ptr<ClientRequest> request(Request &requestHeaders,
        bool forceNewConnection = false,
        boost::function<void (boost::shared_ptr<ClientRequest>)> bodyDg = NULL)
        = 0;
};

// RequestBrokerFilter is the base class for filter request brokers,
// which exist in a chain leading to the BaseRequestBroker.
// Each derived class can implement special logic in the request()
// method and then call request on the next element in the chain (its parent).
// A filter can stop a request by throwing an exception.
class RequestBrokerFilter : public RequestBroker
{
public:
    // When created a RequestBrokerFilter is inserted at the
    // beginning of a chain of RequestBrokers, with the parent being the
    // next broker in the chain
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

// The BaseRequestBroker is the final broker in a chain of requestbrokers
// and sends the fully prepared request to its ConnectionBroker to be performed
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

// Takes care of filling in the User-Agent header
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

// When creating a new RequestBroker this structure is used to
// defines the configuration of a chain of requestBrokers
// and associated services.  The most common configuration options
// are exposed here, but further custom RequestBroker behavior
// can be achieved by directly creating RequestBrokerFilter objects
// and adding them to the chain
struct RequestBrokerOptions
{
    RequestBrokerOptions() :
        ioManager(NULL),
        scheduler(NULL),
        filterNetworksCB(NULL),
        handleRedirects(true),
        timerManager(NULL),
        connectTimeout(~0ull),
        sslConnectReadTimeout(~0ull),
        sslConnectWriteTimeout(~0ull),
        httpReadTimeout(~0ull),
        httpWriteTimeout(~0ull),
        idleTimeout(~0ull),
        connectionsPerHost(1u),
        sslCtx(NULL),
        verifySslCertificate(false),
        verifySslCertificateHost(true),
        enableConnectionCache(true)
    {}

    IOManager *ioManager;
    Scheduler *scheduler;

    // When specified a RetryRequestBroker will be installed.  If a request fails
    // the callback will be called with the current retry count. If the callback
    // returns false then no further retries are attempted.
    boost::function<bool (size_t /*retry count*/)> delayDg;

    // Callback to call directly before a socket connection happens.
    // Implementation should throw an exception if it wants to prevent the connection
    boost::function<void (boost::shared_ptr<Socket>)> filterNetworksCB;

    bool handleRedirects; // Whether to add a RedirectRequestBroker to the chain of RequestBrokers
    TimerManager *timerManager; // When not specified the iomanager will be used

    // Optional timeout values (us)
    unsigned long long connectTimeout;
    unsigned long long sslConnectReadTimeout;
    unsigned long long sslConnectWriteTimeout;
    unsigned long long httpReadTimeout;
    unsigned long long httpWriteTimeout;
    unsigned long long idleTimeout;
    size_t connectionsPerHost;

    // Callback to find proxy for an URI, see ConnectionCache::proxyForURI
    boost::function<std::vector<URI> (const URI &)> proxyForURIDg;

    /// Required to enable https proxy support
    RequestBroker::ptr proxyRequestBroker;

    // When specified these callbacks will be invoked to add authorization to the
    // request.  An alternative is to add the BasicAuth header before
    // calling RequestBroker::request (see HTTP::BasicAuth::authorize())
    boost::function<bool (const URI &,
            boost::shared_ptr<ClientRequest> /* priorRequest = ClientRequest::ptr() */,
            std::string & /* scheme */, std::string & /* realm */,
            std::string & /* username */, std::string & /* password */,
            size_t /* attempts */)>
            getCredentialsDg, getProxyCredentialsDg;

    // When specified the provided object will be installed as a filter
    // in front of the SocketStreamBroker
    StreamBrokerFilter::ptr customStreamBrokerFilter;

    SSL_CTX *sslCtx;
    bool verifySslCertificate;
    bool verifySslCertificateHost;
    bool enableConnectionCache;

    // When specified a UserAgentRequestBroker will take care of adding
    // the User-Agent header to each request
    ProductAndCommentList userAgent;
};

// Factory method to create a chain of request broker objects based on the
// the specified configuration options.  It also creates and returns a
// shared pointer to the request's new ConnectionCache.
// clients do not typically work directly with the ConnectionCache, except to
// shut down connections with ConnectionCache::abortConnections
std::pair<RequestBroker::ptr, ConnectionCache::ptr>
    createRequestBroker(const RequestBrokerOptions &options = RequestBrokerOptions());

/// @deprecated Use createRequestBroker instead
RequestBroker::ptr defaultRequestBroker(IOManager *ioManager = NULL,
                                        Scheduler *scheduler = NULL,
                                        ConnectionBroker::ptr *connBroker = NULL,
                                        boost::function<bool (size_t)> delayDg = NULL);
}}

#endif
