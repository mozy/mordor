#ifndef __HTTP_NEGOTIATE_AUTH_H__
#define __HTTP_NEGOTIATE_AUTH_H__
// Copyright (c) 2009 - Decho Corp.

#include <security.h>

#include <boost/noncopyable.hpp>

#include "http.h"

namespace HTTP
{
    class NegotiateAuth : public boost::noncopyable
    {
    public:
        NegotiateAuth(const std::string &username, const std::string &password);
        ~NegotiateAuth();

        bool authorize(const Response &challenge, Request &nextRequest);

    private:
        std::wstring m_username, m_password, m_domain;
        CredHandle m_creds;
        SecHandle m_secCtx;
    };
};

#endif
