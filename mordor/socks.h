#ifndef __MORDOR_SOCKS_H__
#define __MORDOR_SOCKS_H__
// Copyright (c) 2010 - Mozy, Inc.

#include <boost/shared_ptr.hpp>

#include "mordor/exception.h"

namespace Mordor {

struct IPAddress;
class Stream;
struct URI;

namespace HTTP {

class StreamBroker;

}

namespace SOCKS {

struct Exception : virtual Mordor::Exception {};
struct ProtocolViolationException : virtual Exception {};
struct NoAcceptableAuthenticationMethodException : virtual Exception {};
struct InvalidResponseException : virtual Exception
{
    InvalidResponseException(unsigned char status)
        : m_status(status)
    {}

    unsigned char status() const { return m_status; }
private:
    unsigned char m_status;
};

boost::shared_ptr<Stream> tunnel(boost::shared_ptr<HTTP::StreamBroker> streamBroker,
    const URI &proxy, boost::shared_ptr<IPAddress> targetIP,
    const std::string &targetDomain = std::string(),
    unsigned short targetPort = 0,
    unsigned char version = 5);

}}

#endif
