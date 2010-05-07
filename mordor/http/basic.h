#ifndef __MORDOR_HTTP_BASIC_AUTH_H__
#define __MORDOR_HTTP_BASIC_AUTH_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <string>

namespace Mordor {
namespace HTTP {

struct AuthParams;
struct Request;

namespace BasicAuth {

void authorize(AuthParams &authorization, const std::string &username,
    const std::string &password);

/// @deprecated
void authorize(Request &nextRequest,
    const std::string &username, const std::string &password,
    bool proxy = false);

}}}

#endif
