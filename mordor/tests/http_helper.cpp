// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "http_helper.h"

#include <boost/bind.hpp>

#include "mordor/streams/pipe.h"

using namespace Mordor;
using namespace Mordor::HTTP;

ClientConnection::ptr
HTTPHelper::getConn(const URI &uri)
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
        return client;
    }
    return it->second.first;
}
