// Copyright (c) 2009 - Decho Corp.

#include <boost/bind.hpp>

#include "mordor/common/exception.h"
#include "mordor/common/fiber.h"
#include "mordor/common/streams/pipe.h"
#include "mordor/test/test.h"

TEST_WITH_SUITE(PipeStream, basic)
{
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream();

    Buffer read;
    TEST_ASSERT_EQUAL(pipe.first->write("a"), 1u);
    TEST_ASSERT_EQUAL(pipe.second->read(read, 10), 1u);
    TEST_ASSERT(read == "a");
    pipe.first->close();
    TEST_ASSERT_EQUAL(pipe.second->read(read, 10), 0u);
}

void basicInFibers(Stream::ptr stream, int &sequence)
{
    TEST_ASSERT_EQUAL(sequence, 1);
    TEST_ASSERT_EQUAL(stream->write("a"), 1u);
    stream->close();
    stream->flush();
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 3);
}

TEST_WITH_SUITE(PipeStream, basicInFibers)
{
    Fiber::ptr mainFiber(new Fiber());
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream();
    // pool must destruct before pipe, because when pool destructs it
    // waits for the other fiber to complete, which has a weak ref
    // to pipe.second; if pipe.second is gone, it will throw an
    // exception that we don't want
    WorkerPool pool;
    int sequence = 1;

    pool.schedule(Fiber::ptr(new Fiber(boost::bind(&basicInFibers, pipe.first, boost::ref(sequence)))));

    Buffer read;
    TEST_ASSERT_EQUAL(pipe.second->read(read, 10), 1u);
    TEST_ASSERT(read == "a");
    ++sequence;
    TEST_ASSERT_EQUAL(sequence, 2);
    TEST_ASSERT_EQUAL(pipe.second->read(read, 10), 0u);
}

TEST_WITH_SUITE(PipeStream, readerClosed)
{
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream();

    pipe.second->close();
    TEST_ASSERT_EXCEPTION(pipe.first->write("a"), ConnectionAbortedException);
}

TEST_WITH_SUITE(PipeStream, readerGone)
{
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream();

    pipe.second.reset();
    TEST_ASSERT_EXCEPTION(pipe.first->write("a"), ConnectionResetException);
}

TEST_WITH_SUITE(PipeStream, writerGone)
{
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream();

    pipe.first.reset();
    Buffer read;
    TEST_ASSERT_EXCEPTION(pipe.second->read(read, 10), ConnectionResetException);
}

void threadStress(Stream::ptr stream)
{
    int totalRead = 0;
    int totalWritten = 0;
    int buf[64];
    Buffer buffer;
    for (int i = 0; i < 10000; ++i) {
        if (i % 2) {
            size_t toRead = 64;
            size_t read = stream->read(buffer, toRead * sizeof(int));
            TEST_ASSERT(read % sizeof(int) == 0);
            buffer.copyOut(&buf, read);
            for (size_t j = 0; read > 0; read -= sizeof(int), ++j) {
                TEST_ASSERT_EQUAL(buf[j], ++totalRead);
            }
            buffer.clear();
        } else {
            size_t toWrite = 64;
            for (size_t j = 0; j < toWrite; ++j) {
                buf[j] = ++totalWritten;
            }
            buffer.copyIn(buf, toWrite * sizeof(int));
            size_t written = stream->write(buffer, toWrite * sizeof(int));
            totalWritten -= (toWrite - written / sizeof(int));
            buffer.clear();
        }
    }
    stream->close(Stream::WRITE);
    while (true) {
        size_t toRead = 64;
        size_t read = stream->read(buffer, toRead);
        if (read == 0)
            break;
        TEST_ASSERT(read % sizeof(int) == 0);
        buffer.copyOut(&buf, read);
        for (size_t i = 0; read > 0; read -= sizeof(int), ++i) {
            TEST_ASSERT_EQUAL(buf[i], ++totalRead);
        }
        buffer.clear();
    }
    stream->flush();
}

TEST_WITH_SUITE(PipeStream, threadStress)
{
    Fiber::ptr mainFiber(new Fiber());
    std::pair<Stream::ptr, Stream::ptr> pipe = pipeStream();
    WorkerPool pool(2);
    
    pool.schedule(Fiber::ptr(new Fiber(boost::bind(&threadStress, pipe.first))));
    threadStress(pipe.second);
}
