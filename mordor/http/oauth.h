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
    struct Settings
    {
        boost::function<std::string (const URI::QueryString &params)> authDg;
        URI requestTokenUri, accessTokenUri;
        HTTP::Method requestTokenMethod, accessTokenMethod;
        std::string requestTokenSignatureMethod, accessTokenSignatureMethod;
        std::string consumerKey, consumerSecret;
        URI callbackUri;
    };
public:
    OAuth(RequestBroker::ptr requestBroker, const Settings &settings,
        boost::function<void (const std::string &, const std::string &)> gotTokenDg = NULL);

    void clearToken();
    void setToken(const std::string &token, const std::string &tokenSecret);

    void authorize(Request &nextRequest,
        const std::string &signatureMethod, const std::string &realm);

    template <class T>
    static void nonceAndTimestamp(T &oauthParameters);

    template <class T>
    static void sign(const URI &uri, Method method,
        const std::string &signatureMethod, const std::string &clientSecret,
        const std::string &tokenSecret, T &oauthParameters,
        const URI::QueryString &postParameters = URI::QueryString());

    // For testing use only
    void selfNonce(boost::function<std::pair<unsigned long long, std::string> ()>
        dg) { m_nonceDg = dg; }

private:
    void getRequestToken();
    void getAccessToken(const std::string &verifier);
    URI::QueryString signRequest(const URI &uri, Method method,
        const std::string &signatureMethod);
    void nonceAndTimestampInternal(URI::QueryString &params);

private:
    RequestBroker::ptr m_requestBroker;
    boost::function<std::pair<unsigned long long, std::string> ()> m_nonceDg;
    boost::function<void (const std::string &, const std::string &)> m_gotTokenDg;
    Settings m_settings;
    URI::QueryString m_params;
};

class OAuthBroker : public RequestBrokerFilter
{
private:
    struct State
    {
        OAuth oauth;
        std::string realm;
        std::string signatureMethod;
    };
public:
    OAuthBroker(RequestBroker::ptr parent,
        boost::function<std::pair<OAuth::Settings, std::string>
            (const URI &, const std::string &)> getSettingsDg,
        boost::function<void
            (const std::string &, const std::string &)> gotTokenDg = NULL,
        RequestBroker::ptr brokerForOAuthRequests = RequestBroker::ptr());

    ClientRequest::ptr request(Request &requestHeaders,
        bool forceNewConnection = false,
        boost::function<void (ClientRequest::ptr)> bodyDg = NULL);

private:
    RequestBroker::ptr m_brokerForOAuthRequests;
    std::map<URI, State> m_state;
    boost::function<std::pair<OAuth::Settings, std::string>
        (const URI &, const std::string &)> m_getSettingsDg;
    boost::function<void
        (const std::string &, const std::string &)> m_gotTokenDg;
};

}}

#endif
