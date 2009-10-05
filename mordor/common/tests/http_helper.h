#ifndef __HTTP_HELPER_H__
#define __HTTP_HELPER_H__
// Copyright (c) 2009 - Decho Corp.

#include <boost/function.hpp>

#include "mordor/common/http/client.h"
#include "mordor/common/http/server.h"

class HTTPHelper
{
private:
    typedef std::map<URI,
        std::pair<HTTP::ClientConnection::ptr, HTTP::ServerConnection::ptr> >
        ConnectionCache;
public:
    HTTPHelper(boost::function<void (const URI &uri, HTTP::ServerRequest::ptr)> dg)
        : m_dg(dg)
    {}

    HTTP::ClientConnection::ptr getConn(const URI &uri);

private:
    boost::function<void (const URI &uri, HTTP::ServerRequest::ptr)> m_dg;
    ConnectionCache m_conns;
};

#endif
