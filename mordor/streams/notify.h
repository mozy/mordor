#ifndef __MORDOR_NOTIFY_STREAM_H__
#define __MORDOR_NOTIFY_STREAM_H__
// Copyright (c) 2009 - Decho Corporation

#include <boost/function.hpp>

#include "filter.h"

namespace Mordor {

class NotifyStream : public FilterStream
{
public:
    typedef boost::shared_ptr<NotifyStream> ptr;
public:
    NotifyStream(Stream::ptr parent, bool own = true)
        : FilterStream(parent, own)
    {}

    boost::function<void ()> notifyOnClose;
    boost::function<void ()> notifyOnFlush;
    boost::function<void ()> notifyOnEof;
    boost::function<void ()> notifyOnException;

    void close(CloseType type = BOTH)
    {
        try {
            if (ownsParent())
                parent()->close(type);
        } catch (...) {
            if (notifyOnException)
                notifyOnException();
            throw;
        }
        if (notifyOnClose)
            notifyOnClose();
    }

    size_t read(Buffer &b, size_t len)
    {
        size_t result;
        try {
            result = parent()->read(b, len);
        } catch(...) {
            if (notifyOnException)
                notifyOnException();
            throw;
        }
        if (result == 0 && notifyOnEof)
            notifyOnEof();
        return result;
    }

    size_t write(const Buffer &b, size_t len)
    {
        try {
            return parent()->write(b, len);
        } catch(...) {
            if (notifyOnException)
                notifyOnException();
            throw;
        }
    }

    void flush(bool flushParent = true)
    {
        try {
            parent()->flush(flushParent);
        } catch(...) {
            if (notifyOnException)
                notifyOnException();
            throw;
        }
        if (notifyOnFlush)
            notifyOnFlush();
    }
};

}

#endif
