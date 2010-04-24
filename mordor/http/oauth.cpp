// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "oauth.h"

#include "mordor/streams/memory.h"
#include "mordor/streams/transfer.h"

namespace Mordor {
namespace HTTP {

OAuth::OAuth(RequestBroker::ptr requestBroker, const Settings &settings,
    boost::function<void (const std::string &, const std::string &)> gotTokenDg)
: m_requestBroker(requestBroker),
  m_gotTokenDg(gotTokenDg),
  m_settings(settings)

{
    MORDOR_ASSERT(m_settings.authDg);
    MORDOR_ASSERT(m_settings.requestTokenUri.isDefined());
    MORDOR_ASSERT(m_settings.accessTokenUri.isDefined());
    MORDOR_ASSERT(!m_settings.requestTokenSignatureMethod.empty());
    MORDOR_ASSERT(!m_settings.accessTokenSignatureMethod.empty());
    MORDOR_ASSERT(!m_settings.consumerKey.empty());
    MORDOR_ASSERT(!m_settings.consumerSecret.empty());
}

void
OAuth::clearToken()
{
    m_params.clear();
}

void
OAuth::setToken(const std::string &token, const std::string &tokenSecret)
{
    m_params.clear();
    m_params.insert(std::make_pair(std::string("oauth_token"), token));
    m_params.insert(std::make_pair(std::string("oauth_token_secret"),
        tokenSecret));
}

void
OAuth::authorize(Request &nextRequest,
                 const std::string &signatureMethod,
                 const std::string &realm)
{
    if (m_params.find("oauth_token_secret") == m_params.end() ||
        m_params.find("oauth_token") == m_params.end()) {
        getRequestToken();
        getAccessToken(m_settings.authDg(m_params));
    }
    AuthParams &authorization = nextRequest.request.authorization;
    authorization.scheme = "OAuth";
    URI::QueryString params = signRequest(nextRequest.requestLine.uri,
        nextRequest.requestLine.method, signatureMethod);
    authorization.parameters.clear();
    authorization.parameters.insert(params.begin(), params.end());
    authorization.parameters["realm"] = realm;
}

static void writeBody(ClientRequest::ptr request, const std::string &body)
{
    request->requestStream()->write(body.c_str(), body.size());
    request->requestStream()->close();
}

void
OAuth::getRequestToken()
{
    MORDOR_ASSERT(m_settings.requestTokenMethod == GET || m_settings.requestTokenMethod == POST);
    URI::QueryString qs;

    qs.insert(std::make_pair("oauth_consumer_key", m_settings.consumerKey));
    qs.insert(std::make_pair("oauth_version", "1.0"));
    if (!m_settings.callbackUri.isDefined())
        qs.insert(std::make_pair("oauth_callback", "oob"));
    else
        qs.insert(std::make_pair("oauth_callback", m_settings.callbackUri.toString()));
    nonceAndTimestamp(qs);
    sign(m_settings.requestTokenUri, m_settings.requestTokenMethod,
        m_settings.requestTokenSignatureMethod, qs);

    Request requestHeaders;
    requestHeaders.requestLine.method = m_settings.requestTokenMethod;
    requestHeaders.requestLine.uri = m_settings.requestTokenUri;
    std::string body;
    if (m_settings.requestTokenMethod == GET) {
        // Add parameters that are part of the request token URI
        URI::QueryString qsFromUri = m_settings.requestTokenUri.queryString();
        qs.insert(qsFromUri.begin(), qsFromUri.end());
        requestHeaders.requestLine.uri.query(qs);
    } else {
        body = qs.toString();
        requestHeaders.entity.contentType.type = "application";
        requestHeaders.entity.contentType.subtype = "x-www-form-urlencoded";
        requestHeaders.entity.contentLength = body.size();
    }

    boost::function<void (ClientRequest::ptr)> bodyDg;
    if (!body.empty())
        bodyDg = boost::bind(&writeBody, _1, boost::cref(body));
    ClientRequest::ptr request;
    try {
        request = m_requestBroker->request(requestHeaders, false, bodyDg);
        m_settings.requestTokenUri = requestHeaders.requestLine.uri;
    } catch (...) {
        m_settings.requestTokenUri = requestHeaders.requestLine.uri;
        throw;
    }
    if (request->response().status.status != OK)
        MORDOR_THROW_EXCEPTION(InvalidResponseException(request));

    MemoryStream responseStream;
    transferStream(request->responseStream(), responseStream);
    std::string response;
    response.resize(responseStream.buffer().readAvailable());
    responseStream.buffer().copyOut(&response[0], responseStream.buffer().readAvailable());
    m_params = response;
    URI::QueryString::iterator it = m_params.find("oauth_token");
    if (it == m_params.end())
        MORDOR_THROW_EXCEPTION(InvalidResponseException("Missing oauth_token in response",
            request));
    ++it;
    if (it != m_params.end() &&
        stricmp(it->first.c_str(), "oauth_token") == 0)
        MORDOR_THROW_EXCEPTION(InvalidResponseException("Duplicate oauth_token in response",
            request));
    it = m_params.find("oauth_token_secret");
    if (it == m_params.end())
        MORDOR_THROW_EXCEPTION(InvalidResponseException("Missing oauth_token_secret in response",
            request));
    ++it;
    if (it != m_params.end() &&
        stricmp(it->first.c_str(), "oauth_token_secret") == 0)
        MORDOR_THROW_EXCEPTION(InvalidResponseException("Duplicate oauth_token_secret in response",
            request));
}

void
OAuth::getAccessToken(const std::string &verifier)
{
    MORDOR_ASSERT(m_settings.accessTokenMethod == GET ||
        m_settings.accessTokenMethod == POST);
    URI::QueryString qs;

    qs.insert(std::make_pair("oauth_consumer_key", m_settings.consumerKey));
    qs.insert(*m_params.find("oauth_token"));
    qs.insert(std::make_pair("oauth_verifier", verifier));
    qs.insert(std::make_pair("oauth_version", "1.0"));
    nonceAndTimestamp(qs);
    sign(m_settings.accessTokenUri, m_settings.accessTokenMethod,
        m_settings.requestTokenSignatureMethod, qs);

    Request requestHeaders;
    requestHeaders.requestLine.method = m_settings.accessTokenMethod;
    requestHeaders.requestLine.uri = m_settings.accessTokenUri;
    std::string body;
    if (m_settings.accessTokenMethod == GET) {
        // Add parameters that are part of the request token URI
        URI::QueryString qsFromUri = m_settings.accessTokenUri.queryString();
        qs.insert(qsFromUri.begin(), qsFromUri.end());
        requestHeaders.requestLine.uri.query(qs);
    } else {
        body = qs.toString();
        requestHeaders.entity.contentType.type = "application";
        requestHeaders.entity.contentType.subtype = "x-www-form-urlencoded";
        requestHeaders.entity.contentLength = body.size();
    }

    boost::function<void (ClientRequest::ptr)> bodyDg;
    if (!body.empty())
        bodyDg = boost::bind(&writeBody, _1, boost::cref(body));
    ClientRequest::ptr request;
    try {
        request = m_requestBroker->request(requestHeaders, false, bodyDg);
        m_settings.accessTokenUri = requestHeaders.requestLine.uri;
    } catch (...) {
        m_settings.accessTokenUri = requestHeaders.requestLine.uri;
        throw;
    }
    if (request->response().status.status != OK)
        MORDOR_THROW_EXCEPTION(InvalidResponseException(request));

    MemoryStream responseStream;
    transferStream(request->responseStream(), responseStream);
    std::string response;
    response.resize(responseStream.buffer().readAvailable());
    responseStream.buffer().copyOut(&response[0], responseStream.buffer().readAvailable());
    m_params = response;
    URI::QueryString::iterator it = m_params.find("oauth_token");
    if (it == m_params.end())
        MORDOR_THROW_EXCEPTION(InvalidResponseException("Missing oauth_token in response",
            request));
    std::string token = it->second;
    ++it;
    if (it != m_params.end() &&
        stricmp(it->first.c_str(), "oauth_token") == 0)
        MORDOR_THROW_EXCEPTION(InvalidResponseException("Duplicate oauth_token in response",
            request));
    it = m_params.find("oauth_token_secret");
    if (it == m_params.end())
        MORDOR_THROW_EXCEPTION(InvalidResponseException("Missing oauth_token_secret in response",
            request));
    std::string secret = it->second;
    ++it;
    if (it != m_params.end() &&
        stricmp(it->first.c_str(), "oauth_token_secret") == 0)
        MORDOR_THROW_EXCEPTION(InvalidResponseException("Duplicate oauth_token_secret in response",
            request));
    if (m_gotTokenDg)
        m_gotTokenDg(token, secret);
}

URI::QueryString
OAuth::signRequest(const URI &uri, Method method,
                   const std::string &signatureMethod)
{
    URI::QueryString result;
    result.insert(std::make_pair("oauth_consumer_key", m_settings.consumerKey));
    result.insert(*m_params.find("oauth_token"));
    result.insert(std::make_pair("oauth_version", "1.0"));
    nonceAndTimestamp(result);
    sign(uri, method, signatureMethod, result);
    return result;
}

void
OAuth::nonceAndTimestamp(URI::QueryString &params)
{
    if (m_nonceDg) {
        std::ostringstream os;
        std::pair<unsigned long long, std::string> timestampAndNonce =
            m_nonceDg();
        os << timestampAndNonce.first;
        params.insert(std::make_pair("oauth_timestamp", os.str()));
        params.insert(std::make_pair("oauth_nonce", timestampAndNonce.second));
    } else {
        static boost::posix_time::ptime start(boost::gregorian::date(1970, 1, 1));
        static const char *allowedChars =
            "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        std::ostringstream os;
        boost::posix_time::ptime now =
            boost::posix_time::second_clock::universal_time();
        boost::posix_time::time_duration duration = now - start;
        os << duration.total_seconds();

        std::string nonce;
        nonce.resize(40);
        for (size_t i = 0; i < 40; ++i) {
            nonce[i] = allowedChars[rand() % 36];
        }

        params.insert(std::make_pair("oauth_timestamp", os.str()));
        params.insert(std::make_pair("oauth_nonce", nonce));
    }
}

void
OAuth::sign(const URI &uri, Method method, const std::string &signatureMethod,
            URI::QueryString &params)
{
    MORDOR_ASSERT(params.find("oauth_signature_method") == params.end());
    params.insert(std::make_pair("oauth_signature_method", signatureMethod));
    MORDOR_ASSERT(params.find("oauth_signature") == params.end());

    URI::QueryString::iterator it;

    std::ostringstream os;
    URI requestUri(uri);
    requestUri.queryDefined(false);
    requestUri.fragmentDefined(false);
    requestUri.normalize();
    os << method << '&' << URI::encode(requestUri.toString());
    std::map<std::string, std::multiset<std::string> > combined;
    std::map<std::string, std::multiset<std::string> >::iterator
        combinedIt;
    for (it = params.begin(); it != params.end(); ++it)
        if (stricmp(it->first.c_str(), "realm") != 0)
            combined[it->first].insert(it->second);
    // TODO: POST params of application/x-www-form-urlencoded
    if (uri.queryDefined()) {
        URI::QueryString queryParams = uri.queryString();
        for (it = queryParams.begin(); it != queryParams.end(); ++it)
            combined[it->first].insert(it->second);
    }

    os << '&';
    std::string signatureBaseString = os.str();
    os.str("");
    bool first = true;
    for (combinedIt = combined.begin();
        combinedIt != combined.end();
        ++combinedIt) {
        for (std::multiset<std::string>::iterator it2 =
            combinedIt->second.begin();
            it2 != combinedIt->second.end();
            ++it2) {
            if (!first)
                os << '&';
            first = false;
            os << URI::encode(combinedIt->first)
                << '=' << URI::encode(*it2);
        }
    }
    signatureBaseString.append(URI::encode(os.str()));

    std::string secrets = URI::encode(m_settings.consumerSecret);
    secrets.append(1, '&');
    it = m_params.find("oauth_token_secret");
    if (it != m_params.end())
        secrets.append(URI::encode(it->second));

    if (stricmp(signatureMethod.c_str(), "HMAC-SHA1") == 0) {
        params.insert(std::make_pair("oauth_signature",
            base64encode(hmacSha1(signatureBaseString, secrets))));
    } else if (stricmp(signatureMethod.c_str(), "PLAINTEXT") == 0) {
        params.insert(std::make_pair("oauth_signature", secrets));
    } else {
        MORDOR_NOTREACHED();
    }
}

OAuthBroker::OAuthBroker(RequestBroker::ptr parent,
    boost::function<std::pair<OAuth::Settings, std::string>
        (const URI &, const std::string &)> getSettingsDg,
    boost::function<void (const std::string &, const std::string &)> gotTokenDg,
    RequestBroker::ptr brokerForOAuthRequests)
: RequestBrokerFilter(parent),
  m_brokerForOAuthRequests(brokerForOAuthRequests),
  m_getSettingsDg(getSettingsDg),
  m_gotTokenDg(gotTokenDg)
{
    MORDOR_ASSERT(getSettingsDg);
    if (!brokerForOAuthRequests)
        m_brokerForOAuthRequests = parent;
}

ClientRequest::ptr
OAuthBroker::request(Request &requestHeaders,
        bool forceNewConnection,
        boost::function<void (ClientRequest::ptr)> bodyDg)
{
    URI schemeAndAuthority = requestHeaders.requestLine.uri;
    schemeAndAuthority.path = URI::Path();
    schemeAndAuthority.queryDefined(false);
    schemeAndAuthority.fragmentDefined(false);

    std::map<URI, State>::iterator it = m_state.find(schemeAndAuthority);
    int retries = 2;
    while (true) {
        if (it != m_state.end()) {
            it->second.oauth.authorize(requestHeaders,
                it->second.signatureMethod, it->second.realm);
        }
        ClientRequest::ptr request = parent()->request(requestHeaders,
            forceNewConnection, bodyDg);
        Status status = request->response().status.status;
        if (status == UNAUTHORIZED) {
            if (retries-- == 0)
                return request;
            // We already tried to authorize, and apparently the server isn't
            // going to accept our credentials
            if (it != m_state.end()) {
                it->second.oauth.clearToken();
                if (retries-- == 0)
                    return request;
                continue;
            }
            schemeAndAuthority = requestHeaders.requestLine.uri;
            schemeAndAuthority.path = URI::Path();
            schemeAndAuthority.queryDefined(false);
            schemeAndAuthority.fragmentDefined(false);
            const ChallengeList &challenges =
                request->response().response.wwwAuthenticate;
            for (ChallengeList::const_iterator cit = challenges.begin();
                cit != challenges.end();
                ++cit) {
                if (stricmp(cit->scheme.c_str(), "OAuth") == 0) {
                    StringMap::const_iterator pit =
                        cit->parameters.find("realm");
                    std::string realm;
                    if (pit != cit->parameters.end())
                        realm = pit->second;
                    std::pair<OAuth::Settings, std::string> settings =
                        m_getSettingsDg(schemeAndAuthority, realm);
                    if (!settings.first.consumerSecret.empty()) {
                        State state = { OAuth(m_brokerForOAuthRequests,
                            settings.first, m_gotTokenDg), realm, settings.second };
                        it = m_state.insert(std::make_pair(schemeAndAuthority,
                            state)).first;
                        break;
                    }
                }
            }
            if (it != m_state.end())
                continue;
        }
        return request;
    }
}

}}
