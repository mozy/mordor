// Copyright (c) 2009 - Mozy, Inc.

#include "broker.h"

#include "auth.h"
#include "client.h"
#include "mordor/atomic.h"
#include "mordor/fiber.h"
#include "mordor/future.h"
#include "mordor/iomanager.h"
#include "mordor/log.h"
#include "mordor/socks.h"
#include "mordor/streams/buffered.h"
#include "mordor/streams/pipe.h"
#include "mordor/streams/socket.h"
#include "mordor/streams/ssl.h"
#include "mordor/streams/timeout.h"
#include "proxy.h"
#include "server.h"

namespace Mordor {
namespace HTTP {

std::pair<RequestBroker::ptr, ConnectionCache::ptr>
createRequestBroker(const RequestBrokerOptions &options)
{
    TimerManager *timerManager = const_cast<TimerManager*>(options.timerManager);
    if (options.ioManager && !timerManager)
        timerManager = const_cast<IOManager*>(options.ioManager);

    SocketStreamBroker::ptr socketBroker(new SocketStreamBroker(options.ioManager,
        options.scheduler));
    socketBroker->connectTimeout(options.connectTimeout);
    socketBroker->networkFilterCallback(options.filterNetworksCB);

    StreamBroker::ptr streamBroker = socketBroker;
    if (options.customStreamBrokerFilter) {
        options.customStreamBrokerFilter->parent(streamBroker);
        streamBroker = options.customStreamBrokerFilter;
    }

    ConnectionCache::ptr connectionCache(new ConnectionCache(streamBroker,
        timerManager));
    connectionCache->httpReadTimeout(options.httpReadTimeout);
    connectionCache->httpWriteTimeout(options.httpWriteTimeout);
    connectionCache->idleTimeout(options.idleTimeout);
    connectionCache->sslReadTimeout(options.sslConnectReadTimeout);
    connectionCache->sslWriteTimeout(options.sslConnectWriteTimeout);
    connectionCache->sslCtx(options.sslCtx);
    connectionCache->proxyForURI(options.proxyForURIDg);
    connectionCache->proxyRequestBroker(options.proxyRequestBroker);
    connectionCache->verifySslCertificate(options.verifySslCertificate);
    connectionCache->verifySslCertificateHost(options.verifySslCertificateHost);

    RequestBroker::ptr requestBroker(new BaseRequestBroker(
        boost::static_pointer_cast<ConnectionBroker>(connectionCache)));

    if (options.getCredentialsDg || options.getProxyCredentialsDg)
        requestBroker.reset(new AuthRequestBroker(requestBroker,
            options.getCredentialsDg, options.getProxyCredentialsDg));
    if (options.handleRedirects)
        requestBroker.reset(new RedirectRequestBroker(requestBroker));
    if (options.delayDg)
        requestBroker.reset(new RetryRequestBroker(requestBroker,
        options.delayDg));
    if (!options.userAgent.empty())
        requestBroker.reset(new UserAgentRequestBroker(requestBroker,
            options.userAgent));
    return std::make_pair(requestBroker, connectionCache);
}

RequestBroker::ptr defaultRequestBroker(IOManager *ioManager,
                                        Scheduler *scheduler,
                                        ConnectionBroker::ptr *connBroker,
                                        boost::function<bool (size_t)> delayDg)
{
   RequestBrokerOptions options;
   options.ioManager = ioManager;
   options.scheduler = scheduler;
   options.delayDg = delayDg;
   std::pair<RequestBroker::ptr, ConnectionCache::ptr> result = createRequestBroker(options);
   if (connBroker)
       *connBroker = result.second;
   return result.first;
}


StreamBroker::ptr
StreamBrokerFilter::parent()
{
    if (m_parent)
        return m_parent;
    return StreamBroker::ptr(m_weakParent);
}

Stream::ptr
SocketStreamBroker::getStream(const URI &uri)
{
    if (m_cancelled)
        MORDOR_THROW_EXCEPTION(OperationAbortedException());

    MORDOR_ASSERT(uri.authority.hostDefined());
    MORDOR_ASSERT(uri.authority.portDefined() || uri.schemeDefined());
    std::ostringstream os;
    os << uri.authority.host();
    if (uri.authority.portDefined())
        os << ":" << uri.authority.port();
    else if (uri.schemeDefined())
        os << ":" << uri.scheme();
    std::vector<Address::ptr> addresses;
    {
        SchedulerSwitcher switcher(m_scheduler);
        addresses = Address::lookup(os.str());
    }
    Socket::ptr socket;
    for (std::vector<Address::ptr>::const_iterator it(addresses.begin());
        it != addresses.end();
        )
    {
        if (m_ioManager)
            socket = (*it)->createSocket(*m_ioManager, SOCK_STREAM);
        else
            socket = (*it)->createSocket(SOCK_STREAM);
        std::list<Socket::ptr>::iterator it2;
        {
            boost::mutex::scoped_lock lock(m_mutex);
            if (m_cancelled)
                MORDOR_THROW_EXCEPTION(OperationAbortedException());
            m_pending.push_back(socket);
            it2 = m_pending.end();
            --it2;
        }
        socket->sendTimeout(m_connectTimeout);
        try {
            // if we are filtering network connections, the callback will bind
            // the socket to an approved network address
            if (m_filterNetworkCallback != NULL)
            {
                // the callback function is responsible for exiting out by throwing
                // an exception.
                m_filterNetworkCallback(socket);
            }
            socket->connect(*it);
            boost::mutex::scoped_lock lock(m_mutex);
            m_pending.erase(it2);
            break;
        } catch (...) {
            boost::mutex::scoped_lock lock(m_mutex);
            m_pending.erase(it2);
            if (++it == addresses.end())
                throw;
        }
        socket->sendTimeout(~0ull);
    }
    Stream::ptr stream(new SocketStream(socket));
    return stream;
}

void
SocketStreamBroker::cancelPending()
{
    boost::mutex::scoped_lock lock(m_mutex);
    m_cancelled = true;
    for (std::list<Socket::ptr>::iterator it(m_pending.begin());
        it != m_pending.end();
        ++it) {
        (*it)->cancelConnect();
        (*it)->cancelSend();
        (*it)->cancelReceive();
    }
}

static bool least(const ClientConnection::ptr &lhs,
                  const ClientConnection::ptr &rhs)
{
    if (lhs && rhs)
        return lhs->outstandingRequests() <
            rhs->outstandingRequests();
    if (!lhs)
        return false;
    if (!rhs)
        return true;
    MORDOR_NOTREACHED();
}

static Logger::ptr g_cacheLog = Log::lookup("mordor:http:connectioncache");

std::pair<ClientConnection::ptr, bool>
ConnectionCache::getConnection(const URI &uri, bool forceNewConnection)
{
    std::vector<URI> proxies;
    if (m_proxyForURIDg)
        proxies = m_proxyForURIDg(uri);
    // Remove proxy types that aren't supported
    for (std::vector<URI>::iterator it(proxies.begin());
        it != proxies.end();
        ++it) {
        MORDOR_ASSERT(it->schemeDefined() || !it->isDefined());
        if (!it->schemeDefined())
            continue;
        std::string scheme = it->scheme();
        if (scheme != "http" && (scheme != "https" || !m_proxyBroker) &&
            scheme != "socks")
            it = proxies.erase(it, it);
    }
    URI schemeAndAuthority;
    schemeAndAuthority = uri;
    schemeAndAuthority.path = URI::Path();
    schemeAndAuthority.queryDefined(false);
    schemeAndAuthority.fragmentDefined(false);
    std::pair<ClientConnection::ptr, bool> result;

    FiberMutex::ScopedLock lock(m_mutex);

    if (g_cacheLog->enabled(Log::DEBUG)) {
        std::ostringstream os;
        os << this << " getting connection for " << schemeAndAuthority
        << ", proxies: {";
        for (std::vector<URI>::iterator it(proxies.begin());
             it != proxies.end();
             ++it) {
            if (it != proxies.begin())
                os << ", ";
            os << *it;
        }
        os << "}";
        MORDOR_LOG_DEBUG(g_cacheLog) << os.str();
    }

    if (m_closed)
        MORDOR_THROW_EXCEPTION(OperationAbortedException());
    // Clean out any dead conns
    cleanOutDeadConns(m_conns);

    if (!forceNewConnection) {
        if (proxies.empty()) {
            result = getConnectionViaProxyFromCache(schemeAndAuthority, URI());
            if (result.first)
                return result;
        }
        for (std::vector<URI>::const_iterator it(proxies.begin());
            it != proxies.end();
            ++it) {
            result = getConnectionViaProxyFromCache(schemeAndAuthority, *it);
            if (result.first)
                return result;
        }
    }

    // Create a new connection
    if (proxies.empty())
        return getConnectionViaProxy(schemeAndAuthority, URI(), lock);
    std::vector<URI>::const_iterator it = proxies.begin();
    while(true) {
        try {
            return getConnectionViaProxy(schemeAndAuthority, *it, lock);
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

std::pair<ClientConnection::ptr, bool>
ConnectionCache::getConnectionViaProxyFromCache(const URI &uri, const URI &proxy)
{
    // Check if an existing connection exists to the requested URI
    // that should be reused.
    // When proxy is specified this looks for a connection
    // to the proxy uri instead

    bool proxied = proxy.schemeDefined() && proxy.scheme() == "http";
    const URI &endpoint = proxied ? proxy : uri;
    CachedConnectionMap::iterator it = m_conns.find(endpoint);
    ConnectionList::iterator it2;
    while (true) {
        if (it != m_conns.end() &&
            !it->second->connections.empty() &&
            it->second->connections.size() >= m_connectionsPerHost) {
            boost::shared_ptr<ConnectionInfo> info = it->second;
            ConnectionList &connsForThisUri = info->connections;
            // Assign it2 to point to the connection with the
            // least number of outstanding requests
            it2 = std::min_element(connsForThisUri.begin(),
                connsForThisUri.end(), &least);
            // No connection has completed yet (but it's in progress)
            if (!*it2) {
                MORDOR_LOG_TRACE(g_cacheLog) << this << " waiting for connection to "
                    << endpoint;
                // Wait for somebody to let us try again
                unsigned long long start = TimerManager::now();
                info->condition.wait();
                if (info->lastFailedConnectionTimestamp <= start)
                    MORDOR_THROW_EXCEPTION(PriorConnectionFailedException());
                if (m_closed)
                    MORDOR_THROW_EXCEPTION(OperationAbortedException());
                // We let go of the mutex, and the last connection may have
                // disappeared
                it = m_conns.find(endpoint);
            } else {
                MORDOR_LOG_TRACE(g_cacheLog) << this << " returning cached connection "
                    << *it2 << " to " << endpoint;
                // Return the existing, completed connection
                return std::make_pair(*it2, proxied);
            }
        } else {
            // No existing connections
            return std::make_pair(ClientConnection::ptr(), false);
        }
    }
}

std::pair<ClientConnection::ptr, bool>
ConnectionCache::getConnectionViaProxy(const URI &uri, const URI &proxy,
    FiberMutex::ScopedLock &lock)
{
    // Create a new Connection to the requested URI, using
    // the proxy if specified

    std::string proxyScheme;
    if (proxy.schemeDefined())
        proxyScheme = proxy.scheme();
    bool proxied = proxyScheme == "http";
    const URI &endpoint = proxied ? proxy : uri;

    // Make sure we have a ConnectionList and mutex for this endpoint
    CachedConnectionMap::iterator it = m_conns.find(endpoint);
    boost::shared_ptr<ConnectionInfo> info;
    if (it == m_conns.end()) {
        info.reset(new ConnectionInfo(m_mutex));
        it = m_conns.insert(std::make_pair(endpoint, info)).first;
    } else {
        info = it->second;
    }
    // Add a placeholder for the new connection
    info->connections.push_back(ClientConnection::ptr());

    MORDOR_LOG_TRACE(g_cacheLog) << this << " establishing connection to "
        << endpoint;
    unsigned long long start = TimerManager::now();
    lock.unlock();

    ConnectionList::iterator it2;
    std::pair<ClientConnection::ptr, bool> result;
    // Establish a new connection
    try {
        Stream::ptr stream;
        if (proxyScheme == "https") {
            stream = tunnel(m_proxyBroker, proxy, uri);
        } else if (proxyScheme == "socks") {
            unsigned short port;
            if (uri.authority.portDefined())
                port = uri.authority.port();
            else if (uri.scheme() == "http")
                port = 80;
            else if (uri.scheme() == "https")
                port = 443;
            else
                // TODO: can this be looked up using the system? (getaddrinfo)
                MORDOR_THROW_EXCEPTION(std::invalid_argument("Unknown protocol for proxying connection"));
            stream = SOCKS::tunnel(m_streamBroker, proxy, IPAddress::ptr(),
                uri.authority.host(), port);
        } else {
            stream = m_streamBroker->getStream(endpoint);
        }
        addSSL(endpoint, stream);
        lock.lock();
        // Somebody called abortConnections while we were unlocked; just throw
        // this connection away
        if (m_closed)
            MORDOR_THROW_EXCEPTION(OperationAbortedException());
        result = std::make_pair(ClientConnection::ptr(
            new ClientConnection(stream, m_timerManager)), proxied);
        MORDOR_LOG_TRACE(g_cacheLog) << this << " connection " << result.first
            << " to " << endpoint << " established";
        stream->onRemoteClose(boost::bind(&ConnectionCache::dropConnection,
            this, endpoint, result.first.get()));
        if (m_httpReadTimeout != ~0ull)
            result.first->readTimeout(m_httpReadTimeout);
        if (m_httpWriteTimeout != ~0ull)
            result.first->writeTimeout(m_httpWriteTimeout);
        if (m_idleTimeout != ~0ull)
            result.first->idleTimeout(m_idleTimeout,
            boost::bind(&ConnectionCache::dropConnection, this, endpoint,
                result.first.get()));
        // Assign this connection to the first blank connection for this
        // schemeAndAuthority
        for (it2 = info->connections.begin();
            it2 != info->connections.end();
            ++it2) {
            if (!*it2) {
                *it2 = result.first;
                break;
            }
        }
        // We should have assigned this connection *somewhere*
        MORDOR_ASSERT(it2 != info->connections.end());
        // Unblock all waiters for them to choose an existing connection
        info->condition.broadcast();
    } catch (...) {
        lock.lock();
        MORDOR_LOG_TRACE(g_cacheLog) << this << " connection to " << endpoint
            << " failed: " << boost::current_exception_diagnostic_information();
        // Somebody called abortConnections while we were unlocked; no need to
        // clean up the temporary spot for this connection, since it's gone;
        // pass the original exception on, though
        if (m_closed)
            throw;
        // This connection attempt failed; remove the first blank connection
        // for this schemeAndAuthority to let someone else try to establish a
        // connection
        // it should still be valid, even if the map changed
        for (it2 = info->connections.begin();
            it2 != info->connections.end();
            ++it2) {
            if (!*it2) {
                info->connections.erase(it2);
                break;
            }
        }
        info->lastFailedConnectionTimestamp = start;
        info->condition.broadcast();
        if (info->connections.empty())
            m_conns.erase(it);
        throw;
    }
    return result;
}

void
ConnectionCache::closeIdleConnections()
{
    FiberMutex::ScopedLock lock(m_mutex);
    MORDOR_LOG_DEBUG(g_cacheLog) << " dropping idle connections";
    // We don't just clear the list, because there may be a connection in
    // progress that has an iterator into it
    CachedConnectionMap::iterator it, extraIt;
    for (it = m_conns.begin(); it != m_conns.end();) {
        it->second->condition.broadcast();
        for (ConnectionList::iterator it2 = it->second->connections.begin();
            it2 != it->second->connections.end();) {
            if (*it2) {
                Stream::ptr connStream = (*it2)->stream();
                connStream->cancelRead();
                connStream->cancelWrite();
                if (m_idleTimeout != ~0ull)
                    (*it2)->idleTimeout(~0ull, NULL);
                it2 = it->second->connections.erase(it2);
            } else {
                ++it2;
            }
        }
        if (it->second->connections.empty()) {
            extraIt = it;
            ++it;
            m_conns.erase(extraIt);
        } else {
            ++it;
        }
    }
}

void
ConnectionCache::abortConnections()
{
    FiberMutex::ScopedLock lock(m_mutex);
    MORDOR_LOG_DEBUG(g_cacheLog) << " aborting all connections";
    m_closed = true;
    CachedConnectionMap::iterator it;
    for (it = m_conns.begin(); it != m_conns.end(); ++it) {
        it->second->condition.broadcast();
        for (ConnectionList::iterator it2 = it->second->connections.begin();
            it2 != it->second->connections.end();
            ++it2) {
            if (*it2) {
                Stream::ptr connStream = (*it2)->stream();
                connStream->cancelRead();
                connStream->cancelWrite();
                if (m_idleTimeout != ~0ull)
                    (*it2)->idleTimeout(~0ull, NULL);
            }
        }
    }
    m_conns.clear();
    lock.unlock();
    m_streamBroker->cancelPending();
}

void
ConnectionCache::cleanOutDeadConns(CachedConnectionMap &conns)
{
    CachedConnectionMap::iterator it, it3;
    ConnectionList::iterator it2;
    for (it = conns.begin(); it != conns.end();) {
        for (it2 = it->second->connections.begin();
            it2 != it->second->connections.end();) {
            if (*it2 && !(*it2)->newRequestsAllowed()) {
                if (m_idleTimeout != ~0ull)
                    (*it2)->idleTimeout(~0ull, NULL);
                it2 = it->second->connections.erase(it2);
            } else {
                ++it2;
            }
        }
        if (it->second->connections.empty()) {
            it3 = it;
            ++it3;
            conns.erase(it);
            it = it3;
        } else {
            ++it;
        }
    }
}

void
ConnectionCache::addSSL(const URI &uri, Stream::ptr &stream)
{
    if (uri.schemeDefined() && uri.scheme() == "https") {
        TimeoutStream::ptr timeoutStream;
        if (m_timerManager) {
            timeoutStream.reset(new TimeoutStream(stream, *m_timerManager));
            timeoutStream->readTimeout(m_sslReadTimeout);
            timeoutStream->writeTimeout(m_sslWriteTimeout);
            stream = timeoutStream;
        }
        BufferedStream::ptr bufferedStream(new BufferedStream(stream));
        bufferedStream->allowPartialReads(true);
        SSLStream::ptr sslStream(new SSLStream(bufferedStream, true, true, m_sslCtx));
        sslStream->serverNameIndication(uri.authority.host());
        sslStream->connect();
        if (m_verifySslCertificate)
            sslStream->verifyPeerCertificate();
        if (m_verifySslCertificateHost)
            sslStream->verifyPeerCertificate(uri.authority.host());
        if (timeoutStream) {
            bufferedStream->parent(timeoutStream->parent());
            timeoutStream.reset();
        }
        bufferedStream.reset(new BufferedStream(sslStream));
        // Max data in each SSL record
        bufferedStream->bufferSize(16384);
        bufferedStream->flushMultiplesOfBuffer(true);
        bufferedStream->allowPartialReads(true);
        stream = bufferedStream;
    }
}

namespace {
struct CompareConn
{
    CompareConn(const ClientConnection *conn)
        : m_conn(conn)
    {}

    bool operator()(const ClientConnection::ptr &lhs) const
    {
        return lhs.get() == m_conn;
    }

    const ClientConnection *m_conn;
};
}

void
ConnectionCache::dropConnection(const URI &uri,
    const ClientConnection *connection)
{
    FiberMutex::ScopedLock lock(m_mutex);
    CachedConnectionMap::iterator it = m_conns.find(uri);
    if (it == m_conns.end())
        return;
    ConnectionList::iterator it2 = std::find_if(it->second->connections.begin(),
        it->second->connections.end(), CompareConn(connection));
    if (it2 != it->second->connections.end()) {
        MORDOR_LOG_TRACE(g_cacheLog) << this << " dropping connection "
            << connection << " to " << uri;
        if (m_idleTimeout != ~0ull)
            (*it2)->idleTimeout(~0ull, NULL);
        it->second->connections.erase(it2);
        if (it->second->connections.empty())
            m_conns.erase(it);
    }
}

std::pair<ClientConnection::ptr, bool>
MockConnectionBroker::getConnection(const URI &uri, bool forceNewConnection)
{
    URI schemeAndAuthority = uri;
    schemeAndAuthority.path = URI::Path();
    schemeAndAuthority.queryDefined(false);
    schemeAndAuthority.fragmentDefined(false);
    ConnectionCache::iterator it = m_conns.find(schemeAndAuthority);
    if (it != m_conns.end() && !it->second->newRequestsAllowed()) {
        m_conns.erase(it);
        it = m_conns.end();
    }
    if (it == m_conns.end()) {
        std::pair<Stream::ptr, Stream::ptr> pipes = pipeStream();
        ClientConnection::ptr client(
            new ClientConnection(pipes.first, m_timerManager));
        if (m_timerManager) {
            client->readTimeout(m_readTimeout);
            client->writeTimeout(m_writeTimeout);
        }
        ServerConnection::ptr server(
            new ServerConnection(pipes.second, boost::bind(m_dg,
                schemeAndAuthority, _1)));
        Scheduler::getThis()->schedule(Fiber::ptr(new Fiber(boost::bind(
            &ServerConnection::processRequests, server))));
        m_conns[schemeAndAuthority] = client;
        return std::make_pair(client, false);
    }
    return std::make_pair(it->second, false);
}

RequestBroker::ptr
RequestBrokerFilter::parent()
{
    if (m_parent)
        return m_parent;
    return RequestBroker::ptr(m_weakParent);
}

static void doBody(ClientRequest::ptr request,
    boost::function<void (ClientRequest::ptr)> bodyDg,
    Future<> &future,
    boost::exception_ptr &exception, bool &exceptionWasHttp)
{
    exceptionWasHttp = false;
    try {
        bodyDg(request);
    } catch (boost::exception &ex) {
        exceptionWasHttp = request->requestState() == ClientRequest::ERROR;
        if (exceptionWasHttp)
            ex << errinfo_source(HTTP);
        // Make sure the request is fully aborted so we don't hang waiting for
        // a response (since the request object is still in scope by the
        // caller, it won't do this automatically)
        request->cancel();
        exception = boost::current_exception();
    } catch (...) {
        exceptionWasHttp = request->requestState() == ClientRequest::ERROR;
        // Make sure the request is fully aborted so we don't hang waiting for
        // a response (since the request object is still in scope by the
        // caller, it won't do this automatically)
        request->cancel();
        exception = boost::current_exception();
    }
    future.signal();
}

ClientRequest::ptr
BaseRequestBroker::request(Request &requestHeaders, bool forceNewConnection,
                           boost::function<void (ClientRequest::ptr)> bodyDg)
{
    URI &currentUri = requestHeaders.requestLine.uri;
    URI originalUri = currentUri;
    bool connect = requestHeaders.requestLine.method == CONNECT;
    MORDOR_ASSERT(originalUri.authority.hostDefined());
    if (!connect) {
        requestHeaders.request.host = originalUri.authority.host();
    } else {
        MORDOR_ASSERT(originalUri.scheme() == "http");
        MORDOR_ASSERT(originalUri.path.segments.size() == 2);
        currentUri = URI();
        currentUri.authority = originalUri.path.segments[1];
    }
    ConnectionBroker::ptr connectionBroker = m_connectionBroker;
    if (!connectionBroker)
        connectionBroker = m_weakConnectionBroker.lock();
    std::pair<ClientConnection::ptr, bool> conn;
    try {
        conn = connectionBroker->getConnection(
            connect ? originalUri : currentUri, forceNewConnection);
    } catch (boost::exception &ex) {
        currentUri = originalUri;
        ex << errinfo_source(CONNECTION);
        throw;
    } catch (...) {
        currentUri = originalUri;
        throw;
    }
    // We use an absolute URI if we're talking to a proxy, or if the path
    // starts with "//" (path_absolute does not allow an empty first segment;
    // path_abempty as part of absolute_URI does); otherwise use only the path
    // (and query)
    if (!connect && !conn.second && !(originalUri.path.isAbsolute() &&
        originalUri.path.segments.size() > 2 &&
        originalUri.path.segments[1].empty())) {
        currentUri.schemeDefined(false);
        currentUri.authority.hostDefined(false);
    }
    try {
        ClientRequest::ptr request;
        try {
            request = conn.first->request(requestHeaders);
            if (!bodyDg)
                request->doRequest();
        } catch (boost::exception &ex) {
            ex << errinfo_source(HTTP);
            throw;
        }
        Future<> future;
        boost::exception_ptr exception;
        bool exceptionWasHttp = false;
        if (bodyDg)
            Scheduler::getThis()->schedule(boost::bind(&doBody,
                request, bodyDg, boost::ref(future), boost::ref(exception),
                boost::ref(exceptionWasHttp)));
        currentUri = originalUri;
        try {
            // Force reading the response here to check for connectivity problems
            request->response();
        } catch (boost::exception &ex) {
            ex << errinfo_source(HTTP);
            if (bodyDg)
                future.wait();
            // Prefer to throw an exception from send, if there was one
            if (exception)
                Mordor::rethrow_exception(exception);
            throw;
        } catch (...) {
            if (bodyDg)
                future.wait();
            // Prefer to throw an exception from send, if there was one
            if (exception)
                Mordor::rethrow_exception(exception);
            throw;
        }
        if (bodyDg)
            future.wait();
        // Rethrow any exception from bodyDg *if* it didn't come from the
        // HTTP framework, or directly from a stream within the framework
        // (i.e. an exception from the user's code)
        // otherwise, ignore it - we have a response, and we don't really
        // care that the request didn't complete
        if (exception && !exceptionWasHttp)
            Mordor::rethrow_exception(exception);
        return request;
    } catch (...) {
        currentUri = originalUri;
        throw;
    }
}

ClientRequest::ptr
RetryRequestBroker::request(Request &requestHeaders, bool forceNewConnection,
                           boost::function<void (ClientRequest::ptr)> bodyDg)
{
    size_t localRetries = 0;
    size_t *retries = mp_retries ? mp_retries : &localRetries;
    while (true) {
        try {
            ClientRequest::ptr request =
                parent()->request(requestHeaders, forceNewConnection, bodyDg);
            // Successful request resets shared retry counter
            *retries = 0;
            return request;
        } catch (SocketException &ex) {
            const ExceptionSource *source = boost::get_error_info<errinfo_source>(ex);
            if (!source || (*source != HTTP && *source != CONNECTION))
                throw;
            if (m_delayDg && !m_delayDg(atomicIncrement(*retries)))
                throw;
            continue;
        } catch (PriorRequestFailedException &ex) {
            const ExceptionSource *source = boost::get_error_info<errinfo_source>(ex);
            if (!source || *source != HTTP)
                throw;
            if (m_delayDg && !m_delayDg(*retries + 1))
                throw;
            continue;
        } catch (PriorConnectionFailedException &ex) {
            const ExceptionSource *source = boost::get_error_info<errinfo_source>(ex);
            if (!source || (*source != HTTP && *source != CONNECTION))
                throw;
            if (m_delayDg && !m_delayDg(*retries + 1))
                throw;
            continue;
        } catch (UnexpectedEofException &ex) {
            const ExceptionSource *source = boost::get_error_info<errinfo_source>(ex);
            if (!source || *source != HTTP)
                throw;
            if (m_delayDg && !m_delayDg(atomicIncrement(*retries)))
                throw;
            continue;
        }
        MORDOR_NOTREACHED();
    }
}

ClientRequest::ptr
RedirectRequestBroker::request(Request &requestHeaders, bool forceNewConnection,
                               boost::function<void (ClientRequest::ptr)> bodyDg)
{
    URI &currentUri = requestHeaders.requestLine.uri;
    URI originalUri = currentUri;
    std::list<URI> uris;
    uris.push_back(originalUri);
    size_t redirects = 0;
    while (true) {
        try {
            ClientRequest::ptr request = parent()->request(requestHeaders,
                forceNewConnection, bodyDg);
            bool handleRedirect = false;
            switch (request->response().status.status)
            {
            case FOUND:
                handleRedirect = m_handle302;
                break;
            case TEMPORARY_REDIRECT:
                handleRedirect = m_handle307;
                break;
            case MOVED_PERMANENTLY:
                handleRedirect = m_handle301;
                break;
            default:
                currentUri = originalUri;
                return request;
            }
            if (handleRedirect) {
                if (++redirects == m_maxRedirects)
                    MORDOR_THROW_EXCEPTION(CircularRedirectException(originalUri));
                currentUri = URI::transform(currentUri,
                    request->response().response.location);
                if (std::find(uris.begin(), uris.end(), currentUri)
                    != uris.end())
                    MORDOR_THROW_EXCEPTION(CircularRedirectException(originalUri));
                uris.push_back(currentUri);
                if (request->response().status.status == MOVED_PERMANENTLY)
                    originalUri = currentUri;
                request->finish();
                continue;
            } else {
                currentUri = originalUri;
                return request;
            }
        } catch (...) {
            currentUri = originalUri;
            throw;
        }
        MORDOR_NOTREACHED();
    }
}

ClientRequest::ptr
UserAgentRequestBroker::request(Request &requestHeaders, bool forceNewConnection,
                               boost::function<void (ClientRequest::ptr)> bodyDg)
{
    requestHeaders.request.userAgent = m_userAgent;
    return parent()->request(requestHeaders, forceNewConnection, bodyDg);
}

}}
