// Copyright (c) 2009 - Decho Corporation

#include "mordor/http/oauth.h"
#include "mordor/test/test.h"

using namespace Mordor::HTTP;

MORDOR_UNITTEST(OAuth, oauthExample)
{
    std::pair<std::string, std::string>
        clientCredentials("dpf43f3p2l4k3l03", "kd94hf93k423kf44"),
        temporaryCredentials("hh5s93j4hdidpola", "hdhd0244k9j7ao03"),
        tokenCredentials("nnch734d00sl2jdk", "pfkkdhi9sl3r4s00");
    std::string verifier = "hfdp7dh39dks9884";

    StringMap oauthParameters;
    oauthParameters["oauth_consumer_key"] = clientCredentials.first;
    oauthParameters["oauth_version"] = "1.0";
    oauthParameters["oauth_callback"] =
        "http://printer.example.com/request_token_ready";
    oauthParameters["oauth_timestamp"] = "1191242090";
    oauthParameters["oauth_nonce"] = "hsu94j3884jdopsl";
    OAuth::sign("https://photos.example.net/request_token", POST, "PLAINTEXT",
        clientCredentials.second, std::string(), oauthParameters);
    MORDOR_ASSERT(oauthParameters.find("oauth_signature") !=
        oauthParameters.end());
    MORDOR_TEST_ASSERT_EQUAL(oauthParameters["oauth_signature"],
        "kd94hf93k423kf44&");
    MORDOR_ASSERT(OAuth::validate("https://photos.example.net/request_token",
        POST, clientCredentials.second, std::string(), oauthParameters));

    oauthParameters.erase("oauth_callback");
    oauthParameters["oauth_token"] = temporaryCredentials.first;
    oauthParameters["oauth_verifier"] = verifier;
    oauthParameters["oauth_timestamp"] = "1191242092";
    oauthParameters["oauth_nonce"] = "dji430splmx33448";
    OAuth::sign("https://photos.example.net/access_token", POST, "PLAINTEXT",
        clientCredentials.second, temporaryCredentials.second,
        oauthParameters);
    MORDOR_ASSERT(oauthParameters.find("oauth_signature") !=
        oauthParameters.end());
    MORDOR_TEST_ASSERT_EQUAL(oauthParameters["oauth_signature"],
        "kd94hf93k423kf44&hdhd0244k9j7ao03");
    MORDOR_ASSERT(OAuth::validate("https://photos.example.net/access_token",
        POST, clientCredentials.second, temporaryCredentials.second,
        oauthParameters));

    oauthParameters.erase("oauth_verifier");
    oauthParameters["oauth_token"] = tokenCredentials.first;
    oauthParameters["oauth_timestamp"] = "1191242096";
    oauthParameters["oauth_nonce"] = "kllo9940pd9333jh";
    OAuth::sign("http://photos.example.net/photos?file=vacation.jpg&size=original",
        GET, "HMAC-SHA1", clientCredentials.second, tokenCredentials.second,
        oauthParameters);
    MORDOR_ASSERT(oauthParameters.find("oauth_signature") !=
        oauthParameters.end());
    MORDOR_TEST_ASSERT_EQUAL(oauthParameters["oauth_signature"],
        "tR3+Ty81lMeYAr/Fid0kMTYa/WM=");
    MORDOR_ASSERT(OAuth::validate("http://photos.example.net/photos?file=vacation.jpg&size=original",
        GET, clientCredentials.second, tokenCredentials.second,
        oauthParameters));
}
