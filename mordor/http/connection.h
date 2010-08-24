#ifndef __MORDOR_HTTP_CONNECTION_H__
#define __MORDOR_HTTP_CONNECTION_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

#include "http.h"

namespace Mordor {

class Stream;

namespace HTTP {

class Connection
{
public:
    boost::shared_ptr<Stream> stream() { return m_stream; }

    static bool hasMessageBody(const GeneralHeaders &general,
        const EntityHeaders &entity,
        const std::string &method,
        Status status,
        bool includeEmpty = true);

protected:
    Connection(boost::shared_ptr<Stream> stream);

    boost::shared_ptr<Stream> getStream(const GeneralHeaders &general,
        const EntityHeaders &entity,
        const std::string &method,
        Status status,
        boost::function<void ()> notifyOnEof,
        boost::function<void ()> notifyOnException,
        bool forRead);

protected:
    boost::shared_ptr<Stream> m_stream;
};

}}

#endif
