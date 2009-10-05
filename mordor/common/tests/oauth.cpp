// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "http_helper.h"
#include "mordor/common/http/oauth.h"
#include "mordor/common/http/server.h"
#include "mordor/common/scheduler.h"
#include "mordor/common/streams/memory.h"
#include "mordor/common/streams/transfer.h"
#include "mordor/test/test.h"

static void
oauthExampleServer(const URI &uri, HTTP::ServerRequest::ptr request)
{
    TEST_ASSERT_EQUAL(request->request().requestLine.method, HTTP::POST);        
    TEST_ASSERT_EQUAL(request->request().entity.contentType.type, "application");
    TEST_ASSERT_EQUAL(request->request().entity.contentType.subtype, "x-www-form-urlencoded");
    MemoryStream requestBody;
    transferStream(request->requestStream(), requestBody);
    std::string queryString;
    queryString.resize(requestBody.buffer().readAvailable());
    requestBody.buffer().copyOut(&queryString[0], requestBody.buffer().readAvailable());        

    URI::QueryString qs(queryString);
    if (uri.path == "/request_token") {
        TEST_ASSERT_EQUAL(uri, "https://photos.example.net/request_token");
        
        TEST_ASSERT_EQUAL(qs.size(), 7u);
        URI::QueryString::iterator it = qs.find("oauth_consumer_key");
        TEST_ASSERT(it != qs.end());
        TEST_ASSERT_EQUAL(it->second, "dpf43f3p2l4k3l03");
        it = qs.find("oauth_signature_method");
        TEST_ASSERT(it != qs.end());
        TEST_ASSERT_EQUAL(it->second, "PLAINTEXT");
        it = qs.find("oauth_signature");
        TEST_ASSERT(it != qs.end());
        TEST_ASSERT_EQUAL(it->second, "kd94hf93k423kf44&");
        it = qs.find("oauth_timestamp");
        TEST_ASSERT(it != qs.end());
        TEST_ASSERT_EQUAL(it->second, "1191242090");
        it = qs.find("oauth_nonce");
        TEST_ASSERT(it != qs.end());
        TEST_ASSERT_EQUAL(it->second, "hsu94j3884jdopsl");
        it = qs.find("oauth_version");
        TEST_ASSERT(it != qs.end());
        TEST_ASSERT_EQUAL(it->second, "1.0");
        it = qs.find("oauth_callback");
        TEST_ASSERT(it != qs.end());
        TEST_ASSERT_EQUAL(it->second, "http://printer.example.com/request_token_ready");

        qs.clear();
        qs.insert(std::make_pair("oauth_token", "hh5s93j4hdidpola"));
        qs.insert(std::make_pair("oauth_token_secret", "hdhd0244k9j7ao03"));
        qs.insert(std::make_pair("oauth_callback_confirmed", "true"));
    } else if (uri.path == "/access_token") {
        TEST_ASSERT_EQUAL(uri, "https://photos.example.net/access_token");

        TEST_ASSERT_EQUAL(qs.size(), 8u);
        URI::QueryString::iterator it = qs.find("oauth_consumer_key");
        TEST_ASSERT(it != qs.end());
        TEST_ASSERT_EQUAL(it->second, "dpf43f3p2l4k3l03");
        it = qs.find("oauth_token");
        TEST_ASSERT(it != qs.end());
        TEST_ASSERT_EQUAL(it->second, "hh5s93j4hdidpola");
        it = qs.find("oauth_signature_method");
        TEST_ASSERT(it != qs.end());
        TEST_ASSERT_EQUAL(it->second, "PLAINTEXT");
        it = qs.find("oauth_signature");
        TEST_ASSERT(it != qs.end());
        TEST_ASSERT_EQUAL(it->second, "kd94hf93k423kf44&hdhd0244k9j7ao03");
        it = qs.find("oauth_timestamp");
        TEST_ASSERT(it != qs.end());
        TEST_ASSERT_EQUAL(it->second, "1191242092");
        it = qs.find("oauth_nonce");
        TEST_ASSERT(it != qs.end());
        TEST_ASSERT_EQUAL(it->second, "dji430splmx33448");
        it = qs.find("oauth_version");
        TEST_ASSERT(it != qs.end());
        TEST_ASSERT_EQUAL(it->second, "1.0");
        it = qs.find("oauth_verifier");
        TEST_ASSERT(it != qs.end());
        TEST_ASSERT_EQUAL(it->second, "hfdp7dh39dks9884");

        qs.clear();
        qs.insert(std::make_pair("oauth_token", "nnch734d00sl2jdk"));
        qs.insert(std::make_pair("oauth_token_secret", "pfkkdhi9sl3r4s00"));
    } else {
        NOTREACHED();
    }
    std::string response = qs.toString();
    request->response().entity.contentLength = response.size();
    request->response().status.status = HTTP::OK;
    request->responseStream()->write(response.c_str(), response.size());
    request->responseStream()->close();
}

static std::string
oauthExampleAuth(const URI::QueryString &params)
{
    TEST_ASSERT_GREATER_THAN_OR_EQUAL(params.size(), 1u);
    URI::QueryString::const_iterator it = params.find("oauth_token");
    TEST_ASSERT(it != params.end());
    TEST_ASSERT_EQUAL(it->second, "hh5s93j4hdidpola");
    return "hfdp7dh39dks9884";
}

static std::pair<unsigned long long, std::string>
getTimestampAndNonce()
{
    static int sequence = 0;
    switch (++sequence) {
        case 1:
            return std::make_pair(1191242090ull,
                std::string("hsu94j3884jdopsl"));
        case 2:
            return std::make_pair(1191242092ull,
                std::string("dji430splmx33448"));
        case 3:
            return std::make_pair(1191242096ull,
                std::string("kllo9940pd9333jh"));
        default:
            NOTREACHED();
    }
}

TEST_WITH_SUITE(OAuth, oauthExample)
{
    Fiber::ptr mainFiber(new Fiber());
    WorkerPool pool;
    HTTPHelper helper(&oauthExampleServer);

    HTTP::OAuth oauth(boost::bind(&HTTPHelper::getConn, &helper, _1),
        &oauthExampleAuth,
        "https://photos.example.net/request_token", HTTP::POST, "PLAINTEXT",
        "https://photos.example.net/access_token", HTTP::POST, "PLAINTEXT",
        "dpf43f3p2l4k3l03",
        "kd94hf93k423kf44",
        "http://printer.example.com/request_token_ready");
    oauth.selfNonce(&getTimestampAndNonce);

    HTTP::Request requestHeaders;
    requestHeaders.requestLine.uri =
        "http://photos.example.net/photos?file=vacation.jpg&size=original";
    oauth.authorize(requestHeaders, "HMAC-SHA1", "http://photos.example.net/");
    TEST_ASSERT_EQUAL(requestHeaders.request.authorization.scheme, "OAuth");
    HTTP::StringMap &params = requestHeaders.request.authorization.parameters;
    TEST_ASSERT_EQUAL(params.size(), 8u);
    HTTP::StringMap::iterator it = params.find("realm");
    TEST_ASSERT(it != params.end());
    TEST_ASSERT_EQUAL(it->second, "http://photos.example.net/");
    it = params.find("oauth_consumer_key");
    TEST_ASSERT(it != params.end());
    TEST_ASSERT_EQUAL(it->second, "dpf43f3p2l4k3l03");
    it = params.find("oauth_token");
    TEST_ASSERT(it != params.end());
    TEST_ASSERT_EQUAL(it->second, "nnch734d00sl2jdk");
    it = params.find("oauth_signature_method");
    TEST_ASSERT(it != params.end());
    TEST_ASSERT_EQUAL(it->second, "HMAC-SHA1");
    it = params.find("oauth_signature");
    TEST_ASSERT(it != params.end());
    TEST_ASSERT_EQUAL(it->second, "tR3+Ty81lMeYAr/Fid0kMTYa/WM=");
    it = params.find("oauth_timestamp");
    TEST_ASSERT(it != params.end());
    TEST_ASSERT_EQUAL(it->second, "1191242096");
    it = params.find("oauth_nonce");
    TEST_ASSERT(it != params.end());
    TEST_ASSERT_EQUAL(it->second, "kllo9940pd9333jh");
    it = params.find("oauth_version");
    TEST_ASSERT(it != params.end());
    TEST_ASSERT_EQUAL(it->second, "1.0");
}
