#ifndef __MORDOR_PROGRESS_STREAM_H__
#define __MORDOR_PROGRESS_STREAM_H__
// Copyright (c) 2010 - Mozy, Inc.

#include <boost/function.hpp>

#include "filter.h"

namespace Mordor {

class ProgressStream : public FilterStream
{
public:
    typedef boost::shared_ptr<ProgressStream> ptr;
    typedef boost::function<void (size_t)> progress_callback;

private:
    progress_callback m_readDg, m_writeDg;
    size_t m_pendingRead, m_pendingWrite, m_threshold;

    void reportRead()
    {
        if (m_readDg && m_pendingRead > 0) {
            m_readDg(m_pendingRead);
            m_pendingRead = 0;
        }
    }

    void reportWrite()
    {
        if (m_writeDg && m_pendingWrite > 0) {
            m_writeDg(m_pendingWrite);
            m_pendingWrite = 0;
        }
    }

public:
    ProgressStream(Stream::ptr parent,
                   progress_callback readDg,
                   progress_callback writeDg,
                   size_t threshold = 0,
                   bool own = true)
        : FilterStream(parent, own),
          m_readDg(readDg),
          m_writeDg(writeDg),
          m_pendingRead(0),
          m_pendingWrite(0),
          m_threshold(threshold)
    {}


    void close(CloseType type = BOTH)
    {
        // report any unreported data before close
        reportRead();
        reportWrite();

        if (ownsParent())
            parent()->close(type);
    }

    using FilterStream::read;
    size_t read(Buffer &b, size_t len)
    {
        size_t result = parent()->read(b, len);

        m_pendingRead += result;
        if (m_pendingRead >= m_threshold)
            reportRead();

        return result;
    }

    using FilterStream::write;
    size_t write(const Buffer &b, size_t len)
    {
        size_t result = parent()->write(b, len);

        m_pendingWrite += result;
        if (m_pendingWrite >= m_threshold)
            reportWrite();

        return result;
    }

    void flush(bool flushParent = true)
    {
        // report any unreported data before flush
        reportRead();
        reportWrite();

        parent()->flush(flushParent);
    }
};

}

#endif
