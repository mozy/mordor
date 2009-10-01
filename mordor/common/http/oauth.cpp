// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "oauth.h"

#include "mordor/common/streams/memory.h"
#include "mordor/common/streams/transfer.h"

void
HTTP::OAuth::authorize(Request &nextRequest)
{
    if (m_params.find("oauth_token_secret") == m_params.end() ||
        m_params.find("oauth_token") == m_params.end()) {
        getRequestToken();
        getAccessToken(m_authDg(m_params));
    }
    AuthParams &authorization = nextRequest.request.authorization;
    authorization.scheme = "OAuth";
    URI::QueryString params = signRequest(nextRequest.requestLine.uri,
        nextRequest.requestLine.method);
    authorization.parameters.clear();
    authorization.parameters.insert(params.begin(), params.end());
}

void
HTTP::OAuth::getRequestToken()
{
    ASSERT(m_requestTokenMethod == HTTP::GET || m_requestTokenMethod == HTTP::POST);
    URI::QueryString qs;
 
    qs.insert(std::make_pair("oauth_consumer_key", m_consumerKey));
    qs.insert(std::make_pair("oauth_version", "1.0"));
    qs.insert(std::make_pair("oauth_callback", "oob"));
    nonceAndTimestamp(qs);
    sign(m_requestTokenUri, m_requestTokenMethod, qs);

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
        throw HTTP::InvalidResponseException("", request->response());
    }

    MemoryStream responseStream;
    transferStream(request->responseStream(), responseStream);
    std::string response;
    response.resize(responseStream.buffer().readAvailable());
    responseStream.buffer().copyOut(&response[0], responseStream.buffer().readAvailable());
    m_params = response;
    URI::QueryString::iterator it = m_params.find("oauth_token");
    if (it == m_params.end())
        throw HTTP::InvalidResponseException("Missing oauth_token in response",
            request->response());
    ++it;
    if (it != m_params.end() &&
        stricmp(it->first.c_str(), "oauth_token") == 0)
        throw HTTP::InvalidResponseException("Duplicate oauth_token in response",
            request->response());
    it = m_params.find("oauth_token_secret");
    if (it == m_params.end())
        throw HTTP::InvalidResponseException("Missing oauth_token_secret in response",
            request->response());
    ++it;
    if (it != m_params.end() &&
        stricmp(it->first.c_str(), "oauth_token_secret") == 0)
        throw HTTP::InvalidResponseException("Duplicate oauth_token_secret in response",
            request->response());
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
    sign(m_accessTokenUri, m_accessTokenMethod, qs);

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
        throw HTTP::InvalidResponseException("", request->response());
    }

    MemoryStream responseStream;
    transferStream(request->responseStream(), responseStream);
    std::string response;
    response.resize(responseStream.buffer().readAvailable());
    responseStream.buffer().copyOut(&response[0], responseStream.buffer().readAvailable());
    m_params = response;
    URI::QueryString::iterator it = m_params.find("oauth_token");
    if (it == m_params.end())
        throw HTTP::InvalidResponseException("Missing oauth_token in response",
            request->response());
    ++it;
    if (it != m_params.end() &&
        stricmp(it->first.c_str(), "oauth_token") == 0)
        throw HTTP::InvalidResponseException("Duplicate oauth_token in response",
            request->response());
    it = m_params.find("oauth_token_secret");
    if (it == m_params.end())
        throw HTTP::InvalidResponseException("Missing oauth_token_secret in response",
            request->response());
    ++it;
    if (it != m_params.end() &&
        stricmp(it->first.c_str(), "oauth_token_secret") == 0)
        throw HTTP::InvalidResponseException("Duplicate oauth_token_secret in response",
            request->response());
}

URI::QueryString
HTTP::OAuth::signRequest(const URI &uri, Method method)
{
    URI::QueryString result;
    result.insert(std::make_pair("oauth_consumer_key", m_consumerKey));
    result.insert(*m_params.find("oauth_token"));
    result.insert(std::make_pair("oauth_version", "1.0"));
    nonceAndTimestamp(result);
    sign(uri, method, result);
    return result;
}

void
HTTP::OAuth::nonceAndTimestamp(URI::QueryString &params)
{
    // TODO: fill in with better data
    params.insert(std::make_pair("oauth_timestamp", "123"));
    params.insert(std::make_pair("oauth_nonce", "abc"));
}

void
HTTP::OAuth::sign(URI uri, Method method, URI::QueryString &params)
{
    std::string signatureMethod;
    URI::QueryString::iterator it = params.find("oauth_signature_method");
    if (it == params.end()) {
        params.insert(std::make_pair("oauth_signature_method", "PLAINTEXT"));
        signatureMethod = "PLAINTEXT";
    } else {
        signatureMethod = it->second;
    }
    it = params.find("oauth_signature");
    if (it != params.end())
        params.erase(it);

    std::ostringstream os;
    uri.queryDefined(false);
    uri.fragmentDefined(false);
    uri.normalize();
    os << method << '&' << uri;
    URI::QueryString combined = params;
    it = combined.find("realm");
    if (it != combined.end())
        combined.erase(it);
    // TODO: POST params of application/x-www-form-urlencoded
    if (uri.queryDefined()) {
        URI::QueryString queryParams = uri.queryString();
        combined.insert(queryParams.begin(), queryParams.end());
    }
    
    // TODO: ordering of duplicate keys
    std::string signatureBaseString = combined.toString();

    std::string secrets = URI::encode(m_consumerSecret, URI::QUERYSTRING);
    secrets.append(1, '&');
    secrets.append(URI::encode(m_params.find("oauth_token_secret")->second,
        URI::QUERYSTRING));

    if (stricmp(signatureMethod.c_str(), "HMAC-SHA1") == 0) {
        params.insert(std::make_pair("oauth_signature",
            hmacSha1(signatureBaseString, secrets)));
    } else if (stricmp(signatureMethod.c_str(), "PLAINTEXT") == 0) {
        params.insert(std::make_pair("oauth_signature", secrets));
    } else {
        NOTREACHED();
    }
}
