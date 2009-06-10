// Copyright (c) 2009 - Decho Corp.

#include "basic.h"

#include "common/string.h"

bool
HTTP::BasicClientAuthenticationScheme::authorize(ClientRequest::ptr challenge, Request &nextRequest, bool proxy)
{
    bool result = false;
    std::string username;
    std::string password;

    if (challenge.get() && m_authDg) {
        const ParameterizedList &authenticate = proxy ?
            challenge->response().response.proxyAuthenticate : challenge->response().response.wwwAuthenticate;
        for (ParameterizedList::const_iterator it(authenticate.begin());
            it != authenticate.end();
            ++it) {
            if (stricmp(it->value.c_str(), "Basic") == 0) {
                std::string realm;
                StringMap::const_iterator realmIt = it->parameters.find("realm");
                if (realmIt != it->parameters.end())
                    realm = realmIt->second;
                result = m_authDg(nextRequest.requestLine.uri, realm, proxy, username, password);
                if (result)
                    break;
            }
        }
    }
    if (!result && m_preauthDg) {
        result = m_preauthDg(nextRequest.requestLine.uri, proxy, username, password);
    }
    if (result) {
        ValueWithParameters &credentials = proxy ? nextRequest.request.proxyAuthorization :
            nextRequest.request.authorization;
        credentials.value = "Basic";
        username = username + ":" + password;
        username = base64encode(username);
        credentials.parameters[username] = "";
    }
    return result;
}