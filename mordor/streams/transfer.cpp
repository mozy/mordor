// Copyright (c) 2009 - Mozy, Inc.

#include "transfer.h"

#include <boost/bind.hpp>

#include "mordor/assert.h"
#include "mordor/config.h"
#include "mordor/fiber.h"
#include "mordor/parallel.h"
#include "mordor/streams/buffer.h"
#include "mordor/streams/null.h"
#include "stream.h"

namespace Mordor {

static ConfigVar<size_t>::ptr g_chunkSize =
    Config::lookup("transferstream.chunksize",
                   (size_t)65536,
                   "transfer chunk size.");

static ConfigVar<size_t>::ptr g_defaultSize =
    Config::lookup<size_t>("transferstream.defaultsize", 4096,
    "Default size for transfer stream");

static Logger::ptr g_log = Log::lookup("mordor:stream:transfer");

static void readOne(Stream &src, Buffer *&buffer, size_t len, size_t &result)
{
    result = src.read(*buffer, len);
    MORDOR_LOG_TRACE(g_log) << "read " << result << " bytes from " << &src;
}

static void writeOne(Stream &dst, Buffer *&buffer)
{
    size_t result;
    while (buffer->readAvailable() > 0) {
        result = dst.write(*buffer, buffer->readAvailable());
        MORDOR_LOG_TRACE(g_log) << "wrote " << result << " bytes to " << &dst;
        buffer->consume(result);
    }
}

unsigned long long transferStream(Stream &src, Stream &dst,
                                  unsigned long long toTransfer,
                                  ExactLength exactLength)
{
    MORDOR_LOG_DEBUG(g_log) << "transferring " << toTransfer << " bytes from "
        << &src << " to " << &dst;
    MORDOR_ASSERT(src.supportsRead());
    MORDOR_ASSERT(dst.supportsWrite());
    Buffer buf1, buf2;
    Buffer *readBuffer, *writeBuffer;
    size_t chunkSize = g_chunkSize->val();
    size_t todo;
    size_t readResult;
    unsigned long long totalRead = 0;
    if (toTransfer == 0)
        return 0;
    if (exactLength == INFER)
        exactLength = (toTransfer == ~0ull ? UNTILEOF : EXACT);
    MORDOR_ASSERT(exactLength == EXACT || exactLength == UNTILEOF);

    readBuffer = &buf1;
    todo = chunkSize;
    if (toTransfer - totalRead < (unsigned long long)todo)
        todo = (size_t)(toTransfer - totalRead);
    readOne(src, readBuffer, todo, readResult);
    totalRead += readResult;
    if (readResult == 0 && exactLength == EXACT)
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
            if (readResult == 0 && exactLength == EXACT)
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
        if (readResult == 0 && exactLength == EXACT && totalRead < toTransfer) {
            MORDOR_LOG_ERROR(g_log) << "only read " << totalRead << "/"
                << toTransfer << " from " << &src;
            MORDOR_THROW_EXCEPTION(UnexpectedEofException());
        }
        if (readResult == 0)
            return totalRead;
    }
    writeBuffer = readBuffer;
    writeOne(dst, writeBuffer);
    MORDOR_LOG_VERBOSE(g_log) << "transferred " << totalRead << "/" << toTransfer
        << " from " << &src << " to " << &dst;
    return totalRead;
}

unsigned long long transferStreamDirect(Stream::ptr src, Stream::ptr dst) {
	size_t size = g_defaultSize->val();
	char buffer[size];
	size_t total = 0;

	while (true)
	{
		size_t len = src->read(buffer, size);
		total += len;
		if (!len)
			break;
		dst->write (buffer, len);
	}

	return total;
}

}
