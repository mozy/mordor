// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "transfer.h"

#include <boost/bind.hpp>

#include "mordor/exception.h"
#include "mordor/scheduler.h"
#include "mordor/streams/null.h"
#include "stream.h"

namespace Mordor {

static void readOne(Stream &src, Buffer *&buffer, size_t len, size_t &result)
{
    result = src.read(*buffer, len);
}

static void writeOne(Stream &dst, Buffer *&buffer)
{
    size_t result;
    while (buffer->readAvailable() > 0) {
        result = dst.write(*buffer, buffer->readAvailable());
        buffer->consume(result);
    }
}

unsigned long long transferStream(Stream &src, Stream &dst,
                                  unsigned long long toTransfer)
{
    MORDOR_ASSERT(src.supportsRead());
    MORDOR_ASSERT(dst.supportsWrite());
    Buffer buf1, buf2;
    Buffer *readBuffer, *writeBuffer;
    size_t chunkSize = 65536;
    size_t todo;
    size_t readResult;
    unsigned long long totalRead = 0;
    if (toTransfer == 0)
        return 0;

    readBuffer = &buf1;
    todo = chunkSize;
    if (toTransfer - totalRead < (unsigned long long)todo)
        todo = (size_t)(toTransfer - totalRead);
    readOne(src, readBuffer, todo, readResult);
    totalRead += readResult;
    if (readResult == 0 && toTransfer != ~0ull)
        MORDOR_THROW_EXCEPTION(UnexpectedEofException());
    if (readResult == 0)
        return totalRead;

    // Optimize transfer to NullStream
    if (&dst == &NullStream::get()) {
        while (true) {
            readBuffer->clear();
            todo = chunkSize;
            if (toTransfer - totalRead < (unsigned long long)todo)
                todo = (size_t)(toTransfer - totalRead);
            if (todo == 0)
                return totalRead;
            readOne(src, readBuffer, todo, readResult);
            totalRead += readResult;
            if (readResult == 0 && toTransfer != ~0ull)
                MORDOR_THROW_EXCEPTION(UnexpectedEofException());
            if (readResult == 0)
                return totalRead;
        }
    }

    std::vector<boost::function<void ()> > dgs;
    std::vector<Fiber::ptr> fibers;
    dgs.resize(2);
    fibers.resize(2);
    fibers[0].reset(new Fiber(NULL));
    fibers[1].reset(new Fiber(NULL));
    dgs[0] = boost::bind(&readOne, boost::ref(src), boost::ref(readBuffer),
        boost::ref(todo), boost::ref(readResult));
    dgs[1] = boost::bind(&writeOne, boost::ref(dst), boost::ref(writeBuffer));
    while (totalRead < toTransfer) {
        writeBuffer = readBuffer;
        if (readBuffer == &buf1)
            readBuffer = &buf2;
        else
            readBuffer = &buf1;
        todo = chunkSize;
        if (toTransfer - totalRead < (unsigned long long)todo)
            todo = (size_t)(toTransfer - totalRead);
        parallel_do(dgs, fibers);
        totalRead += readResult;
        if (readResult == 0 && toTransfer != ~0ull && totalRead < toTransfer)
            MORDOR_THROW_EXCEPTION(UnexpectedEofException());
        if (readResult == 0)
            return totalRead;
    }
    writeBuffer = readBuffer;
    writeOne(dst, writeBuffer);
    return totalRead;
}

}
