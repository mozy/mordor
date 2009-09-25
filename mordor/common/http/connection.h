#ifndef __HTTP_CONNECTION_H__
#define __HTTP_CONNECTION_H__
// Copyright (c) 2009 - Decho Corp.

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

#include "http.h"

class Stream;

namespace HTTP
{
    class Connection
    {
    public:
        boost::shared_ptr<Stream> stream() { return m_stream; }

    protected:
        Connection(boost::shared_ptr<Stream> stream);

        static bool hasMessageBody(const GeneralHeaders &general,
            const EntityHeaders &entity,
            Method method,
            Status status);

        boost::shared_ptr<Stream> getStream(const GeneralHeaders &general,
            const EntityHeaders &entity,
            Method method,
            Status status,
            boost::function<void ()> notifyOnEof,
            boost::function<void ()> notifyOnException,
            bool forRead);

    protected:
        boost::shared_ptr<Stream> m_stream;
    };
};

#endif
