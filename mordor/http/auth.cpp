// Copyright (c) 2009 - Mozy, Inc.

#include "auth.h"

#include "basic.h"
#include "client.h"
#include "digest.h"
#include "mordor/socket.h"

#ifdef WINDOWS
#include "negotiate.h"
#elif defined (OSX)
#include "mordor/util.h"

#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecKeychainSearch.h>
#endif

namespace Mordor {
namespace HTTP {

static void authorize(const ChallengeList *challenge, AuthParams &authorization,
    const URI &uri, const std::string &method, const std::string &scheme,
    const std::string &realm, const std::string &username,
    const std::string &password)
{
    if (stricmp(scheme.c_str(), "Basic") == 0) {
        BasicAuth::authorize(authorization, username, password);
    } else if (stricmp(scheme.c_str(), "Digest") == 0) {
        MORDOR_ASSERT(challenge);
        DigestAuth::authorize(
            challengeForSchemeAndRealm(*challenge, "Digest", realm),
            authorization, uri, method, username, password);
    }
}

ClientRequest::ptr
AuthRequestBroker::request(Request &requestHeaders, bool forceNewConnection,
    boost::function<void (ClientRequest::ptr)> bodyDg)
{
    ClientRequest::ptr priorRequest;
    std::string scheme, realm, username, password;
    size_t attempts = 0, proxyAttempts = 0;
#ifdef WINDOWS
    boost::scoped_ptr<NegotiateAuth> negotiateAuth, negotiateProxyAuth;
#endif
    while (true) {
        // Negotiate auth is a multi-request transaction; if we're in the
        // middle of one, just do the next step, and skip asking for
        // credentials again
#ifdef WINDOWS
        if (negotiateProxyAuth && !negotiateProxyAuth->authorize(
            challengeForSchemeAndRealm(priorRequest->response().response.proxyAuthenticate, scheme),
            requestHeaders.request.proxyAuthorization,
            requestHeaders.requestLine.uri))
            negotiateProxyAuth.reset();
        if (negotiateAuth && !negotiateAuth->authorize(
            challengeForSchemeAndRealm(priorRequest->response().response.wwwAuthenticate, scheme),
            requestHeaders.request.authorization,
            requestHeaders.requestLine.uri))
            negotiateAuth.reset();
        if (!negotiateAuth && !negotiateProxyAuth) {
#endif
            // If this is the first try, or the last one failed UNAUTHORIZED,
            // ask for credentials, and use them if we got them
            if ((!priorRequest ||
                priorRequest->response().status.status == UNAUTHORIZED) &&
                m_getCredentialsDg &&
                m_getCredentialsDg(requestHeaders.requestLine.uri, priorRequest,
                scheme, realm, username, password, attempts++)) {
#ifdef WINDOWS
                MORDOR_ASSERT(
                    stricmp(scheme.c_str(), "Negotiate") == 0 ||
                    stricmp(scheme.c_str(), "NTLM") == 0 ||
                    stricmp(scheme.c_str(), "Digest") == 0 ||
                    stricmp(scheme.c_str(), "Basic") == 0);
#else
                    MORDOR_ASSERT(
                    stricmp(scheme.c_str(), "Digest") == 0 ||
                    stricmp(scheme.c_str(), "Basic") == 0);
#endif
#ifdef WINDOWS
                if (scheme == "Negotiate" || scheme == "NTLM") {
                    negotiateAuth.reset(new NegotiateAuth(username, password));
                    negotiateAuth->authorize(
                        challengeForSchemeAndRealm(priorRequest->response().response.wwwAuthenticate, scheme),
                        requestHeaders.request.authorization,
                        requestHeaders.requestLine.uri);
                } else
#endif
                authorize(priorRequest ?
                    &priorRequest->response().response.wwwAuthenticate : NULL,
                    requestHeaders.request.authorization,
                    requestHeaders.requestLine.uri,
                    requestHeaders.requestLine.method,
                    scheme, realm, username, password);
            } else if (priorRequest &&
                priorRequest->response().status.status == UNAUTHORIZED) {
                // caller didn't want to retry
                return priorRequest;
            }
            // If this is the first try, or the last one failed (for a proxy)
            // ask for credentials, and use them if we got them
            if ((!priorRequest ||
                priorRequest->response().status.status ==
                    PROXY_AUTHENTICATION_REQUIRED) &&
                m_getProxyCredentialsDg &&
                m_getProxyCredentialsDg(requestHeaders.requestLine.uri,
                priorRequest, scheme, realm, username, password, proxyAttempts++)) {
#ifdef WINDOWS
                MORDOR_ASSERT(
                    stricmp(scheme.c_str(), "Negotiate") == 0 ||
                    stricmp(scheme.c_str(), "NTLM") == 0 ||
                    stricmp(scheme.c_str(), "Digest") == 0 ||
                    stricmp(scheme.c_str(), "Basic") == 0);
#else
                    MORDOR_ASSERT(
                    stricmp(scheme.c_str(), "Digest") == 0 ||
                    stricmp(scheme.c_str(), "Basic") == 0);
#endif
#ifdef WINDOWS
                if (scheme == "Negotiate" || scheme == "NTLM") {
                    negotiateProxyAuth.reset(new NegotiateAuth(username, password));
                    negotiateProxyAuth->authorize(
                        challengeForSchemeAndRealm(priorRequest->response().response.proxyAuthenticate, scheme),
                        requestHeaders.request.proxyAuthorization,
                        requestHeaders.requestLine.uri);
                } else
#endif
                authorize(priorRequest ?
                    &priorRequest->response().response.proxyAuthenticate : NULL,
                    requestHeaders.request.proxyAuthorization,
                    requestHeaders.requestLine.uri,
                    requestHeaders.requestLine.method,
                    scheme, realm, username, password);
            } else if (priorRequest &&
                priorRequest->response().status.status ==
                    PROXY_AUTHENTICATION_REQUIRED) {
                // Caller didn't want to retry
                return priorRequest;
            }
#ifdef WINDOWS
        }
#endif
        if (priorRequest) {
            priorRequest->finish();
        } else {
            // We're passed our pre-emptive authentication, regardless of what
            // actually happened
            attempts = 1;
            proxyAttempts = 1;
        }
        priorRequest = parent()->request(requestHeaders, forceNewConnection,
            bodyDg);
        const ChallengeList *challengeList = NULL;
        if (priorRequest->response().status.status == UNAUTHORIZED)
            challengeList = &priorRequest->response().response.wwwAuthenticate;
        if (priorRequest->response().status.status == PROXY_AUTHENTICATION_REQUIRED)
            challengeList = &priorRequest->response().response.proxyAuthenticate;
        if (challengeList &&
            (isAcceptable(*challengeList, "Basic") ||
            isAcceptable(*challengeList, "Digest")
#ifdef WINDOWS
            || isAcceptable(*challengeList, "Negotiate") ||
            isAcceptable(*challengeList, "NTLM")
#endif
            ))
            continue;
        return priorRequest;
    }
}

#ifdef OSX
bool getCredentialsFromKeychain(const URI &uri, ClientRequest::ptr priorRequest,
    std::string &scheme, std::string &realm, std::string &username,
    std::string &password, size_t attempts)
{
    if (attempts != 1)
        return false;
    bool proxy =
       priorRequest->response().status.status == PROXY_AUTHENTICATION_REQUIRED;
    const ChallengeList &challengeList = proxy ?
        priorRequest->response().response.proxyAuthenticate :
        priorRequest->response().response.wwwAuthenticate;
    if (isAcceptable(challengeList, "Basic"))
        scheme = "Basic";
    else if (isAcceptable(challengeList, "Digest"))
        scheme = "Digest";
    else
        return false;

    std::vector<SecKeychainAttribute> attrVector;
    std::string host = uri.authority.host();
    attrVector.push_back((SecKeychainAttribute){kSecServerItemAttr, host.size(),
       (void *)host.c_str()});

    UInt32 port = 0;
    if (uri.authority.portDefined()) {
        port = uri.authority.port();
        attrVector.push_back((SecKeychainAttribute){kSecPortItemAttr,
           sizeof(UInt32), &port});
    }
    SecProtocolType protocol;
    if (proxy && priorRequest->request().requestLine.method == CONNECT)
        protocol = kSecProtocolTypeHTTPSProxy;
    else if (proxy)
        protocol = kSecProtocolTypeHTTPProxy;
    else if (uri.scheme() == "https")
        protocol = kSecProtocolTypeHTTPS;
    else if (uri.scheme() == "http")
        protocol = kSecProtocolTypeHTTP;
    else
        MORDOR_NOTREACHED();
    attrVector.push_back((SecKeychainAttribute){kSecProtocolItemAttr,
        sizeof(SecProtocolType), &protocol});

    ScopedCFRef<SecKeychainSearchRef> search;
    SecKeychainAttributeList attrList;
    attrList.count = (UInt32)attrVector.size();
    attrList.attr = &attrVector[0];

    OSStatus status = SecKeychainSearchCreateFromAttributes(NULL,
        kSecInternetPasswordItemClass, &attrList, &search);
    if (status != 0)
        return false;
    ScopedCFRef<SecKeychainItemRef> item;
    status = SecKeychainSearchCopyNext(search, &item);
    if (status != 0)
        return false;
    SecKeychainAttributeInfo info;
    SecKeychainAttrType tag = kSecAccountItemAttr;
    CSSM_DB_ATTRIBUTE_FORMAT format = CSSM_DB_ATTRIBUTE_FORMAT_STRING;
    info.count = 1;
    info.tag = (UInt32 *)&tag;
    info.format = (UInt32 *)&format;
    
    SecKeychainAttributeList *attrs;
    UInt32 passwordLength = 0;
    void *passwordBytes = NULL;

    status = SecKeychainItemCopyAttributesAndData(item, &info, NULL, &attrs,
        &passwordLength, &passwordBytes);
    if (status != 0)
        return false;

    try {
        username.assign((const char *)attrs->attr[0].data, attrs->attr[0].length);
        password.assign((const char *)passwordBytes, passwordLength);
    } catch (...) {
        SecKeychainItemFreeContent(attrs, passwordBytes);
        throw;
    }
    SecKeychainItemFreeContent(attrs, passwordBytes);
    return true;
}
#endif
	
}}
