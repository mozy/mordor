#ifndef __HTTP_CONNECTION_H__
#define __HTTP_CONNECTION_H__
// Copyright (c) 2009 - Decho Corp.

#include <boost/function.hpp>

#include "http.h"

class Stream;

namespace HTTP
{
    class Connection
    {
    protected:
        Connection(Stream *stream, bool own = true);
        virtual ~Connection();

        static bool hasMessageBody(const GeneralHeaders &general,
            const EntityHeaders &entity,
            Method method,
            Status status);

        Stream *getStream(const GeneralHeaders &general,
            const EntityHeaders &entity,
            Method method,
            Status status,
            boost::function<void ()> notifyOnEof,
            boost::function<void ()> notifyOnException,
            bool forRead);

    protected:
        Stream *m_stream;
        bool m_own;
    };
};

#endif
