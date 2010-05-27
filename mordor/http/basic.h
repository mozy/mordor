#ifndef __MORDOR_HTTP_BASIC_AUTH_H__
#define __MORDOR_HTTP_BASIC_AUTH_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "http.h"

namespace Mordor {
namespace HTTP {
namespace BasicAuth {

void authorize(AuthParams &authorization, const std::string &username,
    const std::string &password);

/// @deprecated
inline void authorize(Request &nextRequest,
    const std::string &username, const std::string &password,
    bool proxy = false)
{ authorize(proxy ? nextRequest.request.proxyAuthorization :
    nextRequest.request.authorization, username, password); }

}}}

#endif
