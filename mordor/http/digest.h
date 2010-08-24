#ifndef __MORDOR_HTTP_DIGEST_AUTH_H__
#define __MORDOR_HTTP_DIGEST_AUTH_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "http.h"

namespace Mordor {
namespace HTTP {
namespace DigestAuth {

struct InvalidParamsException : virtual InvalidMessageHeaderException {};
struct InvalidQopException : virtual InvalidParamsException
{
public:
    InvalidQopException(const std::string &message) : m_message(message) {}
    ~InvalidQopException() throw() {}

    const char *what() const throw() { return m_message.c_str(); }
private:
    std::string m_message;
};
struct InvalidAlgorithmException : virtual InvalidParamsException
{
public:
    InvalidAlgorithmException(const std::string &message) : m_message(message) {}
    ~InvalidAlgorithmException() throw() {}

    const char *what() const throw() { return m_message.c_str(); }
private:
    std::string m_message;
};

void authorize(const AuthParams &challenge, AuthParams &authorization,
    const URI &uri, const std::string &method, const std::string &username,
    const std::string &password);

/// @deprecated
void authorize(const Response &challenge, Request &nextRequest,
    const std::string &username, const std::string &password);

}}}

#endif
