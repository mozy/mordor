#ifndef __MORDOR_HTTP_AUTH_H__
#define __MORDOR_HTTP_AUTH_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

#include "broker.h"
#include "client.h"

namespace Mordor {
namespace HTTP {

/// @deprecated Please use AuthRequestBroker
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

class AuthRequestBroker : public RequestBrokerFilter
{
public:
    AuthRequestBroker(RequestBroker::ptr parent,
        boost::function<bool (const URI &,
            ClientRequest::ptr /* priorRequest = ClientRequest::ptr() */,
            std::string & /* scheme */, std::string & /* realm */,
            std::string & /* username */, std::string & /* password */,
            size_t /* attempts */)>
            getCredentialsDg,
        boost::function<bool (const URI &,
            ClientRequest::ptr /* priorRequest = ClientRequest::ptr() */,
            std::string & /* scheme */, std::string & /* realm */,
            std::string & /* username */, std::string & /* password */,
            size_t /* attempts */)>
            getProxyCredentialsDg)
        : RequestBrokerFilter(parent),
          m_getCredentialsDg(getCredentialsDg),
          m_getProxyCredentialsDg(getProxyCredentialsDg)
    {}

    ClientRequest::ptr request(Request &requestHeaders,
        bool forceNewConnection = false,
        boost::function<void (ClientRequest::ptr)> bodyDg = NULL);

private:
    boost::function<bool (const URI &, ClientRequest::ptr, std::string &,
        std::string &, std::string &, std::string &, size_t)>
        m_getCredentialsDg, m_getProxyCredentialsDg;
};

}}

#endif
