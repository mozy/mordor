#ifndef __HTTP_MOCK_SERVER_H__
#define __HTTP_MOCK_SERVER_H__
// Copyright (c) 2009 - Decho Corp.

#include <boost/function.hpp>

#include "client.h"
#include "server.h"

namespace Mordor {
namespace HTTP {

class MockServer
{
private:
    typedef std::map<URI,
        std::pair<ClientConnection::ptr, ServerConnection::ptr> >
        ConnectionCache;
public:
    MockServer(boost::function<void (const URI &uri, ServerRequest::ptr)> dg)
        : m_dg(dg)
    {}

    ClientConnection::ptr getConnection(const URI &uri);

private:
    boost::function<void (const URI &uri, ServerRequest::ptr)> m_dg;
    ConnectionCache m_conns;
};

}}

#endif
