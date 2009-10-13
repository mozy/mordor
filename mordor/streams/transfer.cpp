// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "transfer.h"

#include <boost/bind.hpp>

#include "mordor/exception.h"
#include "mordor/scheduler.h"
#include "stream.h"

namespace Mordor {

static void readOne(Stream &src, Buffer &buffer, size_t len, size_t &result)
{
    result = src.read(buffer, len);
}

static void writeOne(Stream &dst, Buffer &buffer, unsigned long long *totalWritten)
{
    size_t result;
    while (buffer.readAvailable() > 0) {
        result = dst.write(buffer, buffer.readAvailable());
        buffer.consume(result);
        if (totalWritten)
            *totalWritten += result;
    }
}

void transferStream(Stream &src, Stream &dst,
                            unsigned long long toTransfer,
                            unsigned long long *totalRead,
                            unsigned long long *totalWritten)
{
    MORDOR_ASSERT(src.supportsRead());
    MORDOR_ASSERT(dst.supportsWrite());
    Buffer buf1, buf2;
    Buffer *readBuffer, *writeBuffer;
    size_t chunkSize = 65536;
    size_t todo;
    size_t readResult;
    unsigned long long totalReadS;
    if (!totalRead)
        totalRead = &totalReadS;
    *totalRead = 0;
    if (totalWritten)
        *totalWritten = 0;
    if (toTransfer == 0)
        return;

    readBuffer = &buf1;
    todo = chunkSize;
    if (toTransfer - *totalRead < (unsigned long long)todo)
        todo = (size_t)toTransfer;
    readOne(src, *readBuffer, todo, readResult);
    *totalRead += readResult;
    if (readResult == 0 && toTransfer != ~0ull) {
        MORDOR_THROW_EXCEPTION(UnexpectedEofException());
    }
    if (readResult == 0)
        return;

    std::vector<boost::function<void ()> > dgs;
    std::vector<Fiber::ptr> fibers;
    dgs.resize(2);
    fibers.resize(2);
    fibers[0].reset(new Fiber(NULL));
    fibers[1].reset(new Fiber(NULL));
    while (*totalRead < toTransfer) {
        writeBuffer = readBuffer;
        if (readBuffer == &buf1)
            readBuffer = &buf2;
        else
            readBuffer = &buf1;
        todo = chunkSize;
        if (toTransfer - *totalRead < (unsigned long long)todo)
            todo = (size_t)toTransfer;
        dgs[0] = boost::bind(&readOne, boost::ref(src), boost::ref(*readBuffer), todo, 
boost::ref(readResult));
        dgs[1] = boost::bind(&writeOne, boost::ref(dst), boost::ref(*writeBuffer), 
totalWritten);
        parallel_do(dgs, fibers);
        *totalRead += readResult;
        if (readResult == 0 && toTransfer != ~0ull && *totalRead < toTransfer) {
            MORDOR_THROW_EXCEPTION(UnexpectedEofException());
        }
        if (readResult == 0)
            return;
    }
    writeBuffer = readBuffer;
    writeOne(dst, *writeBuffer, totalWritten);
}

void transferStream(Stream::ptr src, Stream &dst,
                    unsigned long long toTransfer,
                    unsigned long long *totalRead,
                    unsigned long long *totalWritten)
{ transferStream(*src.get(), dst, toTransfer, totalRead, totalWritten); }
void transferStream(Stream &src, Stream::ptr dst,
                    unsigned long long toTransfer,
                    unsigned long long *totalRead,
                    unsigned long long *totalWritten)
{ transferStream(src, *dst.get(), toTransfer, totalRead, totalWritten); }
void transferStream(Stream::ptr src, Stream::ptr dst,
                    unsigned long long toTransfer,
                    unsigned long long *totalRead,
                    unsigned long long *totalWritten)
{ transferStream(*src.get(), *dst.get(), toTransfer, totalRead, totalWritten); }

void transferStream(Stream &src, Stream &dst,
                    unsigned long long *totalRead,
                    unsigned long long *totalWritten)
{ transferStream(src, dst, ~0, totalRead, totalWritten); }
void transferStream(Stream::ptr src, Stream &dst,
                    unsigned long long *totalRead,
                    unsigned long long *totalWritten)
{ transferStream(*src.get(), dst, ~0, totalRead, totalWritten); }
void transferStream(Stream &src, Stream::ptr dst,
                    unsigned long long *totalRead,
                    unsigned long long *totalWritten)
{ transferStream(src, *dst.get(), ~0, totalRead, totalWritten); }
void transferStream(Stream::ptr src, Stream::ptr dst,
                    unsigned long long *totalRead,
                    unsigned long long *totalWritten)
{ transferStream(*src.get(), *dst.get(), ~0, totalRead, totalWritten); }

}
