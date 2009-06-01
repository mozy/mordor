#ifndef __NOTIFY_STREAM_H__
#define __NOTIFY_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include <boost/function.hpp>

#include "filter.h"

class NotifyStream : public FilterStream
{
public:
    NotifyStream(Stream *parent, bool own = true)
        : FilterStream(parent, own)
    {}

    boost::function<void ()> notifyOnClose;
    boost::function<void ()> notifyOnEof;

    void close(CloseType type = BOTH)
    {
        FilterStream::close(type);
        if (notifyOnClose) {
            notifyOnClose();
        }
    }

    size_t read(Buffer *b, size_t len)
    {
        size_t result = FilterStream::read(b, len);
        if (result == 0 && notifyOnEof) {
            notifyOnEof();
        }
        return result;
    }
};

#endif
