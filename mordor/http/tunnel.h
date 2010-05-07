#ifndef __MORDOR_HTTP_TUNNEL_H__
#define __MORDOR_HTTP_TUNNEL_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "client.h"

namespace Mordor {

namespace HTTP {

template <class T>
boost::shared_ptr<Stream> tunnel(T &conn, const std::string &proxy, const std::string &target)
{
    Request requestHeaders;
    requestHeaders.requestLine.method = CONNECT;
    requestHeaders.requestLine.uri = target;
    requestHeaders.request.host = proxy;
    requestHeaders.general.connection.insert("Proxy-Connection");
    requestHeaders.general.proxyConnection.insert("Keep-Alive");
    ClientRequest::ptr request = conn.request(requestHeaders);
    if (request->response().status.status == HTTP::OK) {
        return request->stream();
    } else {
        MORDOR_THROW_EXCEPTION(InvalidResponseException("proxy connection failed",
            request));
    }
}

}}

#endif
