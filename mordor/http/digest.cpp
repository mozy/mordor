// Copyright (c) 2009 - Mozy, Inc.

#include "digest.h"

#include "mordor/string.h"
#include "mordor/timer.h"
#include "parser.h"

namespace Mordor {
namespace HTTP {
namespace DigestAuth {

void authorize(const AuthParams &challenge, AuthParams &authorization,
    const URI &uri, const std::string &method, const std::string &username,
    const std::string &password)
{
    std::string realm, qop, nonce, opaque, algorithm;
    StringMap::const_iterator it;
    if ( (it = challenge.parameters.find("realm")) != challenge.parameters.end()) realm = it->second;
    if ( (it = challenge.parameters.find("qop")) != challenge.parameters.end()) qop = it->second;
    if ( (it = challenge.parameters.find("nonce")) != challenge.parameters.end()) nonce = it->second;
    if ( (it = challenge.parameters.find("opaque")) != challenge.parameters.end()) opaque = it->second;
    if ( (it = challenge.parameters.find("algorithm")) != challenge.parameters.end()) algorithm = it->second;

    if (algorithm.empty())
        algorithm = "MD5";
    StringSet qopValues;
    bool authQop = false;
    // If the server specified a quality of protection (qop), make sure it allows "auth"
    if (!qop.empty()) {
        ListParser parser(qopValues);
        parser.run(qop);
        if (parser.error() || !parser.complete())
            MORDOR_THROW_EXCEPTION(BadMessageHeaderException());
        if (qopValues.find("auth") == qopValues.end())
            MORDOR_THROW_EXCEPTION(InvalidQopException(qop));
        authQop = true;
    }

    // come up with a suitable client nonce
    std::ostringstream os;
    os << std::hex << TimerManager::now();
    std::string cnonce = os.str();
    std::string nc = "00000001";

    // compute A1
    std::string A1;
    if (algorithm == "MD5")
        A1 = username + ':' + realm + ':' + password;
    else if (algorithm == "MD5-sess")
        A1 = md5( username + ':' + realm + ':' + password ) + ':' + nonce + ':' + cnonce;
    else
        MORDOR_THROW_EXCEPTION(InvalidAlgorithmException(algorithm));

    // compute A2 - our qop is always auth or unspecified
    os.str("");
    os << method << ':' << uri;
    std::string A2 = os.str();

    authorization.scheme = "Digest";
    authorization.base64.clear();
    authorization.parameters["username"] = username;
    authorization.parameters["realm"] = realm;
    authorization.parameters["nonce"] = nonce;
    authorization.parameters["uri"] = uri.toString();
    authorization.parameters["algorithm"] = algorithm;

    std::string response;
    if (authQop) {
        qop = "auth";
        response = md5( md5(A1) + ':' + nonce + ':' + nc + ':' + cnonce + ':' + qop + ':' + md5(A2) );
        authorization.parameters["qop"] = qop;
        authorization.parameters["nc"] = nc;
        authorization.parameters["cnonce"] = cnonce;
    } else {
        response = md5( md5(A1) + ':' + nonce + ':' + md5(A2) );

    }
    authorization.parameters["response"] = response;
    if (!opaque.empty())
        authorization.parameters["opaque"] = opaque;
}

void
authorize(const Response &challenge, Request &nextRequest,
    const std::string &username, const std::string &password)
{
    MORDOR_ASSERT(challenge.status.status == UNAUTHORIZED ||
        challenge.status.status == PROXY_AUTHENTICATION_REQUIRED);
    bool proxy = challenge.status.status == PROXY_AUTHENTICATION_REQUIRED;
    const ChallengeList &authenticate = proxy ?
        challenge.response.proxyAuthenticate :
        challenge.response.wwwAuthenticate;
    AuthParams &authorization = proxy ?
        nextRequest.request.proxyAuthorization :
        nextRequest.request.authorization;
    authorize(challengeForSchemeAndRealm(authenticate, "Digest"),
        authorization, nextRequest.requestLine.uri,
        nextRequest.requestLine.method, username, password);
}

}}}
