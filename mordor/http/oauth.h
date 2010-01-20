#ifndef __MORDOR_HTTP_OAUTH_H__
#define __MORDOR_HTTP_OAUTH_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <boost/function.hpp>

#include "broker.h"
#include "http.h"

namespace Mordor {
namespace HTTP {

class OAuth
{
public:
    OAuth(RequestBroker::ptr requestBroker,
        boost::function<std::string (const URI::QueryString &params)> authDg,
        const URI &requestTokenUri, Method requestTokenMethod,
        const std::string &requestTokenSignatureMethod,
        const URI &accessTokenUri, Method accessTokenMethod,
        const std::string &accessTokenSignatureMethod,
        const std::string &consumerKey, const std::string &consumerSecret,
        const URI &callbackUri = "")
        : m_requestBroker(requestBroker),
          m_authDg(authDg),
          m_requestTokenUri(requestTokenUri),
          m_accessTokenUri(accessTokenUri),
          m_requestTokenMethod(requestTokenMethod),
          m_accessTokenMethod(accessTokenMethod),
          m_requestTokenSignatureMethod(requestTokenSignatureMethod),
          m_accessTokenSignatureMethod(accessTokenSignatureMethod),
          m_consumerKey(consumerKey),
          m_consumerSecret(consumerSecret),
          m_callbackUri(callbackUri)
    {}

    void authorize(Request &nextRequest,
        const std::string &signatureMethod, const std::string &realm);

    // For testing use only
    void selfNonce(boost::function<std::pair<unsigned long long, std::string> ()>
        dg) { m_nonceDg = dg; }

private:
    void getRequestToken();
    void getAccessToken(const std::string &verifier);
    URI::QueryString signRequest(const URI &uri, Method method,
        const std::string &signatureMethod);
    void nonceAndTimestamp(URI::QueryString &parameters);
    void sign(const URI &uri, Method method, const std::string &signatureMethod,
        URI::QueryString &params);
private:
    RequestBroker::ptr m_requestBroker;
    boost::function<std::pair<unsigned long long, std::string> ()> m_nonceDg;
    boost::function<std::string (const URI::QueryString &params)> m_authDg;
    URI m_requestTokenUri, m_accessTokenUri;
    HTTP::Method m_requestTokenMethod, m_accessTokenMethod;
    std::string m_requestTokenSignatureMethod, m_accessTokenSignatureMethod;
    std::string m_consumerKey, m_consumerSecret;
    URI m_callbackUri;
    URI::QueryString m_params;
};

}}

#endif
