#ifndef __HTTP_DIGEST_AUTH_H__
#define __HTTP_DIGEST_AUTH_H__
// Copyright (c) 2009 - Decho Corp.

#include "http.h"

namespace HTTP
{
    class InvalidDigestParamsException : public InvalidMessageHeaderException
    {
    public:
        InvalidDigestParamsException(const std::string &message)
            : InvalidMessageHeaderException(message)
        {}
    };
    class InvalidDigestQopException : public InvalidDigestParamsException
    {
    public:
        InvalidDigestQopException(const std::string &message)
            : InvalidDigestParamsException(message)
        {}
    };
    class InvalidDigestAlgorithmException : public InvalidDigestParamsException
    {
    public:
        InvalidDigestAlgorithmException(const std::string &message)
            : InvalidDigestParamsException(message)
        {}
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
