#ifndef __MORDOR_HTTP_OAUTH2_H__
#define __MORDOR_HTTP_OAUTH2_H__
// Copyright (c) 2011 - Mozy, Inc.

#include <boost/function.hpp>

#include "broker.h"

namespace Mordor {
namespace HTTP {
namespace OAuth2 {

    void authorize(Request &nextRequest,
        const std::string &token);

    class RequestBroker : public RequestBrokerFilter
    {
    public:
        RequestBroker(HTTP::RequestBroker::ptr parent,
            boost::function<bool (const URI &,
            boost::shared_ptr<ClientRequest> /* priorRequest = ClientRequest::ptr() */,
            std::string & /* token */,
            size_t /* attempts */)> getCredentialsDg)
            : RequestBrokerFilter(parent),
            m_getCredentialsDg(getCredentialsDg)
        {}

        boost::shared_ptr<ClientRequest> request(Request &requestHeaders,
            bool forceNewConnection = false,
            boost::function<void (boost::shared_ptr<ClientRequest>)> bodyDg = NULL);

    private:
        boost::function<bool (const URI &, boost::shared_ptr<ClientRequest>, std::string &,
            size_t)> m_getCredentialsDg;
    };

}}}

#endif // __MORDOR_HTTP_OAUTH2_H__

