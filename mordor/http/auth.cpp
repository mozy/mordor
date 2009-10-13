// Copyright (c) 2009 - Decho Corp.

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

}}
