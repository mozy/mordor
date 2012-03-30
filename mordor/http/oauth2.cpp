// Copyright (c) 2011 - Mozy, Inc.

#include "oauth2.h"
#include "client.h"

namespace Mordor {
namespace HTTP {
namespace OAuth2 {

    void
    authorize(Request &nextRequest, const std::string &token)
    {
        MORDOR_ASSERT(!token.empty());

        AuthParams &authorization = nextRequest.request.authorization;
        authorization.scheme = "Bearer";
        authorization.base64 = base64encode(token);
        authorization.parameters.clear();
    }

    static void authorizeDg(ClientRequest::ptr request,
        const std::string &token,
        boost::function<void (ClientRequest::ptr)> bodyDg)
    {
        authorize(request->request(), token);
        if (bodyDg)
            bodyDg(request);
        else
            request->doRequest();
    }

    ClientRequest::ptr
    RequestBroker::request(Request &requestHeaders, bool forceNewConnection,
        boost::function<void (ClientRequest::ptr)> bodyDg)
    {
        ClientRequest::ptr priorRequest;
        std::string token;
        size_t attempts = 0;
        while (true) {
            boost::function<void (ClientRequest::ptr)> wrappedBodyDg = bodyDg;
            if (m_getCredentialsDg(requestHeaders.requestLine.uri, priorRequest, token, attempts++))
                wrappedBodyDg = boost::bind(&authorizeDg, _1, boost::cref(token), bodyDg);
            else if (priorRequest)
                return priorRequest;
            if (priorRequest)
                priorRequest->finish();
            priorRequest = parent()->request(requestHeaders, forceNewConnection,
                wrappedBodyDg);
            if (priorRequest->response().status.status == UNAUTHORIZED &&
                isAcceptable(priorRequest->response().response.wwwAuthenticate,
                "Bearer"))
                continue;
            return priorRequest;
        }
    }

}}}
