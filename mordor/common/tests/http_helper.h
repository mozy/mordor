#ifndef __HTTP_HELPER_H__
#define __HTTP_HELPER_H__
// Copyright (c) 2009 - Decho Corp.

#include <boost/function.hpp>

#include "mordor/common/http/client.h"
#include "mordor/common/http/server.h"

class HTTPHelper
{
private:
    typedef std::map<Mordor::URI,
        std::pair<Mordor::HTTP::ClientConnection::ptr, Mordor::HTTP::ServerConnection::ptr> >
        ConnectionCache;
public:
    HTTPHelper(boost::function<void (const Mordor::URI &uri, Mordor::HTTP::ServerRequest::ptr)> dg)
        : m_dg(dg)
    {}

    Mordor::HTTP::ClientConnection::ptr getConn(const Mordor::URI &uri);

private:
    boost::function<void (const Mordor::URI &uri, Mordor::HTTP::ServerRequest::ptr)> m_dg;
    ConnectionCache m_conns;
};

#endif
