#ifndef __MORDOR_HTTP_BASIC_AUTH_H__
#define __MORDOR_HTTP_BASIC_AUTH_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "http.h"

namespace Mordor {
namespace HTTP {

class BasicAuth
{
public:
    static void authorize(Request &nextRequest,
        const std::string &username, const std::string &password,
        bool proxy = false);
};

}}

#endif
