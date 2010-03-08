// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "broker.h"

#include "mordor/future.h"
#include "mordor/streams/pipe.h"
#include "mordor/streams/socket.h"
#include "mordor/streams/ssl.h"
#include "proxy.h"

namespace Mordor {
namespace HTTP {

RequestBroker::ptr defaultRequestBroker(IOManager *ioManager,
                                        Scheduler *scheduler,
                                        ConnectionBroker::ptr *connBroker,
                                        boost::function<bool (size_t)> delayDg)
{
    StreamBroker::ptr socketBroker(new SocketStreamBroker(ioManager, scheduler));
    StreamBrokerFilter::ptr sslBroker(new SSLStreamBroker(socketBroker));
    ConnectionBroker::ptr connectionBroker(new ConnectionCache(sslBroker));
    if (connBroker != NULL)
        *connBroker = connectionBroker;
    RequestBroker::ptr requestBroker(new BaseRequestBroker(ConnectionBroker::weak_ptr(connectionBroker)));
    if (delayDg)
        requestBroker.reset(new RetryRequestBroker(requestBroker, delayDg));

    socketBroker.reset(new ProxyStreamBroker(socketBroker, requestBroker));
    sslBroker->parent(socketBroker);
    connectionBroker.reset(new ProxyConnectionBroker(connectionBroker));
    requestBroker.reset(new BaseRequestBroker(connectionBroker));
    if (delayDg)
        requestBroker.reset(new RetryRequestBroker(requestBroker, delayDg));
    return requestBroker;
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
        socket->sendTimeout(connectTimeout);
        try {
            socket->connect(*it);
            socket->sendTimeout(sendTimeout);
            socket->receiveTimeout(receiveTimeout);
            boost::mutex::scoped_lock lock(m_mutex);
            m_pending.erase(it2);
            break;
        } catch (...) {
            boost::mutex::scoped_lock lock(m_mutex);
            m_pending.erase(it2);
            if (++it == addresses.end())
                throw;
        }
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

Stream::ptr
SSLStreamBroker::getStream(const URI &uri)
{
    Stream::ptr result = parent()->getStream(uri);
    if (uri.schemeDefined() && uri.scheme() == "https") {
        SSLStream::ptr sslStream(new SSLStream(result, true, true, m_sslCtx));
        result = sslStream;
        sslStream->connect();
        if (m_verifySslCert)
            sslStream->verifyPeerCertificate();
        if (m_verifySslCertHost)
            sslStream->verifyPeerCertificate(uri.authority.host());
    }
    return result;
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

std::pair<ClientConnection::ptr, bool>
ConnectionCache::getConnection(const URI &uri, bool forceNewConnection)
{
    URI schemeAndAuthority;
    std::map<URI, std::pair<ConnectionList,
        boost::shared_ptr<FiberCondition> > >::iterator it, it3;
    ConnectionList::iterator it2;
    std::pair<ClientConnection::ptr, bool> result;
    {
        FiberMutex::ScopedLock lock(m_mutex);
        // Clean out any dead conns
        for (it = m_conns.begin(); it != m_conns.end();) {
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
                m_conns.erase(it);
                it = it3;
            } else {
                ++it;
            }
        }

        schemeAndAuthority = uri;
        schemeAndAuthority.path = URI::Path();
        schemeAndAuthority.queryDefined(false);
        schemeAndAuthority.fragmentDefined(false);

        if (!forceNewConnection) {
            while (true) {
                // Look for an existing connection
                it = m_conns.find(schemeAndAuthority);
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
                        // Wait for somebody to let us try again
                        it->second.second->wait();
                    } else {
                        // Return the existing, completed connection
                        return std::make_pair(*it2, false);
                    }
                } else {
                    // No existing connections
                    break;
                }
            }
        }
        // Create a new (blank) connection
        m_conns[schemeAndAuthority].first.push_back(ClientConnection::ptr());
        if (it == m_conns.end()) {
            // This is the first connection for this schemeAndAuthority
            it = m_conns.find(schemeAndAuthority);
            // (double-check)
            if (!it->second.second)
                // Create the condition variable for it
                it->second.second.reset(new FiberCondition(m_mutex));
        }
    }

    // Establish a new connection
    try {
        Stream::ptr stream = m_streamBroker->getStream(schemeAndAuthority);
        {
            FiberMutex::ScopedLock lock(m_mutex);
            result = std::make_pair(ClientConnection::ptr(
                new ClientConnection(stream)), false);
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
        }
    } catch (...) {
        FiberMutex::ScopedLock lock(m_mutex);
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
ConnectionCache::closeConnections()
{
    m_streamBroker->cancelPending();
    FiberMutex::ScopedLock lock(m_mutex);
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
    if (!connect)
        requestHeaders.request.host = originalUri.authority.host();
    else
        originalUri = "http://" + requestHeaders.request.host;
    ConnectionBroker::ptr connectionBroker = m_connectionBroker;
    if (!connectionBroker)
        connectionBroker = m_weakConnectionBroker.lock();
    std::pair<ClientConnection::ptr, bool> conn;
    try {
        conn = connectionBroker->getConnection(
            connect ? originalUri : currentUri, forceNewConnection);
    } catch (boost::exception &ex) {
        if (!connect)
            currentUri = originalUri;
        ex << errinfo_source(CONNECTION);
        throw;
    } catch (...) {
        if (!connect)
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
        if (!connect)
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
        if (!connect)
            currentUri = originalUri;
        throw;
    }
}

ClientRequest::ptr
RetryRequestBroker::request(Request &requestHeaders, bool forceNewConnection,
                           boost::function<void (ClientRequest::ptr)> bodyDg)
{
    size_t retries = 0;
    while (true) {
        try {
            return parent()->request(requestHeaders, forceNewConnection, bodyDg);
        } catch (SocketException &ex) {
            const ExceptionSource *source = boost::get_error_info<errinfo_source>(ex);
            if (!source || *source != HTTP)
                throw;
            if (m_delayDg && !m_delayDg(++retries))
                throw;
            continue;
        } catch (PriorRequestFailedException &ex) {
            const ExceptionSource *source = boost::get_error_info<errinfo_source>(ex);
            if (!source || *source != HTTP)
                throw;
            if (m_delayDg && !m_delayDg(++retries))
                throw;
            continue;
        } catch (UnexpectedEofException &ex) {
            const ExceptionSource *source = boost::get_error_info<errinfo_source>(ex);
            if (!source || *source != HTTP)
                throw;
            if (m_delayDg && !m_delayDg(++retries))
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
