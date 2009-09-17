#ifndef __HTTP_TUNNEL_H__
#define __HTTP_TUNNEL_H__
// Copyright (c) 2009 - Decho Corp.

#include "auth.h"

namespace HTTP
{
    template <class T>
    Stream::ptr tunnel(T &conn, const std::string &proxy, const std::string &target)
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
            throw std::runtime_error("proxy connection failed");
        }
    }
}

#endif
