// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "auth.h"

#include "basic.h"
#include "digest.h"
#ifdef WINDOWS
#include "negotiate.h"
#endif

namespace Mordor {
namespace HTTP {

ClientRequest::ptr
ClientAuthBroker::request(Request &requestHeaders,
                          boost::function< void (ClientRequest::ptr)> dg)
{
    if (!m_conn)
        m_conn = m_dg();

    bool triedWwwAuth = false;
    bool triedProxyAuth = false;
#ifdef WINDOWS
    boost::scoped_ptr<NegotiateAuth> negotiateAuth, negotiateProxyAuth;
#endif
    while (true) {
        try {
            ClientRequest::ptr request = m_conn->request(requestHeaders);
            if (dg)
                dg(request);
            const Response &responseHeaders = request->response();
            if (responseHeaders.status.status == UNAUTHORIZED ||
                responseHeaders.status.status == PROXY_AUTHENTICATION_REQUIRED)
            {
                bool proxy = responseHeaders.status.status == PROXY_AUTHENTICATION_REQUIRED;
                if (proxy && triedProxyAuth)
                    return request;
                if (!proxy && triedWwwAuth)
                    return request;
                const ChallengeList &authenticate = proxy ?
                    responseHeaders.response.proxyAuthenticate :
                    responseHeaders.response.wwwAuthenticate;
                bool hasCreds = proxy ?
                    (!m_proxyUsername.empty() || !m_proxyPassword.empty()) :
                    (!m_username.empty() || !m_password.empty());
#ifdef WINDOWS
                if (isAcceptable(authenticate, "Negotiate") ||
                    isAcceptable(authenticate, "NTLM")) {
                    boost::scoped_ptr<NegotiateAuth> &auth = proxy ?
                        negotiateAuth : negotiateProxyAuth;
                    if (!auth.get()) {
                        auth.reset(new NegotiateAuth(
                            proxy ? m_proxyUsername : m_username,
                            proxy ? m_proxyPassword : m_password));
                    }
                    request->finish();
                    if (auth->authorize(responseHeaders, requestHeaders))
                        continue;
                    else
                        return request;
                } else
#endif
                if (isAcceptable(authenticate, "Digest") && hasCreds) {
                    request->finish();
                    DigestAuth::authorize(responseHeaders, requestHeaders,
                        proxy ? m_proxyUsername : m_username,
                        proxy ? m_proxyPassword : m_password);
                } else if (isAcceptable(authenticate, "Basic") && hasCreds) {
                    request->finish();
                    BasicAuth::authorize(requestHeaders,
                        proxy ? m_proxyUsername : m_username,
                        proxy ? m_proxyPassword : m_password, proxy);
                } else {
                    return request;
                }
                (proxy ? triedProxyAuth : triedWwwAuth) = true;
            } else {
                return request;
            }
        } catch (SocketException) {
            m_conn = m_dg();
            continue;
        } catch (PriorRequestFailedException) {
            m_conn = m_dg();
            continue;
        }
    }
}

static void authorize(const ChallengeList *challenge, AuthParams &authorization,
    const URI &uri, Method method, const std::string &scheme,
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

}}
