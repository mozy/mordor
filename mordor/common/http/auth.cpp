// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "auth.h"

#include "basic.h"
#include "digest.h"

HTTP::ClientRequest::ptr
HTTP::ClientAuthBroker::request(Request &requestHeaders,
                                boost::function< void (ClientRequest::ptr)> dg)
{
    if (!m_conn)
        m_conn = m_dg();
    
    bool triedWwwAuth = false;
    bool triedProxyAuth = false;
    while (true) {
        try {
            HTTP::ClientRequest::ptr request = m_conn->request(requestHeaders);
            if (dg)
                dg(request);
            const HTTP::Response &responseHeaders = request->response();
            if (responseHeaders.status.status == HTTP::UNAUTHORIZED ||
                responseHeaders.status.status == HTTP::PROXY_AUTHENTICATION_REQUIRED)
            {
                bool proxy = responseHeaders.status.status == HTTP::PROXY_AUTHENTICATION_REQUIRED;
                if (proxy && triedProxyAuth)
                    return request;
                if (!proxy && triedWwwAuth)
                    return request;
                const ParameterizedList &authenticate = proxy ?
                    responseHeaders.response.proxyAuthenticate :
                    responseHeaders.response.wwwAuthenticate;
                bool hasCreds = proxy ?
                    (!m_proxyUsername.empty() || !m_proxyPassword.empty()) :
                    (!m_username.empty() || !m_password.empty());
                if (isAcceptable(authenticate, "Digest") && hasCreds) {
                    HTTP::DigestAuth::authorize(responseHeaders, requestHeaders,
                        proxy ? m_proxyUsername : m_username,
                        proxy ? m_proxyPassword : m_password);
                    request->finish();
                } else if (isAcceptable(authenticate, "Basic") && hasCreds) {
                    HTTP::BasicAuth::authorize(requestHeaders,
                        proxy ? m_proxyUsername : m_username,
                        proxy ? m_proxyPassword : m_password, proxy);
                    request->finish();
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
        } catch (HTTP::PriorRequestFailedException) {
            m_conn = m_dg();
            continue;
        }
    }
}
