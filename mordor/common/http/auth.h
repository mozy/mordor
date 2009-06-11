#ifndef __HTTP_AUTH_H__
#define __HTTP_AUTH_H__
// Copyright (c) 2009 - Decho Corp.

#include <boost/shared_ptr.hpp>

#include "client.h"

namespace HTTP
{
    class ClientAuthenticationScheme
    {
    public:
        typedef boost::shared_ptr<ClientAuthenticationScheme> ptr;

        virtual ~ClientAuthenticationScheme() {}

        virtual bool authorize(ClientRequest::ptr challenge, Request &nextRequest, bool proxy = false) = 0;
    };
};

#endif
