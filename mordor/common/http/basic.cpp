// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "basic.h"

#include "mordor/common/string.h"

void
HTTP::BasicAuth::authorize(Request &nextRequest,
                                                 const std::string &username,
                                                 const std::string &password,
                                                 bool proxy)
{
    AuthParams &authorization = proxy ?
        nextRequest.request.proxyAuthorization :
        nextRequest.request.authorization;
    authorization.scheme = "Basic";
    authorization.base64 = base64encode(username + ":" + password);
    authorization.parameters.clear();
}
