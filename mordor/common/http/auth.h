#ifndef __HTTP_AUTH_H__
#define __HTTP_AUTH_H__
// Copyright (c) 2009 - Decho Corp.

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>

#include "client.h"

namespace HTTP
{
    class ClientAuthBroker : public boost::noncopyable
    {
    public:
        ClientAuthBroker(boost::function<ClientConnection::ptr ()> dg,
            const std::string &username, const std::string &password,
            const std::string &proxyUsername, const std::string &proxyPassword)
            : m_dg(dg),
              m_username(username),
              m_password(password),
              m_proxyUsername(proxyUsername),
              m_proxyPassword(proxyPassword)
        {}

        // optional dg is to provide the request body if necessary
        ClientRequest::ptr request(Request &requestHeaders,
            boost::function< void (ClientRequest::ptr)> dg = NULL);

    private:
        boost::function<ClientConnection::ptr ()> m_dg;
        std::string m_username, m_password, m_proxyUsername, m_proxyPassword;
        ClientConnection::ptr m_conn;
    };
};

#endif
