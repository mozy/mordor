#ifndef __HTTP_BASIC_AUTH_H__
#define __HTTP_BASIC_AUTH_H__
// Copyright (c) 2009 - Decho Corp.

#include <boost/function.hpp>

#include "auth.h"
#include "common/uri.h"

namespace HTTP
{
    class BasicClientAuthenticationScheme : public ClientAuthenticationScheme
    {
    public:
        BasicClientAuthenticationScheme(
            boost::function<bool (const URI & /* absolute_URI */, const std::string & /* realm */, bool /* proxy */, std::string &/* username */, std::string &/* password */)> authDg = NULL,
            boost::function<bool (const URI & /* absolute_URI */, bool /* proxy */, std::string &/* username */, std::string &/* password */)> preauthDg = NULL)
            : m_authDg(authDg),
              m_preauthDg(preauthDg)
        {}

        bool authorize(ClientRequest::ptr challenge, Request &nextRequest, bool proxy);

    private:
        boost::function<bool (const URI &, const std::string &, bool, std::string &, std::string &)> m_authDg;
        boost::function<bool (const URI &, bool, std::string &, std::string &)> m_preauthDg;
    };
};

#endif
