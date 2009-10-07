// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "oauth.h"

#include "mordor/common/streams/memory.h"
#include "mordor/common/streams/transfer.h"

void
HTTP::OAuth::authorize(Request &nextRequest,
                       const std::string &signatureMethod,
                       const std::string &realm)
{
    if (m_params.find("oauth_token_secret") == m_params.end() ||
        m_params.find("oauth_token") == m_params.end()) {
        getRequestToken();
        getAccessToken(m_authDg(m_params));
    }
    AuthParams &authorization = nextRequest.request.authorization;
    authorization.scheme = "OAuth";
    URI::QueryString params = signRequest(nextRequest.requestLine.uri,
        nextRequest.requestLine.method, signatureMethod);
    authorization.parameters.clear();
    authorization.parameters.insert(params.begin(), params.end());
    authorization.parameters["realm"] = realm;
}

void
HTTP::OAuth::getRequestToken()
{
    ASSERT(m_requestTokenMethod == HTTP::GET || m_requestTokenMethod == HTTP::POST);
    URI::QueryString qs;
 
    qs.insert(std::make_pair("oauth_consumer_key", m_consumerKey));
    qs.insert(std::make_pair("oauth_version", "1.0"));
    if (!m_callbackUri.isDefined())
        qs.insert(std::make_pair("oauth_callback", "oob"));
    else
        qs.insert(std::make_pair("oauth_callback", m_callbackUri.toString()));
    nonceAndTimestamp(qs);
    sign(m_requestTokenUri, m_requestTokenMethod, m_requestTokenSignatureMethod, qs);

    HTTP::Request requestHeaders;
    requestHeaders.requestLine.method = m_requestTokenMethod;
    requestHeaders.requestLine.uri = m_requestTokenUri;
    std::string body;
    if (m_requestTokenMethod == HTTP::GET) {
        // Add parameters that are part of the request token URI
        URI::QueryString qsFromUri = m_requestTokenUri.queryString();
        qs.insert(qsFromUri.begin(), qsFromUri.end());
        requestHeaders.requestLine.uri.query(qs);
    } else {
        body = qs.toString();
        requestHeaders.entity.contentType.type = "application";
        requestHeaders.entity.contentType.subtype = "x-www-form-urlencoded";
        requestHeaders.entity.contentLength = body.size();
    }

    HTTP::ClientRequest::ptr request =
        m_connDg(m_requestTokenUri)->request(requestHeaders);
    if (!body.empty()) {
        request->requestStream()->write(body.c_str(), body.size());
        request->requestStream()->close();
    }
    if (request->response().status.status != HTTP::OK) {
        request->cancel();
        MORDOR_THROW_EXCEPTION(HTTP::InvalidResponseException("", request->response()));
    }

    MemoryStream responseStream;
    transferStream(request->responseStream(), responseStream);
    std::string response;
    response.resize(responseStream.buffer().readAvailable());
    responseStream.buffer().copyOut(&response[0], responseStream.buffer().readAvailable());
    m_params = response;
    URI::QueryString::iterator it = m_params.find("oauth_token");
    if (it == m_params.end())
        MORDOR_THROW_EXCEPTION(HTTP::InvalidResponseException("Missing oauth_token in response",
            request->response()));
    ++it;
    if (it != m_params.end() &&
        stricmp(it->first.c_str(), "oauth_token") == 0)
        MORDOR_THROW_EXCEPTION(HTTP::InvalidResponseException("Duplicate oauth_token in response",
            request->response()));
    it = m_params.find("oauth_token_secret");
    if (it == m_params.end())
        MORDOR_THROW_EXCEPTION(HTTP::InvalidResponseException("Missing oauth_token_secret in response",
            request->response()));
    ++it;
    if (it != m_params.end() &&
        stricmp(it->first.c_str(), "oauth_token_secret") == 0)
        MORDOR_THROW_EXCEPTION(HTTP::InvalidResponseException("Duplicate oauth_token_secret in response",
            request->response()));
}

void
HTTP::OAuth::getAccessToken(const std::string &verifier)
{
    ASSERT(m_accessTokenMethod == HTTP::GET || m_accessTokenMethod == HTTP::POST);
    URI::QueryString qs;

    qs.insert(std::make_pair("oauth_consumer_key", m_consumerKey));
    qs.insert(*m_params.find("oauth_token"));
    qs.insert(std::make_pair("oauth_verifier", verifier));
    qs.insert(std::make_pair("oauth_version", "1.0"));
    nonceAndTimestamp(qs);
    sign(m_accessTokenUri, m_accessTokenMethod, m_requestTokenSignatureMethod, qs);

    HTTP::Request requestHeaders;
    requestHeaders.requestLine.method = m_accessTokenMethod;
    requestHeaders.requestLine.uri = m_accessTokenUri;
    std::string body;
    if (m_accessTokenMethod == HTTP::GET) {
        // Add parameters that are part of the request token URI
        URI::QueryString qsFromUri = m_accessTokenUri.queryString();
        qs.insert(qsFromUri.begin(), qsFromUri.end());
        requestHeaders.requestLine.uri.query(qs);
    } else {
        body = qs.toString();
        requestHeaders.entity.contentType.type = "application";
        requestHeaders.entity.contentType.subtype = "x-www-form-urlencoded";
        requestHeaders.entity.contentLength = body.size();
    }

    HTTP::ClientRequest::ptr request =
        m_connDg(m_accessTokenUri)->request(requestHeaders);
    if (!body.empty()) {
        request->requestStream()->write(body.c_str(), body.size());
        request->requestStream()->close();
    }
    if (request->response().status.status != HTTP::OK) {
        request->cancel();
        MORDOR_THROW_EXCEPTION(HTTP::InvalidResponseException("", request->response()));
    }

    MemoryStream responseStream;
    transferStream(request->responseStream(), responseStream);
    std::string response;
    response.resize(responseStream.buffer().readAvailable());
    responseStream.buffer().copyOut(&response[0], responseStream.buffer().readAvailable());
    m_params = response;
    URI::QueryString::iterator it = m_params.find("oauth_token");
    if (it == m_params.end())
        MORDOR_THROW_EXCEPTION(HTTP::InvalidResponseException("Missing oauth_token in response",
            request->response()));
    ++it;
    if (it != m_params.end() &&
        stricmp(it->first.c_str(), "oauth_token") == 0)
        MORDOR_THROW_EXCEPTION(HTTP::InvalidResponseException("Duplicate oauth_token in response",
            request->response()));
    it = m_params.find("oauth_token_secret");
    if (it == m_params.end())
        MORDOR_THROW_EXCEPTION(HTTP::InvalidResponseException("Missing oauth_token_secret in response",
            request->response()));
    ++it;
    if (it != m_params.end() &&
        stricmp(it->first.c_str(), "oauth_token_secret") == 0)
        MORDOR_THROW_EXCEPTION(HTTP::InvalidResponseException("Duplicate oauth_token_secret in response",
            request->response()));
}

URI::QueryString
HTTP::OAuth::signRequest(const URI &uri, Method method,
                         const std::string &signatureMethod)
{
    URI::QueryString result;
    result.insert(std::make_pair("oauth_consumer_key", m_consumerKey));
    result.insert(*m_params.find("oauth_token"));
    result.insert(std::make_pair("oauth_version", "1.0"));
    nonceAndTimestamp(result);
    sign(uri, method, signatureMethod, result);
    return result;
}

void
HTTP::OAuth::nonceAndTimestamp(URI::QueryString &params)
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
HTTP::OAuth::sign(const URI &uri, Method method, const std::string &signatureMethod,
                  URI::QueryString &params)
{
    ASSERT(params.find("oauth_signature_method") == params.end());
    params.insert(std::make_pair("oauth_signature_method", signatureMethod));
    ASSERT(params.find("oauth_signature") == params.end());

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

    std::string secrets = URI::encode(m_consumerSecret);
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
        NOTREACHED();
    }
}
