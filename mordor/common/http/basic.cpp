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
    ValueWithParameters &authorization = proxy ?
        nextRequest.request.proxyAuthorization :
        nextRequest.request.authorization;
    authorization.value = "Basic";
    authorization.parameters[base64encode(username + ":" + password)] = "";
}
