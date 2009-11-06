// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "mordor/http/oauth.h"
#include "mordor/http/server.h"
#include "mordor/scheduler.h"
#include "mordor/streams/memory.h"
#include "mordor/streams/transfer.h"
#include "mordor/test/test.h"
#include "mordor/util.h"

using namespace Mordor;
using namespace Mordor::HTTP;
using namespace Mordor::Test;

static void
oauthExampleServer(const URI &uri, ServerRequest::ptr request)
{
    MORDOR_TEST_ASSERT_EQUAL(request->request().requestLine.method, POST);        
    MORDOR_TEST_ASSERT_EQUAL(request->request().entity.contentType.type, "application");
    MORDOR_TEST_ASSERT_EQUAL(request->request().entity.contentType.subtype, "x-www-form-urlencoded");
    MemoryStream requestBody;
    transferStream(request->requestStream(), requestBody);
    std::string queryString;
    queryString.resize(requestBody.buffer().readAvailable());
    requestBody.buffer().copyOut(&queryString[0], requestBody.buffer().readAvailable());        

    URI::QueryString qs(queryString);
    if (uri.path == "/request_token") {
        MORDOR_TEST_ASSERT_EQUAL(uri, "https://photos.example.net/request_token");
        
        MORDOR_TEST_ASSERT_EQUAL(qs.size(), 7u);
        URI::QueryString::iterator it = qs.find("oauth_consumer_key");
        MORDOR_TEST_ASSERT(it != qs.end());
        MORDOR_TEST_ASSERT_EQUAL(it->second, "dpf43f3p2l4k3l03");
        it = qs.find("oauth_signature_method");
        MORDOR_TEST_ASSERT(it != qs.end());
        MORDOR_TEST_ASSERT_EQUAL(it->second, "PLAINTEXT");
        it = qs.find("oauth_signature");
        MORDOR_TEST_ASSERT(it != qs.end());
        MORDOR_TEST_ASSERT_EQUAL(it->second, "kd94hf93k423kf44&");
        it = qs.find("oauth_timestamp");
        MORDOR_TEST_ASSERT(it != qs.end());
        MORDOR_TEST_ASSERT_EQUAL(it->second, "1191242090");
        it = qs.find("oauth_nonce");
        MORDOR_TEST_ASSERT(it != qs.end());
        MORDOR_TEST_ASSERT_EQUAL(it->second, "hsu94j3884jdopsl");
        it = qs.find("oauth_version");
        MORDOR_TEST_ASSERT(it != qs.end());
        MORDOR_TEST_ASSERT_EQUAL(it->second, "1.0");
        it = qs.find("oauth_callback");
        MORDOR_TEST_ASSERT(it != qs.end());
        MORDOR_TEST_ASSERT_EQUAL(it->second, "http://printer.example.com/request_token_ready");

        qs.clear();
        qs.insert(std::make_pair("oauth_token", "hh5s93j4hdidpola"));
        qs.insert(std::make_pair("oauth_token_secret", "hdhd0244k9j7ao03"));
        qs.insert(std::make_pair("oauth_callback_confirmed", "true"));
    } else if (uri.path == "/access_token") {
        MORDOR_TEST_ASSERT_EQUAL(uri, "https://photos.example.net/access_token");

        MORDOR_TEST_ASSERT_EQUAL(qs.size(), 8u);
        URI::QueryString::iterator it = qs.find("oauth_consumer_key");
        MORDOR_TEST_ASSERT(it != qs.end());
        MORDOR_TEST_ASSERT_EQUAL(it->second, "dpf43f3p2l4k3l03");
        it = qs.find("oauth_token");
        MORDOR_TEST_ASSERT(it != qs.end());
        MORDOR_TEST_ASSERT_EQUAL(it->second, "hh5s93j4hdidpola");
        it = qs.find("oauth_signature_method");
        MORDOR_TEST_ASSERT(it != qs.end());
        MORDOR_TEST_ASSERT_EQUAL(it->second, "PLAINTEXT");
        it = qs.find("oauth_signature");
        MORDOR_TEST_ASSERT(it != qs.end());
        MORDOR_TEST_ASSERT_EQUAL(it->second, "kd94hf93k423kf44&hdhd0244k9j7ao03");
        it = qs.find("oauth_timestamp");
        MORDOR_TEST_ASSERT(it != qs.end());
        MORDOR_TEST_ASSERT_EQUAL(it->second, "1191242092");
        it = qs.find("oauth_nonce");
        MORDOR_TEST_ASSERT(it != qs.end());
        MORDOR_TEST_ASSERT_EQUAL(it->second, "dji430splmx33448");
        it = qs.find("oauth_version");
        MORDOR_TEST_ASSERT(it != qs.end());
        MORDOR_TEST_ASSERT_EQUAL(it->second, "1.0");
        it = qs.find("oauth_verifier");
        MORDOR_TEST_ASSERT(it != qs.end());
        MORDOR_TEST_ASSERT_EQUAL(it->second, "hfdp7dh39dks9884");

        qs.clear();
        qs.insert(std::make_pair("oauth_token", "nnch734d00sl2jdk"));
        qs.insert(std::make_pair("oauth_token_secret", "pfkkdhi9sl3r4s00"));
    } else {
        MORDOR_NOTREACHED();
    }
    std::string response = qs.toString();
    request->response().entity.contentLength = response.size();
    request->response().status.status = OK;
    request->responseStream()->write(response.c_str(), response.size());
    request->responseStream()->close();
}

static std::string
oauthExampleAuth(const URI::QueryString &params)
{
    MORDOR_TEST_ASSERT_GREATER_THAN_OR_EQUAL(params.size(), 1u);
    URI::QueryString::const_iterator it = params.find("oauth_token");
    MORDOR_TEST_ASSERT(it != params.end());
    MORDOR_TEST_ASSERT_EQUAL(it->second, "hh5s93j4hdidpola");
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
            MORDOR_NOTREACHED();
    }
}

MORDOR_UNITTEST(OAuth, oauthExample)
{
    Fiber::ptr mainFiber(new Fiber());
    WorkerPool pool;
    MockConnectionBroker server(&oauthExampleServer);
    BaseRequestBroker requestBroker(ConnectionBroker::ptr(&server, &nop<ConnectionBroker *>));

    OAuth oauth(RequestBroker::ptr(&requestBroker, &nop<RequestBroker *>),
        &oauthExampleAuth,
        "https://photos.example.net/request_token", POST, "PLAINTEXT",
        "https://photos.example.net/access_token", POST, "PLAINTEXT",
        "dpf43f3p2l4k3l03",
        "kd94hf93k423kf44",
        "http://printer.example.com/request_token_ready");
    oauth.selfNonce(&getTimestampAndNonce);

    Request requestHeaders;
    requestHeaders.requestLine.uri =
        "http://photos.example.net/photos?file=vacation.jpg&size=original";
    oauth.authorize(requestHeaders, "HMAC-SHA1", "http://photos.example.net/");
    MORDOR_TEST_ASSERT_EQUAL(requestHeaders.request.authorization.scheme, "OAuth");
    StringMap &params = requestHeaders.request.authorization.parameters;
    MORDOR_TEST_ASSERT_EQUAL(params.size(), 8u);
    StringMap::iterator it = params.find("realm");
    MORDOR_TEST_ASSERT(it != params.end());
    MORDOR_TEST_ASSERT_EQUAL(it->second, "http://photos.example.net/");
    it = params.find("oauth_consumer_key");
    MORDOR_TEST_ASSERT(it != params.end());
    MORDOR_TEST_ASSERT_EQUAL(it->second, "dpf43f3p2l4k3l03");
    it = params.find("oauth_token");
    MORDOR_TEST_ASSERT(it != params.end());
    MORDOR_TEST_ASSERT_EQUAL(it->second, "nnch734d00sl2jdk");
    it = params.find("oauth_signature_method");
    MORDOR_TEST_ASSERT(it != params.end());
    MORDOR_TEST_ASSERT_EQUAL(it->second, "HMAC-SHA1");
    it = params.find("oauth_signature");
    MORDOR_TEST_ASSERT(it != params.end());
    MORDOR_TEST_ASSERT_EQUAL(it->second, "tR3+Ty81lMeYAr/Fid0kMTYa/WM=");
    it = params.find("oauth_timestamp");
    MORDOR_TEST_ASSERT(it != params.end());
    MORDOR_TEST_ASSERT_EQUAL(it->second, "1191242096");
    it = params.find("oauth_nonce");
    MORDOR_TEST_ASSERT(it != params.end());
    MORDOR_TEST_ASSERT_EQUAL(it->second, "kllo9940pd9333jh");
    it = params.find("oauth_version");
    MORDOR_TEST_ASSERT(it != params.end());
    MORDOR_TEST_ASSERT_EQUAL(it->second, "1.0");
}
