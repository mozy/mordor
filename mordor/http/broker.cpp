// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "broker.h"

#include "mordor/streams/pipe.h"
#include "mordor/streams/socket.h"
#include "mordor/streams/ssl.h"

namespace Mordor {
namespace HTTP {

std::pair<Stream::ptr, bool>
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
    if (uri.schemeDefined() && uri.scheme() == "https") {
        SSLStream::ptr sslStream(new SSLStream(stream, true, true, m_sslCtx));
        stream = sslStream;
        sslStream->connect();
        if (m_verifySslCert)
            sslStream->verifyPeerCertificate();
        if (m_verifySslCertHost)
            sslStream->verifyPeerCertificate(uri.authority.host());
    }
    return std::make_pair(stream, false);    
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

static bool least(const std::pair<ClientConnection::ptr, bool> &lhs,
                  const std::pair<ClientConnection::ptr, bool> &rhs)
{
    if (lhs.first && rhs.first)
        return lhs.first->outstandingRequests() <
            rhs.first->outstandingRequests();
    if (!lhs.first)
        return false;
    if (!rhs.first)
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
                if (it2->first && !it2->first->newRequestsAllowed()) {
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
                    if (!it2->first) {
                        // Wait for somebody to let us try again
                        it->second.second->wait();
                    } else {
                        // Return the existing, completed connection
                        return *it2;
                    }
                } else {
                    // No existing connections
                    break;
                }
            }
        }
        // Create a new (blank) connection
        m_conns[schemeAndAuthority].first.push_back(result);
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
        std::pair<Stream::ptr, bool> stream =
            m_streamBroker->getStream(schemeAndAuthority);
        {
            FiberMutex::ScopedLock lock(m_mutex);
            result = std::make_pair(ClientConnection::ptr(
                new ClientConnection(stream.first)), stream.second);
            // Assign this connection to the first blank connection for this
            // schemeAndAuthority
            // it should still be valid, even if the map changed
            for (it2 = it->second.first.begin();
                it2 != it->second.first.end();
                ++it2) {
                if (!it2->first) {
                    *it2 = result;
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
            if (!it2->first) {
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
            if (it2->first) {
                Stream::ptr connStream = it2->first->stream();
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
    ConnectionCache::iterator it = m_conns.find(uri);
    if (it != m_conns.end() && !it->second.first->newRequestsAllowed()) {
        m_conns.erase(it);
        it = m_conns.end();
    }
    if (it == m_conns.end()) {
        std::pair<Stream::ptr, Stream::ptr> pipes = pipeStream();
        ClientConnection::ptr client(
            new ClientConnection(pipes.first));
        ServerConnection::ptr server(
            new ServerConnection(pipes.second, boost::bind(m_dg,
                uri, _1)));
        Scheduler::getThis()->schedule(Fiber::ptr(new Fiber(boost::bind(
            &ServerConnection::processRequests, server))));
        m_conns[uri] = std::make_pair(client, server);
        return std::make_pair(client, false);
    }
    return std::make_pair(it->second.first, false);
}

ClientRequest::ptr
BaseRequestBroker::request(Request &requestHeaders, bool forceNewConnection)
{
    URI &currentUri = requestHeaders.requestLine.uri;
    URI originalUri = currentUri;
    MORDOR_ASSERT(originalUri.authority.hostDefined());
    requestHeaders.request.host = originalUri.authority.host();
    while (true) {
        std::pair<ClientConnection::ptr, bool> conn =
            m_connectionBroker->getConnection(originalUri, forceNewConnection);
        try {
            // Fix up our URI for use with/without proxies
            if (conn.second && !currentUri.authority.hostDefined()) {
                currentUri.authority = originalUri.authority;
                if (originalUri.schemeDefined())
                    currentUri.scheme(originalUri.scheme());
            } else if (!conn.second && currentUri.authority.hostDefined()) {
                currentUri.schemeDefined(false);
                currentUri.authority.hostDefined(false);
            }

            ClientRequest::ptr request = conn.first->request(requestHeaders);
            currentUri = originalUri;
            return request;
        } catch (SocketException &) {
            continue;
        } catch (PriorRequestFailedException &) {
            continue;
        } catch (...) {
            currentUri = originalUri;
            throw;
        }
        MORDOR_NOTREACHED();
    }
}

ClientRequest::ptr
RedirectRequestBroker::request(Request &requestHeaders, bool forceNewConnection)
{
    URI &currentUri = requestHeaders.requestLine.uri;
    URI originalUri = currentUri;
    while (true) {
        try {
            ClientRequest::ptr request = RequestBrokerFilter::request(requestHeaders,
                forceNewConnection);
            if (request->hasRequestBody()) {
                currentUri = originalUri;
                return request;
            }
            switch (request->response().status.status)
            {
            case FOUND:
            case TEMPORARY_REDIRECT:
            case MOVED_PERMANENTLY:
                currentUri = URI::transform(currentUri,
                    request->response().response.location);
                if (request->response().status.status == MOVED_PERMANENTLY)
                    originalUri = currentUri;
                request->finish();
                continue;
            default:
                currentUri = originalUri;
                return request;
            }
            MORDOR_NOTREACHED();
        } catch (...) {
            currentUri = originalUri;
            throw;
        }
        MORDOR_NOTREACHED();
    }
}

bool
RedirectRequestBroker::checkResponse(ClientRequest::ptr request,
                                     Request &requestHeaders)
{
    URI &currentUri = requestHeaders.requestLine.uri;
    switch (request->response().status.status)
    {
    case FOUND:
    case TEMPORARY_REDIRECT:
    case MOVED_PERMANENTLY:
        currentUri = URI::transform(currentUri,
            request->response().response.location);
        return true;
    default:
        return RequestBrokerFilter::checkResponse(request, requestHeaders);
    }
    MORDOR_NOTREACHED();
}

}}
