#ifndef __HTTP_CONNECTION_H__
#define __HTTP_CONNECTION_H__
// Copyright (c) 2009 - Decho Corp.

#include <boost/function.hpp>

#include "common/streams/stream.h"
#include "http.h"

namespace HTTP
{
    class Connection
    {
    protected:
        Connection(Stream::ptr stream);

        static bool hasMessageBody(const GeneralHeaders &general,
            const EntityHeaders &entity,
            Method method,
            Status status);

        Stream::ptr getStream(const GeneralHeaders &general,
            const EntityHeaders &entity,
            Method method,
            Status status,
            boost::function<void ()> notifyOnEof,
            boost::function<void ()> notifyOnException,
            bool forRead);

    protected:
        Stream::ptr m_stream;
    };
};

#endif
