// Copyright (c) 2009 - Decho Corp.

#include "transfer.h"

#include <boost/bind.hpp>

#include "scheduler.h"
#include "stream.h"

static void read(Stream *src, Buffer *buffer, size_t len, size_t *result)
{
    *result = src->read(buffer, len);
}

static void write(Stream *dst, Buffer *buffer, long long *totalWritten)
{
    size_t result;
    while (buffer->readAvailable() > 0) {
        result = dst->write(buffer, buffer->readAvailable());
        buffer->consume(result);
        if (totalWritten)
            *totalWritten = result;
    }
}

void transferStream(Stream *src, Stream *dst, long long toTransfer,
                    long long *totalRead, long long *totalWritten)
{
    assert(src);
    assert(src->supportsRead());
    assert(dst);
    assert(dst->supportsWrite());
    assert(toTransfer >= 0 || toTransfer == -1);
    Buffer buf1, buf2;
    Buffer *readBuffer, *writeBuffer;
    size_t chunkSize = 65536;
    size_t todo;
    size_t readResult;
    long long totalReadS;
    if (!totalRead)
        totalRead = &totalReadS;
    *totalRead = 0;
    if (totalWritten)
        *totalWritten = 0;

    readBuffer = &buf1;
    todo = chunkSize;
    if (toTransfer != -1 && toTransfer - *totalRead < (long long)todo)
        todo = (size_t)toTransfer;
    read(src, readBuffer, todo, &readResult);
    *totalRead += readResult;
    if (readResult == 0 && toTransfer != -1) {
        // throw UnexpectedEofException();
    }
    if (readResult == 0)
        return;

    std::vector<boost::function<void ()> > dgs;
    while (*totalRead < toTransfer || toTransfer == -1) {
        writeBuffer = readBuffer;
        if (readBuffer == &buf1)
            readBuffer = &buf2;
        else
            readBuffer = &buf1;
        todo = chunkSize;
        if (toTransfer != -1 && toTransfer - *totalRead < (long long)todo)
            todo = (size_t)toTransfer;
        dgs[0] = boost::bind(&read, src, readBuffer, todo, &readResult);
        dgs[1] = boost::bind(&write, dst, writeBuffer, totalWritten);
        parallel_do(dgs);
        if (readResult == 0 && toTransfer != -1) {
        // throw UnexpectedEofException();
        }
        if (readResult == 0)
            return;
    }
    writeBuffer = readBuffer;
    write(dst, writeBuffer, totalWritten);
}

void transferStream(Stream *src, Stream *dst,
                    long long *totalRead, long long *totalWritten)
{
    transferStream(src, dst, -1, totalRead, totalWritten);
}
