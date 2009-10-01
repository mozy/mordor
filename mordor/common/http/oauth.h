#ifndef __HTTP_OAUTH_H__
#define __HTTP_OAUTH_H__
// Copyright (c) 2009 - Decho Corp.

#include <boost/function.hpp>

#include "client.h"
#include "http.h"

namespace HTTP
{
    class OAuth
    {
    public:
        OAuth(boost::function<ClientConnection::ptr (const URI &uri)> connDg,
            boost::function<std::string (const URI::QueryString &params)> authDg,
            const URI &requestTokenUri, Method requestTokenMethod,
            const URI &accessTokenUri, Method accessTokenMethod,
            const std::string &consumerKey, const std::string &consumerSecret);

        void authorize(Request &nextRequest);

    private:
        void getRequestToken();
        void getAccessToken(const std::string &verifier);
        URI::QueryString signRequest(const URI &uri, Method method);
        void nonceAndTimestamp(URI::QueryString &parameters);
        void sign(URI uri, Method method, URI::QueryString &params);
    private:
        boost::function<HTTP::ClientConnection::ptr (const URI &uri)> m_connDg;
        boost::function<std::string (const URI::QueryString &params)> m_authDg;
        std::string m_consumerKey, m_consumerSecret;
        URI m_requestTokenUri, m_accessTokenUri;
        HTTP::Method m_requestTokenMethod, m_accessTokenMethod;
        URI::QueryString m_params;
    };
};

#endif
