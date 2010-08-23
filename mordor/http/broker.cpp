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
    TimerManager *timerManager = options.timerManager;
    if (options.ioManager && !timerManager)
        timerManager = options.ioManager;

    SocketStreamBroker::ptr socketBroker(new SocketStreamBroker(options.ioManager,
        options.scheduler));
    socketBroker->connectTimeout(options.connectTimeout);

    StreamBroker::ptr streamBroker = socketBroker;
    if (options.customStreamBrokerFilter) {
        options.customStreamBrokerFilter->parent(streamBroker);
        streamBroker = options.customStreamBrokerFilter;
    }

    ConnectionCache::ptr connectionCache(new ConnectionCache(streamBroker,
        timerManager));
    connectionCache->httpReadTimeout(options.httpReadTimeout);
    connectionCache->httpWriteTimeout(options.httpWriteTimeout);
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
        addresses = Address::lookup(os.str(), AF_UNSPEC, SOCK_STREAM);
    }
    Socket::ptr socket;
    for (std::vector<Address::ptr>::const_iterator it(addresses.begin());
        it != addresses.end();
        ) {
        if (m_ioManager)
            socket = (*it)->createSocket(*m_ioManager);
        else
            socket = (*it)->createSocket();
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
    bool proxied = proxy.schemeDefined() && proxy.scheme() == "http";
    const URI &endpoint = proxied ? proxy : uri;
    CachedConnectionMap::iterator it = m_conns.find(endpoint);
    ConnectionList::iterator it2;
    while (true) {
        if (it != m_conns.end() &&
            !it->second.first.empty() &&
            it->second.first.size() >= m_connectionsPerHost) {
            ConnectionList &connsForThisUri = it->second.first;
            // Assign it2 to point to the connection with the
            // least number of outstanding requests
            it2 = std::min_element(connsForThisUri.begin(),
                connsForThisUri.end(), &least);
            // No connection has completed yet (but it's in progress)
            if (!*it2) {
                MORDOR_LOG_TRACE(g_cacheLog) << this << " waiting for connection to "
                    << endpoint;
                // Wait for somebody to let us try again
                // Have to copy the shared ptr to this stack, because the element may be
                // removed while we're waiting for it; the wait will then crash because
                // the object gets deleted out from under it
                boost::shared_ptr<FiberCondition> condition = it->second.second;
                condition->wait();
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
    std::string proxyScheme;
    if (proxy.schemeDefined())
        proxyScheme = proxy.scheme();
    bool proxied = proxyScheme == "http";
    const URI &endpoint = proxied ? proxy : uri;

    // Make sure we have a ConnectionList and mutex for this endpoint
    CachedConnectionMap::iterator it = m_conns.find(endpoint);
    if (it == m_conns.end()) {
        it = m_conns.insert(std::make_pair(endpoint,
            std::make_pair(ConnectionList(),
            boost::shared_ptr<FiberCondition>()))).first;
        it->second.second.reset(new FiberCondition(m_mutex));
    }
    // Add a placeholder for the new connection
    it->second.first.push_back(ClientConnection::ptr());

    MORDOR_LOG_TRACE(g_cacheLog) << this << " establishing connection to " << endpoint;
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
        if (m_httpReadTimeout != ~0ull)
            result.first->readTimeout(m_httpReadTimeout);
        if (m_httpWriteTimeout != ~0ull)
            result.first->writeTimeout(m_httpWriteTimeout);
        // Assign this connection to the first blank connection for this
        // schemeAndAuthority
        // it should still be valid, even if the map changed
        for (it2 = it->second.first.begin();
            it2 != it->second.first.end();
            ++it2) {
            if (!*it2) {
                *it2 = result.first;
                break;
            }
        }
        // Unblock all waiters for them to choose an existing connection
        it->second.second->broadcast();
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
        for (it2 = it->second.first.begin();
            it2 != it->second.first.end();
            ++it2) {
            if (!*it2) {
                it->second.first.erase(it2);
                break;
            }
        }
        it->second.second->broadcast();
        if (it->second.first.empty())
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
    // in progress that has an iterator into it
    std::map<URI, std::pair<ConnectionList,
        boost::shared_ptr<FiberCondition> > >::iterator it, extraIt;
    for (it = m_conns.begin(); it != m_conns.end();) {
        it->second.second->broadcast();
        for (ConnectionList::iterator it2 = it->second.first.begin();
            it2 != it->second.first.end();) {
            if (*it2) {
                Stream::ptr connStream = (*it2)->stream();
                connStream->cancelRead();
                connStream->cancelWrite();
                it2 = it->second.first.erase(it2);
            } else {
                ++it2;
            }
        }
        if (it->second.first.empty()) {
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
    std::map<URI, std::pair<ConnectionList,
        boost::shared_ptr<FiberCondition> > >::iterator it;
    for (it = m_conns.begin(); it != m_conns.end(); ++it) {
        it->second.second->broadcast();
        for (ConnectionList::iterator it2 = it->second.first.begin();
            it2 != it->second.first.end();
            ++it2) {
            if (*it2) {
                Stream::ptr connStream = (*it2)->stream();
                connStream->cancelRead();
                connStream->cancelWrite();
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
        for (it2 = it->second.first.begin();
            it2 != it->second.first.end();) {
            if (*it2 && !(*it2)->newRequestsAllowed()) {
                it2 = it->second.first.erase(it2);
            } else {
                ++it2;
            }
        }
        if (it->second.first.empty()) {
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

std::pair<ClientConnection::ptr, bool>
MockConnectionBroker::getConnection(const URI &uri, bool forceNewConnection)
{
    URI schemeAndAuthority = uri;
    schemeAndAuthority.path = URI::Path();
    schemeAndAuthority.queryDefined(false);
    schemeAndAuthority.fragmentDefined(false);
    ConnectionCache::iterator it = m_conns.find(schemeAndAuthority);
    if (it != m_conns.end() && !it->second.first->newRequestsAllowed()) {
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
        m_conns[schemeAndAuthority] = std::make_pair(client, server);
        return std::make_pair(client, false);
    }
    return std::make_pair(it->second.first, false);
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
    MORDOR_ASSERT(connect || originalUri.authority.hostDefined());
    MORDOR_ASSERT(!connect || !requestHeaders.request.host.empty());
    if (!connect) {
        requestHeaders.request.host = originalUri.authority.host();
    } else {
        MORDOR_ASSERT(originalUri.scheme() == "http");
        MORDOR_ASSERT(originalUri.path.segments.size() == 1);
        currentUri = originalUri.path.segments[0];
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
    try {
        // Fix up our URI for use with/without proxies
        if (!connect) {
            if (conn.second && !currentUri.authority.hostDefined()) {
                currentUri.authority = originalUri.authority;
                if (originalUri.schemeDefined())
                    currentUri.scheme(originalUri.scheme());
            } else if (!conn.second && currentUri.authority.hostDefined()) {
                currentUri.schemeDefined(false);
                currentUri.authority.hostDefined(false);
            }
        }

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
