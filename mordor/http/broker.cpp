// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "broker.h"

#include "mordor/streams/pipe.h"

namespace Mordor {
namespace HTTP {

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
