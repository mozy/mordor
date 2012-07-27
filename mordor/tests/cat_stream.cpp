// Copyright (c) 2010 - Mozy, Inc.

#include <vector>

#include "mordor/streams/cat.h"
#include "mordor/streams/buffer.h"
#include "mordor/streams/memory.h"
#include "mordor/streams/null.h"
#include "mordor/streams/stream.h"
#include "mordor/streams/transfer.h"
#include "mordor/test/test.h"

using namespace Mordor;

namespace {

static Stream::ptr genMemoryStream(size_t length, char b)
{
    return Stream::ptr(new MemoryStream(Buffer(std::string(length, b))));
}

static void readFully(Stream::ptr stream, Buffer &buffer, size_t size)
{
    while(size > 0) {
        size_t read = stream->read(buffer, size);
        if (read == 0)
            MORDOR_THROW_EXCEPTION(Mordor::UnexpectedEofException());
        size -= read;
    }
}

MORDOR_UNITTEST(CatStream, catStreamSeekTest)
{
    std::vector<Stream::ptr> streams;
    streams.push_back(genMemoryStream(1, '0'));
    streams.push_back(genMemoryStream(10, '1'));
    streams.push_back(genMemoryStream(30, '2'));

    Stream::ptr catStream(new CatStream(streams));
    transferStream(catStream, NullStream::get_ptr());

    Buffer buf;

    catStream->seek(0);
    readFully(catStream, buf, 1);
    MORDOR_TEST_ASSERT_EQUAL(buf.toString(), std::string(1, '0'));
    buf.consume(1);

    catStream->seek(2);
    readFully(catStream, buf, 9);
    MORDOR_TEST_ASSERT_EQUAL(buf.toString(), std::string(9, '1'));
    buf.consume(9);

    catStream->seek(13);
    readFully(catStream, buf, 28);
    MORDOR_TEST_ASSERT_EQUAL(buf.toString(), std::string(28, '2'));
    buf.consume(28);

    MORDOR_TEST_ASSERT_EXCEPTION(catStream->seek(100, Stream::CURRENT), std::invalid_argument);
    MORDOR_TEST_ASSERT_EXCEPTION(catStream->seek(-15), std::invalid_argument);
}

}
