// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "http_helper.h"

#include <boost/bind.hpp>

#include "mordor/common/streams/pipe.h"

HTTP::ClientConnection::ptr
HTTPHelper::getConn(const URI &uri)
{
    ConnectionCache::iterator it = m_conns.find(uri);
    if (it != m_conns.end() && !it->second.first->newRequestsAllowed()) {
        m_conns.erase(it);
        it = m_conns.end();
    }
    if (it == m_conns.end()) {
        std::pair<Stream::ptr, Stream::ptr> pipes = pipeStream();
        HTTP::ClientConnection::ptr client(
            new HTTP::ClientConnection(pipes.first));
        HTTP::ServerConnection::ptr server(
            new HTTP::ServerConnection(pipes.second, boost::bind(m_dg,
                uri, _1)));
        Scheduler::getThis()->schedule(Fiber::ptr(new Fiber(boost::bind(
            &HTTP::ServerConnection::processRequests, server))));
        m_conns[uri] = std::make_pair(client, server);
        return client;
    }
    return it->second.first;
}
