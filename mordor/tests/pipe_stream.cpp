// Copyright (c) 2009 - Mozy, Inc.

#include <boost/bind.hpp>

#include "mordor/exception.h"
#include "mordor/fiber.h"
#include "mordor/scheduler.h"
#include "mordor/streams/buffer.h"
#include "mordor/streams/stream.h"
#include "mordor/streams/pipe.h"
#include "mordor/test/test.h"
#include "mordor/workerpool.h"

using namespace Mordor;
using namespace Mordor::Test;

MORDOR_UNITTEST(PipeStream, basic)
{
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream();

    MORDOR_TEST_ASSERT(pipe.first->supportsRead());
    MORDOR_TEST_ASSERT(pipe.first->supportsWrite());
    MORDOR_TEST_ASSERT(!pipe.first->supportsSeek());
    MORDOR_TEST_ASSERT(!pipe.first->supportsSize());
    MORDOR_TEST_ASSERT(!pipe.first->supportsTruncate());
    MORDOR_TEST_ASSERT(!pipe.first->supportsFind());
    MORDOR_TEST_ASSERT(!pipe.first->supportsUnread());
    MORDOR_TEST_ASSERT(pipe.second->supportsRead());
    MORDOR_TEST_ASSERT(pipe.second->supportsWrite());
    MORDOR_TEST_ASSERT(!pipe.second->supportsSeek());
    MORDOR_TEST_ASSERT(!pipe.second->supportsSize());
    MORDOR_TEST_ASSERT(!pipe.second->supportsTruncate());
    MORDOR_TEST_ASSERT(!pipe.second->supportsFind());
    MORDOR_TEST_ASSERT(!pipe.second->supportsUnread());

    Buffer read;
    MORDOR_TEST_ASSERT_EQUAL(pipe.first->write("a"), 1u);
    MORDOR_TEST_ASSERT_EQUAL(pipe.second->read(read, 10), 1u);
    MORDOR_TEST_ASSERT(read == "a");
    pipe.first->close();
    MORDOR_TEST_ASSERT_EQUAL(pipe.second->read(read, 10), 0u);
}

static void basicInFibers(Stream::ptr stream, int &sequence)
{
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
    MORDOR_TEST_ASSERT_EQUAL(stream->write("a"), 1u);
    stream->close();
    stream->flush();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 3);
}

MORDOR_UNITTEST(PipeStream, basicInFibers)
{
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream();
    // pool must destruct before pipe, because when pool destructs it
    // waits for the other fiber to complete, which has a weak ref
    // to pipe.second; if pipe.second is gone, it will throw an
    // exception that we don't want
    WorkerPool pool;
    int sequence = 1;

    pool.schedule(Fiber::ptr(new Fiber(boost::bind(&basicInFibers, pipe.first, boost::ref(sequence)))));

    Buffer read;
    MORDOR_TEST_ASSERT_EQUAL(pipe.second->read(read, 10), 1u);
    MORDOR_TEST_ASSERT(read == "a");
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 2);
    MORDOR_TEST_ASSERT_EQUAL(pipe.second->read(read, 10), 0u);
}

MORDOR_UNITTEST(PipeStream, readerClosed1)
{
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream();

    pipe.second->close();
    MORDOR_TEST_ASSERT_EXCEPTION(pipe.first->write("a"), BrokenPipeException);
    pipe.first->flush();
}

MORDOR_UNITTEST(PipeStream, readerClosed2)
{
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream();

    MORDOR_TEST_ASSERT_EQUAL(pipe.first->write("a"), 1u);
    pipe.second->close();
    MORDOR_TEST_ASSERT_EXCEPTION(pipe.first->flush(), BrokenPipeException);
}

MORDOR_UNITTEST(PipeStream, readerGone)
{
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream();

    pipe.second.reset();
    MORDOR_TEST_ASSERT_EXCEPTION(pipe.first->write("a"), BrokenPipeException);
}

MORDOR_UNITTEST(PipeStream, readerGoneFlush)
{
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream();

    MORDOR_TEST_ASSERT_EQUAL(pipe.first->write("a"), 1u);
    pipe.second.reset();
    MORDOR_TEST_ASSERT_EXCEPTION(pipe.first->flush(), BrokenPipeException);
}

MORDOR_UNITTEST(PipeStream, readerGoneReadEverything)
{
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream();

    pipe.second.reset();
    pipe.first->flush();
}

MORDOR_UNITTEST(PipeStream, writerGone)
{
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream();

    pipe.first.reset();
    Buffer read;
    MORDOR_TEST_ASSERT_EXCEPTION(pipe.second->read(read, 10), BrokenPipeException);
}

static void blockingRead(Stream::ptr stream, int &sequence)
{
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 2);
    MORDOR_TEST_ASSERT_EQUAL(stream->write("hello"), 5u);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 3);
}

MORDOR_UNITTEST(PipeStream, blockingRead)
{
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream(5);
    WorkerPool pool;
    int sequence = 1;

    pool.schedule(Fiber::ptr(new Fiber(boost::bind(&blockingRead, pipe.second,
        boost::ref(sequence)))));

    Buffer output;
    MORDOR_TEST_ASSERT_EQUAL(pipe.first->read(output, 10), 5u);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 4);
    MORDOR_TEST_ASSERT(output == "hello");
}

static void blockingWrite(Stream::ptr stream, int &sequence)
{
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 3);
    Buffer output;
    MORDOR_TEST_ASSERT_EQUAL(stream->read(output, 10), 5u);
    MORDOR_TEST_ASSERT(output == "hello");
}

MORDOR_UNITTEST(PipeStream, blockingWrite)
{
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream(5);
    WorkerPool pool;
    int sequence = 1;

    pool.schedule(Fiber::ptr(new Fiber(boost::bind(&blockingWrite, pipe.second,
        boost::ref(sequence)))));

    MORDOR_TEST_ASSERT_EQUAL(pipe.first->write("hello"), 5u);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 2);
    MORDOR_TEST_ASSERT_EQUAL(pipe.first->write("world"), 5u);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 4);
    Buffer output;
    MORDOR_TEST_ASSERT_EQUAL(pipe.second->read(output, 10), 5u);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 5);
    MORDOR_TEST_ASSERT(output == "world");
}

MORDOR_UNITTEST(PipeStream, oversizedWrite)
{
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream(5);

    MORDOR_TEST_ASSERT_EQUAL(pipe.first->write("helloworld"), 5u);
    Buffer output;
    MORDOR_TEST_ASSERT_EQUAL(pipe.second->read(output, 10), 5u);
    MORDOR_TEST_ASSERT(output == "hello");
}

static void closeOnBlockingReader(Stream::ptr stream, int &sequence)
{
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 2);
    stream->close();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 3);
}

MORDOR_UNITTEST(PipeStream, closeOnBlockingReader)
{
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream();
    WorkerPool pool;
    int sequence = 1;

    pool.schedule(Fiber::ptr(new Fiber(boost::bind(&closeOnBlockingReader,
        pipe.first, boost::ref(sequence)))));

    Buffer output;
    MORDOR_TEST_ASSERT_EQUAL(pipe.second->read(output, 10), 0u);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 4);
}

static void closeOnBlockingWriter(Stream::ptr stream, int &sequence)
{
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 3);
    stream->close();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 4);
}

MORDOR_UNITTEST(PipeStream, closeOnBlockingWriter)
{
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream(5);
    WorkerPool pool;
    int sequence = 1;

    pool.schedule(Fiber::ptr(new Fiber(boost::bind(&closeOnBlockingWriter, pipe.first,
        boost::ref(sequence)))));

    MORDOR_TEST_ASSERT_EQUAL(pipe.second->write("hello"), 5u);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 2);
    MORDOR_TEST_ASSERT_EXCEPTION(pipe.second->write("world"), BrokenPipeException);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 5);
}

static void destructOnBlockingReader(boost::weak_ptr<Stream> weakStream, int &sequence)
{
    Stream::ptr stream(weakStream);
    Fiber::yield();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 2);
    MORDOR_TEST_ASSERT(stream.unique());
    stream.reset();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 3);
}

MORDOR_UNITTEST(PipeStream, destructOnBlockingReader)
{
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream();
    WorkerPool pool;
    int sequence = 1;

    Fiber::ptr f = Fiber::ptr(new Fiber(boost::bind(&destructOnBlockingReader,
        boost::weak_ptr<Stream>(pipe.first), boost::ref(sequence))));
    f->call();
    pipe.first.reset();
    pool.schedule(f);

    Buffer output;
    MORDOR_TEST_ASSERT_EXCEPTION(pipe.second->read(output, 10), BrokenPipeException);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 4);
}

static void destructOnBlockingWriter(boost::weak_ptr<Stream> weakStream, int &sequence)
{
    Stream::ptr stream(weakStream);
    Fiber::yield();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 3);
    MORDOR_TEST_ASSERT(stream.unique());
    stream.reset();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 4);
}

MORDOR_UNITTEST(PipeStream, destructOnBlockingWriter)
{
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream(5);
    WorkerPool pool;
    int sequence = 1;

    Fiber::ptr f = Fiber::ptr(new Fiber(boost::bind(&destructOnBlockingWriter,
        boost::weak_ptr<Stream>(pipe.first), boost::ref(sequence))));
    f->call();
    pipe.first.reset();
    pool.schedule(f);

    MORDOR_TEST_ASSERT_EQUAL(pipe.second->write("hello"), 5u);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 2);
    MORDOR_TEST_ASSERT_EXCEPTION(pipe.second->write("world"), BrokenPipeException);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 5);
}

static void cancelOnBlockingReader(Stream::ptr stream, int &sequence)
{
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 2);
    stream->cancelRead();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 3);
}

MORDOR_UNITTEST(PipeStream, cancelOnBlockingReader)
{
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream();
    WorkerPool pool;
    int sequence = 1;

    pool.schedule(Fiber::ptr(new Fiber(boost::bind(&cancelOnBlockingReader,
        pipe.first, boost::ref(sequence)))));

    Buffer output;
    MORDOR_TEST_ASSERT_EXCEPTION(pipe.first->read(output, 10), OperationAbortedException);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 4);
    MORDOR_TEST_ASSERT_EXCEPTION(pipe.first->read(output, 10), OperationAbortedException);
}

static void cancelOnBlockingWriter(Stream::ptr stream, int &sequence)
{
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 2);
    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    MORDOR_TEST_ASSERT_EXCEPTION(
        while (true) stream->write(buffer, 4096),
        OperationAbortedException);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 4);
    MORDOR_TEST_ASSERT_EXCEPTION(stream->write(buffer, 4096), OperationAbortedException);
}

MORDOR_UNITTEST(PipeStream, cancelOnBlockingWriter)
{
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream(5);
    WorkerPool pool;
    int sequence = 1;

    pool.schedule(Fiber::ptr(new Fiber(boost::bind(&cancelOnBlockingWriter, pipe.first,
        boost::ref(sequence)))));
    Scheduler::yield();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 3);
    pipe.first->cancelWrite();
    pool.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 5);
}

static void threadStress(Stream::ptr stream)
{
    size_t totalRead = 0;
    size_t totalWritten = 0;
    size_t buf[64];
    Buffer buffer;
    for (int i = 0; i < 10000; ++i) {
        if (i % 2) {
            size_t toRead = 64;
            size_t read = stream->read(buffer, toRead * sizeof(size_t));
            MORDOR_TEST_ASSERT(read % sizeof(size_t) == 0);
            buffer.copyOut(&buf, read);
            for (size_t j = 0; read > 0; read -= sizeof(size_t), ++j) {
                MORDOR_TEST_ASSERT_EQUAL(buf[j], ++totalRead);
            }
            buffer.clear();
        } else {
            size_t toWrite = 64;
            for (size_t j = 0; j < toWrite; ++j) {
                buf[j] = ++totalWritten;
            }
            buffer.copyIn(buf, toWrite * sizeof(size_t));
            size_t written = stream->write(buffer, toWrite * sizeof(size_t));
            totalWritten -= (toWrite - written / sizeof(size_t));
            buffer.clear();
        }
    }
    stream->close(Stream::WRITE);
    while (true) {
        size_t toRead = 64;
        size_t read = stream->read(buffer, toRead);
        if (read == 0)
            break;
        MORDOR_TEST_ASSERT(read % sizeof(size_t) == 0);
        buffer.copyOut(&buf, read);
        for (size_t i = 0; read > 0; read -= sizeof(size_t), ++i) {
            MORDOR_TEST_ASSERT_EQUAL(buf[i], ++totalRead);
        }
        buffer.clear();
    }
    stream->flush();
}

MORDOR_UNITTEST(PipeStream, threadStress)
{
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream();
    WorkerPool pool(2);

    pool.schedule(Fiber::ptr(new Fiber(boost::bind(&threadStress, pipe.first))));
    threadStress(pipe.second);
}

static void closed(bool &remoteClosed)
{
    remoteClosed = true;
}

MORDOR_UNITTEST(PipeStream, eventOnRemoteClose)
{
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream();

    bool remoteClosed = false;
    pipe.first->onRemoteClose(boost::bind(&closed, boost::ref(remoteClosed)));
    pipe.second->close();
    MORDOR_TEST_ASSERT(remoteClosed);
}

MORDOR_UNITTEST(PipeStream, eventOnRemoteReset)
{
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream();

    bool remoteClosed = false;
    pipe.first->onRemoteClose(boost::bind(&closed, boost::ref(remoteClosed)));
    pipe.second.reset();
    MORDOR_TEST_ASSERT(remoteClosed);
}
