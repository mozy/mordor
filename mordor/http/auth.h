#ifndef __MORDOR_HTTP_AUTH_H__
#define __MORDOR_HTTP_AUTH_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

#include "broker.h"
#include "mordor/version.h"

namespace Mordor {
namespace HTTP {

class AuthRequestBroker : public RequestBrokerFilter
{
public:
    AuthRequestBroker(RequestBroker::ptr parent,
        boost::function<bool (const URI &,
            boost::shared_ptr<ClientRequest> /* priorRequest = ClientRequest::ptr() */,
            std::string & /* scheme */, std::string & /* realm */,
            std::string & /* username */, std::string & /* password */,
            size_t /* attempts */)>
            getCredentialsDg,
        boost::function<bool (const URI &,
            boost::shared_ptr<ClientRequest> /* priorRequest = ClientRequest::ptr() */,
            std::string & /* scheme */, std::string & /* realm */,
            std::string & /* username */, std::string & /* password */,
            size_t /* attempts */)>
            getProxyCredentialsDg)
        : RequestBrokerFilter(parent),
          m_getCredentialsDg(getCredentialsDg),
          m_getProxyCredentialsDg(getProxyCredentialsDg)
    {}

    boost::shared_ptr<ClientRequest> request(Request &requestHeaders,
        bool forceNewConnection = false,
        boost::function<void (boost::shared_ptr<ClientRequest>)> bodyDg = NULL);

private:
    boost::function<bool (const URI &, boost::shared_ptr<ClientRequest>,
        std::string &, std::string &, std::string &, std::string &, size_t)>
        m_getCredentialsDg, m_getProxyCredentialsDg;
};
	
#ifdef OSX
bool getCredentialsFromKeychain(const URI &uri,
    boost::shared_ptr<ClientRequest> priorRequest,
    std::string &scheme, std::string &realm, std::string &username,
    std::string &password, size_t attempts);
#endif

}}

#endif
