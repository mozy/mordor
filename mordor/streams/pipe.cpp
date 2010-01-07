// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "pipe.h"

#include <boost/thread/mutex.hpp>

#include "mordor/exception.h"
#include "mordor/fiber.h"

namespace Mordor {

class PipeStream : public Stream
{
    friend std::pair<Stream::ptr, Stream::ptr> pipeStream(size_t);
public:
    typedef boost::shared_ptr<PipeStream> ptr;
    typedef boost::weak_ptr<PipeStream> weak_ptr;

public:
    PipeStream(size_t bufferSize);
    ~PipeStream();

    bool supportsHalfClose() { return true; }
    bool supportsRead() { return true; }
    bool supportsWrite() { return true; }

    void close(CloseType type = BOTH);
    size_t read(Buffer &b, size_t len);
    size_t write(const Buffer &b, size_t len);
    void flush();

private:
    PipeStream::weak_ptr m_otherStream;
    boost::shared_ptr<boost::mutex> m_mutex;
    Buffer m_readBuffer;
    size_t m_bufferSize;
    CloseType m_closed, m_otherClosed;
    Scheduler *m_pendingWriterScheduler, *m_pendingReaderScheduler;
    Fiber::ptr m_pendingWriter, m_pendingReader;
};

std::pair<Stream::ptr, Stream::ptr> pipeStream(size_t bufferSize)
{
    if (bufferSize == ~0u)
        bufferSize = 65536;
    std::pair<PipeStream::ptr, PipeStream::ptr> result;
    result.first.reset(new PipeStream(bufferSize));
    result.second.reset(new PipeStream(bufferSize));
    result.first->m_otherStream = result.second;
    result.second->m_otherStream = result.first;
    result.first->m_mutex.reset(new boost::mutex());
    result.second->m_mutex = result.first->m_mutex;
    return result;
}

PipeStream::PipeStream(size_t bufferSize)
: m_bufferSize(bufferSize),
  m_closed(NONE),
  m_otherClosed(NONE),
  m_pendingWriterScheduler(NULL),
  m_pendingReaderScheduler(NULL)
{}

PipeStream::~PipeStream()
{
    boost::mutex::scoped_lock lock(*m_mutex);
    if (!m_otherStream.expired()) {
        PipeStream::ptr otherStream(m_otherStream);
        MORDOR_ASSERT(!otherStream->m_pendingReader);
        MORDOR_ASSERT(!otherStream->m_pendingReaderScheduler);
        MORDOR_ASSERT(!otherStream->m_pendingWriter);
        MORDOR_ASSERT(!otherStream->m_pendingWriterScheduler);
        if (!m_readBuffer.readAvailable())
            otherStream->m_otherClosed = (CloseType)(otherStream->m_otherClosed | READ);
        else
            otherStream->m_otherClosed = (CloseType)(otherStream->m_otherClosed & ~READ);
    }
    if (m_pendingReader) {
        MORDOR_ASSERT(m_pendingReaderScheduler);
        m_pendingReaderScheduler->schedule(m_pendingReader);
        m_pendingReader.reset();
        m_pendingReaderScheduler = NULL;
    }
    if (m_pendingWriter) {
        MORDOR_ASSERT(m_pendingWriterScheduler);
        m_pendingWriterScheduler->schedule(m_pendingWriter);
        m_pendingWriter.reset();
        m_pendingWriterScheduler = NULL;
    }
}

void
PipeStream::close(CloseType type)
{
    boost::mutex::scoped_lock lock(*m_mutex);
    m_closed = (CloseType)(m_closed | type);
    if (!m_otherStream.expired()) {
        PipeStream::ptr otherStream(m_otherStream);
        otherStream->m_otherClosed = m_closed;
    }
    if (m_pendingReader && (m_closed & WRITE)) {
        MORDOR_ASSERT(m_pendingReaderScheduler);
        m_pendingReaderScheduler->schedule(m_pendingReader);
        m_pendingReader.reset();
        m_pendingReaderScheduler = NULL;
    }
    if (m_pendingWriter && (m_closed & READ)) {
        MORDOR_ASSERT(m_pendingWriterScheduler);
        m_pendingWriterScheduler->schedule(m_pendingWriter);
        m_pendingWriter.reset();
        m_pendingWriterScheduler = NULL;
    }
}

size_t
PipeStream::read(Buffer &b, size_t len)
{
    MORDOR_ASSERT(len != 0);
    while (true) {
        {
            boost::mutex::scoped_lock lock(*m_mutex);
            if (m_closed & READ)
                MORDOR_THROW_EXCEPTION(BadHandleException());
            if (m_otherStream.expired() && !(m_otherClosed & WRITE))
                MORDOR_THROW_EXCEPTION(BrokenPipeException());
            size_t avail = m_readBuffer.readAvailable();
            if (avail > 0) {
                size_t todo = std::min(len, avail);
                b.copyIn(m_readBuffer, todo);
                m_readBuffer.consume(todo);
                if (m_pendingWriter) {
                    MORDOR_ASSERT(m_pendingWriterScheduler);
                    m_pendingWriterScheduler->schedule(m_pendingWriter);
                    m_pendingWriter.reset();
                    m_pendingWriterScheduler = NULL;
                }
                return todo;
            }

            if (m_otherClosed & WRITE)
                return 0;
            PipeStream::ptr otherStream(m_otherStream);

            // Wait for the other stream to schedule us
            MORDOR_ASSERT(!otherStream->m_pendingReader);
            MORDOR_ASSERT(!otherStream->m_pendingReaderScheduler);
            otherStream->m_pendingReader = Fiber::getThis();
            otherStream->m_pendingReaderScheduler = Scheduler::getThis();
        }
        try {
            Scheduler::getThis()->yieldTo();
        } catch (...) {
            boost::mutex::scoped_lock lock(*m_mutex);
            if (!m_otherStream.expired()) {
                PipeStream::ptr otherStream(m_otherStream);
                if (otherStream->m_pendingReader == Fiber::getThis()) {
                    MORDOR_ASSERT(otherStream->m_pendingReaderScheduler == Scheduler::getThis());
                    otherStream->m_pendingReader.reset();
                    otherStream->m_pendingReaderScheduler = NULL;
                }
            }
            throw;
        }
    }
}

size_t
PipeStream::write(const Buffer &b, size_t len)
{
    MORDOR_ASSERT(len != 0);
    while (true) {
        {
            boost::mutex::scoped_lock lock(*m_mutex);
            if (m_closed & WRITE)
                MORDOR_THROW_EXCEPTION(BadHandleException());
            if (m_otherStream.expired())
                MORDOR_THROW_EXCEPTION(BrokenPipeException());
            PipeStream::ptr otherStream(m_otherStream);
            if (otherStream->m_closed & READ)
                MORDOR_THROW_EXCEPTION(BrokenPipeException());

            size_t available = otherStream->m_readBuffer.readAvailable();
            size_t todo = std::min(m_bufferSize - available, len);
            if (todo != 0) {
                otherStream->m_readBuffer.copyIn(b, todo);
                if (m_pendingReader) {
                    MORDOR_ASSERT(m_pendingReaderScheduler);
                    m_pendingReaderScheduler->schedule(m_pendingReader);
                    m_pendingReader.reset();
                    m_pendingReaderScheduler = NULL;
                }            
                return todo;
            }
            // Wait for the other stream to schedule us
            MORDOR_ASSERT(!otherStream->m_pendingWriter);
            MORDOR_ASSERT(!otherStream->m_pendingWriterScheduler);
            otherStream->m_pendingWriter = Fiber::getThis();
            otherStream->m_pendingWriterScheduler = Scheduler::getThis();
        }
        try {
            Scheduler::getThis()->yieldTo();
        } catch (...) {
            boost::mutex::scoped_lock lock(*m_mutex);
            if (!m_otherStream.expired()) {
                PipeStream::ptr otherStream(m_otherStream);
                if (otherStream->m_pendingWriter == Fiber::getThis()) {
                    MORDOR_ASSERT(otherStream->m_pendingWriterScheduler == Scheduler::getThis());
                    otherStream->m_pendingWriter.reset();
                    otherStream->m_pendingWriterScheduler = NULL;
                }
            }
            throw;
        }
    }
}

void
PipeStream::flush()
{
    while (true) {
        {
            boost::mutex::scoped_lock lock(*m_mutex);
            if (m_otherStream.expired()) {
                // See if they read everything before destructing
                if (m_otherClosed & READ)
                    return;
                MORDOR_THROW_EXCEPTION(BrokenPipeException());
            }
            PipeStream::ptr otherStream(m_otherStream);
            if (otherStream->m_closed & READ)
                MORDOR_THROW_EXCEPTION(BrokenPipeException());

            if (otherStream->m_readBuffer.readAvailable() == 0)
                return;
            // Wait for the other stream to schedule us
            MORDOR_ASSERT(!otherStream->m_pendingWriter);
            MORDOR_ASSERT(!otherStream->m_pendingWriterScheduler);
            otherStream->m_pendingWriter = Fiber::getThis();
            otherStream->m_pendingWriterScheduler = Scheduler::getThis();
        }
        try {
            Scheduler::getThis()->yieldTo();
        } catch (...) {
            boost::mutex::scoped_lock lock(*m_mutex);
            if (!m_otherStream.expired()) {
                PipeStream::ptr otherStream(m_otherStream);
                if (otherStream->m_pendingWriter == Fiber::getThis()) {
                    MORDOR_ASSERT(otherStream->m_pendingWriterScheduler == Scheduler::getThis());
                    otherStream->m_pendingWriter.reset();
                    otherStream->m_pendingWriterScheduler = NULL;
                }
            }
            throw;
        }
    }
}

}
