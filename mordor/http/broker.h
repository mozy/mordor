#ifndef __HTTP_BROKER_H__
#define __HTTP_BROKER_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <openssl/ssl.h>

#include "client.h"
#include "mordor/scheduler.h"
#include "server.h"

namespace Mordor {
namespace HTTP {

class StreamBroker
{
public:
    typedef boost::shared_ptr<StreamBroker> ptr;
    typedef boost::weak_ptr<StreamBroker> weak_ptr;

public:
    virtual ~StreamBroker() {}

    virtual Stream::ptr getStream(const URI &uri) = 0;
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
    SocketStreamBroker(IOManager *ioManager = NULL, Scheduler *scheduler = NULL)
        : connectTimeout(~0ull),
          sendTimeout(~0ull),
          receiveTimeout(~0ull),
          m_cancelled(false),
          m_ioManager(ioManager),
          m_scheduler(scheduler)
    {}

    Stream::ptr getStream(const URI &uri);
    void cancelPending();

    unsigned long long connectTimeout, sendTimeout, receiveTimeout;

private:
    boost::mutex m_mutex;
    bool m_cancelled;
    std::list<Socket::ptr> m_pending;
    IOManager *m_ioManager;
    Scheduler *m_scheduler;
};

class SSLStreamBroker : public StreamBrokerFilter
{
public:
    SSLStreamBroker(StreamBroker::ptr parent,
        SSL_CTX *sslCtx = NULL, bool verifySslCert = false,
        bool verifySslCertHost = false)
        : StreamBrokerFilter(parent),
          m_sslCtx(sslCtx),
          m_verifySslCert(verifySslCert),
          m_verifySslCertHost(verifySslCertHost)
    {}

    Stream::ptr getStream(const URI &uri);

private:
    SSL_CTX *m_sslCtx;
    bool m_verifySslCert, m_verifySslCertHost;
};

class ConnectionBroker
{
public:
    typedef boost::shared_ptr<ConnectionBroker> ptr;
    typedef boost::weak_ptr<ConnectionBroker> weak_ptr;

public:
    virtual ~ConnectionBroker() {}

    virtual std::pair<ClientConnection::ptr, bool>
        getConnection(const URI &uri, bool forceNewConnection = false) = 0;
};

class ConnectionCache : public ConnectionBroker
{
public:
    typedef boost::shared_ptr<ConnectionCache> ptr;

public:
    ConnectionCache(StreamBroker::ptr streamBroker,
        size_t connectionsPerHost = 1)
        : m_streamBroker(streamBroker),
          m_connectionsPerHost(connectionsPerHost),
          m_closed(false)
    {}

    std::pair<ClientConnection::ptr, bool>
        getConnection(const URI &uri, bool forceNewConnection = false);

    void closeConnections();

private:
    FiberMutex m_mutex;
    StreamBroker::ptr m_streamBroker;
    size_t m_connectionsPerHost;

    typedef std::list<ClientConnection::ptr> ConnectionList;
    std::map<URI, std::pair<ConnectionList, boost::shared_ptr<FiberCondition> > > m_conns;
    bool m_closed;
};

class MockConnectionBroker : public ConnectionBroker
{
private:
    typedef std::map<URI,
        std::pair<ClientConnection::ptr, ServerConnection::ptr> >
        ConnectionCache;
public:
    MockConnectionBroker(boost::function<void (const URI &uri, ServerRequest::ptr)> dg,
        TimerManager *timerManager = NULL, unsigned long long readTimeout = ~0ull,
        unsigned long long writeTimeout = ~0ull)
        : m_dg(dg),
          m_timerManager(timerManager),
          m_readTimeout(readTimeout),
          m_writeTimeout(writeTimeout)
    {}

    std::pair<ClientConnection::ptr, bool>
        getConnection(const URI &uri, bool forceNewConnection = false);

private:
    boost::function<void (const URI &uri, ServerRequest::ptr)> m_dg;
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

    virtual ClientRequest::ptr request(Request &requestHeaders,
        bool forceNewConnection = false,
        boost::function<void (ClientRequest::ptr)> bodyDg = NULL) = 0;
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

    ClientRequest::ptr request(Request &requestHeaders,
        bool forceNewConnection = false,
        boost::function<void (ClientRequest::ptr)> bodyDg = NULL) = 0;

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

    ClientRequest::ptr request(Request &requestHeaders,
        bool forceNewConnection = false,
        boost::function<void (ClientRequest::ptr)> bodyDg = NULL);

private:
    ConnectionBroker::ptr m_connectionBroker;
    ConnectionBroker::weak_ptr m_weakConnectionBroker;
};

/// Retries connection error and PriorRequestFailed errors
class RetryRequestBroker : public RequestBrokerFilter
{
public:
    typedef boost::shared_ptr<BaseRequestBroker> ptr;

public:
    RetryRequestBroker(RequestBroker::ptr parent,
        boost::function<bool (size_t)> delayDg = NULL)
        : RequestBrokerFilter(parent),
          m_delayDg(delayDg),
          mp_retries(NULL)
    {}

    void sharedRetryCounter(size_t *retries) { mp_retries = retries; }

    ClientRequest::ptr request(Request &requestHeaders,
        bool forceNewConnection = false,
        boost::function<void (ClientRequest::ptr)> bodyDg = NULL);

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

    ClientRequest::ptr request(Request &requestHeaders,
        bool forceNewConnection = false,
        boost::function<void (ClientRequest::ptr)> bodyDg = NULL);

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

    ClientRequest::ptr request(Request &requestHeaders,
        bool forceNewConnection = false,
        boost::function<void (ClientRequest::ptr)> bodyDg = NULL);

private:
    ProductAndCommentList m_userAgent;
};

struct RequestBrokerOptions
{
    RequestBrokerOptions() : ioManager(NULL), scheduler(NULL), handleRedirects(true) {}

    IOManager *ioManager;
    Scheduler *scheduler;
    boost::function<bool (size_t)> delayDg;
    bool handleRedirects;
    boost::function<URI (const URI &)> proxyForURIDg;
    boost::function<bool (const URI &,
            ClientRequest::ptr /* priorRequest = ClientRequest::ptr() */,
            std::string & /* scheme */, std::string & /* realm */,
            std::string & /* username */, std::string & /* password */,
            size_t /* attempts */)>
            getCredentialsDg, getProxyCredentialsDg;
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
