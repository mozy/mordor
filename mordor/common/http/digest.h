#ifndef __HTTP_DIGEST_AUTH_H__
#define __HTTP_DIGEST_AUTH_H__
// Copyright (c) 2009 - Decho Corp.

#include "http.h"

namespace HTTP
{
    struct InvalidDigestParamsException : virtual InvalidMessageHeaderException {};
    struct InvalidDigestQopException : virtual InvalidDigestParamsException
    {
    public:
        InvalidDigestQopException(const std::string &message) : m_message(message) {}
        ~InvalidDigestQopException() throw() {}
        
        const char *what() const throw() { return m_message.c_str(); }
    private:
        std::string m_message;
    };
    struct InvalidDigestAlgorithmException : virtual InvalidDigestParamsException
    {
    public:
        InvalidDigestAlgorithmException(const std::string &message) : m_message(message) {}
        ~InvalidDigestAlgorithmException() throw() {}

        const char *what() const throw() { return m_message.c_str(); }
    private:
        std::string m_message;
    };

    class DigestAuth
    {
    public:
        static void authorize(const Response &challenge,
            Request &nextRequest, const std::string &username,
            const std::string &password);
    };
};

#endif
