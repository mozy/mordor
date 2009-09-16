// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "digest.h"

#include "mordor/common/string.h"
#include "mordor/common/timer.h"
#include "parser.h"

void
HTTP::DigestAuth::authorize(const Response &challenge, Request &nextRequest,
                            const std::string &username,
                            const std::string &password)
{
    ASSERT(challenge.status.status == UNAUTHORIZED ||
        challenge.status.status == PROXY_AUTHENTICATION_REQUIRED);
    bool proxy = challenge.status.status == PROXY_AUTHENTICATION_REQUIRED;
    const ChallengeList &authenticate = proxy ?
        challenge.response.proxyAuthenticate :
        challenge.response.wwwAuthenticate;
    AuthParams &authorization = proxy ?
        nextRequest.request.proxyAuthorization :
        nextRequest.request.authorization;
    const StringMap *params = NULL;
    for (ChallengeList::const_iterator it = authenticate.begin();
        it != authenticate.end();
        ++it) {
        if (stricmp(it->scheme.c_str(), "Digest") == 0) {
            params = &it->parameters;
            break;
        }
    }
    ASSERT(params);

    std::string realm, qop, nonce, opaque, algorithm;
    StringMap::const_iterator it;
    if ( (it = params->find("realm")) != params->end()) realm = it->second;
    if ( (it = params->find("qop")) != params->end()) qop = it->second;
    if ( (it = params->find("nonce")) != params->end()) nonce = it->second;
    if ( (it = params->find("opaque")) != params->end()) opaque = it->second;
    if ( (it = params->find("algorithm")) != params->end()) algorithm = it->second;

    if (algorithm.empty())
        algorithm = "MD5";
    StringSet qopValues;
    bool authQop = false;
    // If the server specified a quality of protection (qop), make sure it allows "auth"
    if (!qop.empty()) {
        ListParser parser(qopValues);
        parser.run(qop);
        if (parser.error() || !parser.complete())
            throw BadMessageHeaderException();
        if (qopValues.find("auth") == qopValues.end())
            throw InvalidDigestQopException(qop);
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
        throw InvalidDigestAlgorithmException(algorithm);
    
    // compute A2 - our qop is always auth or unspecified
    os.str("");
    os << nextRequest.requestLine.method << ':' << nextRequest.requestLine.uri;
    std::string A2 = os.str();

    authorization.scheme = "Digest";
    authorization.base64.clear();
    authorization.parameters["username"] = username;
    authorization.parameters["realm"] = realm;
    authorization.parameters["nonce"] = nonce;
    authorization.parameters["uri"] = nextRequest.requestLine.uri.toString();
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
