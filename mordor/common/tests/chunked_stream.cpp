// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/http/chunked.h"
#include "mordor/common/streams/memory.h"
#include "mordor/test/test.h"

TEST_WITH_SUITE(ChunkedStream, readEmpty)
{
    Stream::ptr baseStream(new MemoryStream(Buffer("0\r\n")));
    Stream::ptr chunkedStream(new HTTP::ChunkedStream(baseStream));

    TEST_ASSERT(chunkedStream->supportsRead());
    TEST_ASSERT(chunkedStream->supportsWrite());
    TEST_ASSERT(!chunkedStream->supportsSeek());
    TEST_ASSERT(!chunkedStream->supportsSize());
    TEST_ASSERT(!chunkedStream->supportsTruncate());
    TEST_ASSERT(!chunkedStream->supportsFind());
    TEST_ASSERT(!chunkedStream->supportsUnread());

    Buffer output;
    TEST_ASSERT_EQUAL(chunkedStream->read(output, 10), 0u);
    TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 3);
}

TEST_WITH_SUITE(ChunkedStream, readSome)
{
    Stream::ptr baseStream(new MemoryStream(Buffer("a\r\nhelloworld\r\n0\r\n")));
    Stream::ptr chunkedStream(new HTTP::ChunkedStream(baseStream));

    Buffer output;
    TEST_ASSERT_EQUAL(chunkedStream->read(output, 15), 10u);
    TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 13);
    TEST_ASSERT(output == "helloworld");

    output.clear();
    TEST_ASSERT_EQUAL(chunkedStream->read(output, 15), 0u);
    TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 18);

    output.clear();
    TEST_ASSERT_EQUAL(chunkedStream->read(output, 15), 0u);
    TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 18);
}

TEST_WITH_SUITE(ChunkedStream, readUppercaseHex)
{
    Stream::ptr baseStream(new MemoryStream(Buffer("A\r\nhelloworld\r\n0\r\n")));
    Stream::ptr chunkedStream(new HTTP::ChunkedStream(baseStream));

    Buffer output;
    TEST_ASSERT_EQUAL(chunkedStream->read(output, 15), 10u);
    TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 13);
    TEST_ASSERT(output == "helloworld");
}

TEST_WITH_SUITE(ChunkedStream, ignoreExtensions)
{
    Stream::ptr baseStream(new MemoryStream(Buffer("a;mordor=rules\r\nhelloworld\r\n0\r\n")));
    Stream::ptr chunkedStream(new HTTP::ChunkedStream(baseStream));

    Buffer output;
    TEST_ASSERT_EQUAL(chunkedStream->read(output, 15), 10u);
    TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 26);
    TEST_ASSERT(output == "helloworld");
}

TEST_WITH_SUITE(ChunkedStream, dontReadPastEnd)
{
    Stream::ptr baseStream(new MemoryStream(Buffer("a\r\nhelloworld\r\n0\r\nmore stuff")));
    Stream::ptr chunkedStream(new HTTP::ChunkedStream(baseStream));

    Buffer output;
    TEST_ASSERT_EQUAL(chunkedStream->read(output, 15), 10u);
    TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 13);
    TEST_ASSERT(output == "helloworld");

    output.clear();
    TEST_ASSERT_EQUAL(chunkedStream->read(output, 15), 0u);
    TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 18);

    output.clear();
    TEST_ASSERT_EQUAL(chunkedStream->read(output, 15), 0u);
    TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 18);

    output.clear();
    TEST_ASSERT_EQUAL(baseStream->read(output, 15), 10u);
    TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 28);
    TEST_ASSERT(output == "more stuff");
}

TEST_WITH_SUITE(ChunkedStream, invalidChunkSize)
{
    Stream::ptr baseStream(new MemoryStream(Buffer("hello\r\n0\r\n")));
    Stream::ptr chunkedStream(new HTTP::ChunkedStream(baseStream));

    try {
        Buffer output;
        chunkedStream->read(output, 15);
        NOTREACHED();
    } catch (const HTTP::InvalidChunkError &ex) {
        TEST_ASSERT_EQUAL(ex.type(), HTTP::InvalidChunkError::HEADER);
        TEST_ASSERT_EQUAL(ex.line(), "hello");
    }
}

TEST_WITH_SUITE(ChunkedStream, invalidChunkData)
{
    Stream::ptr baseStream(new MemoryStream(Buffer("3\r\nhello\r\n0\r\n")));
    Stream::ptr chunkedStream(new HTTP::ChunkedStream(baseStream));

    Buffer output;
    TEST_ASSERT_EQUAL(chunkedStream->read(output, 15), 3u);
    TEST_ASSERT(output == "hel");
    try {
        chunkedStream->read(output, 15);        
        NOTREACHED();
    } catch (const HTTP::InvalidChunkError &ex) {
        TEST_ASSERT_EQUAL(ex.type(), HTTP::InvalidChunkError::FOOTER);
        TEST_ASSERT_EQUAL(ex.line(), "lo");
    }
}

TEST_WITH_SUITE(ChunkedStream, writeEmpty)
{
    MemoryStream::ptr baseStream(new MemoryStream());
    Stream::ptr chunkedStream(new HTTP::ChunkedStream(baseStream));

    TEST_ASSERT(chunkedStream->supportsRead());
    TEST_ASSERT(chunkedStream->supportsWrite());
    TEST_ASSERT(!chunkedStream->supportsSeek());
    TEST_ASSERT(!chunkedStream->supportsSize());
    TEST_ASSERT(!chunkedStream->supportsTruncate());
    TEST_ASSERT(!chunkedStream->supportsFind());
    TEST_ASSERT(!chunkedStream->supportsUnread());

    chunkedStream->close();
    TEST_ASSERT(baseStream->buffer() == "0\r\n");
}

TEST_WITH_SUITE(ChunkedStream, write)
{
    MemoryStream::ptr baseStream(new MemoryStream());
    Stream::ptr chunkedStream(new HTTP::ChunkedStream(baseStream));

    TEST_ASSERT(chunkedStream->supportsRead());
    TEST_ASSERT(chunkedStream->supportsWrite());
    TEST_ASSERT(!chunkedStream->supportsSeek());
    TEST_ASSERT(!chunkedStream->supportsSize());
    TEST_ASSERT(!chunkedStream->supportsTruncate());
    TEST_ASSERT(!chunkedStream->supportsFind());
    TEST_ASSERT(!chunkedStream->supportsUnread());

    TEST_ASSERT_EQUAL(chunkedStream->write("hello"), 5u);
    TEST_ASSERT(baseStream->buffer() == "5\r\nhello\r\n");

    TEST_ASSERT_EQUAL(chunkedStream->write("helloworld"), 10u);
    TEST_ASSERT(baseStream->buffer() == "5\r\nhello\r\na\r\nhelloworld\r\n");

    chunkedStream->close();
    TEST_ASSERT(baseStream->buffer() == "5\r\nhello\r\na\r\nhelloworld\r\n0\r\n");
}
