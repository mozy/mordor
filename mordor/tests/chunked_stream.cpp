// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "mordor/http/chunked.h"
#include "mordor/streams/memory.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

MORDOR_UNITTEST(ChunkedStream, readEmpty)
{
    Stream::ptr baseStream(new MemoryStream(Buffer("0\r\n")));
    Stream::ptr chunkedStream(new HTTP::ChunkedStream(baseStream));

    MORDOR_TEST_ASSERT(chunkedStream->supportsRead());
    MORDOR_TEST_ASSERT(chunkedStream->supportsWrite());
    MORDOR_TEST_ASSERT(!chunkedStream->supportsSeek());
    MORDOR_TEST_ASSERT(!chunkedStream->supportsTell());
    MORDOR_TEST_ASSERT(!chunkedStream->supportsSize());
    MORDOR_TEST_ASSERT(!chunkedStream->supportsTruncate());
    MORDOR_TEST_ASSERT(!chunkedStream->supportsFind());
    MORDOR_TEST_ASSERT(!chunkedStream->supportsUnread());

    Buffer output;
    MORDOR_TEST_ASSERT_EQUAL(chunkedStream->read(output, 10), 0u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 3);
}

MORDOR_UNITTEST(ChunkedStream, readSome)
{
    Stream::ptr baseStream(new MemoryStream(Buffer("a\r\nhelloworld\r\n0\r\n")));
    Stream::ptr chunkedStream(new HTTP::ChunkedStream(baseStream));

    Buffer output;
    MORDOR_TEST_ASSERT_EQUAL(chunkedStream->read(output, 15), 10u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 13);
    MORDOR_TEST_ASSERT(output == "helloworld");

    output.clear();
    MORDOR_TEST_ASSERT_EQUAL(chunkedStream->read(output, 15), 0u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 18);

    output.clear();
    MORDOR_TEST_ASSERT_EQUAL(chunkedStream->read(output, 15), 0u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 18);
}

MORDOR_UNITTEST(ChunkedStream, readUppercaseHex)
{
    Stream::ptr baseStream(new MemoryStream(Buffer("A\r\nhelloworld\r\n0\r\n")));
    Stream::ptr chunkedStream(new HTTP::ChunkedStream(baseStream));

    Buffer output;
    MORDOR_TEST_ASSERT_EQUAL(chunkedStream->read(output, 15), 10u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 13);
    MORDOR_TEST_ASSERT(output == "helloworld");
}

MORDOR_UNITTEST(ChunkedStream, ignoreExtensions)
{
    Stream::ptr baseStream(new MemoryStream(Buffer("a;mordor=rules\r\nhelloworld\r\n0\r\n")));
    Stream::ptr chunkedStream(new HTTP::ChunkedStream(baseStream));

    Buffer output;
    MORDOR_TEST_ASSERT_EQUAL(chunkedStream->read(output, 15), 10u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 26);
    MORDOR_TEST_ASSERT(output == "helloworld");
}

MORDOR_UNITTEST(ChunkedStream, dontReadPastEnd)
{
    Stream::ptr baseStream(new MemoryStream(Buffer("a\r\nhelloworld\r\n0\r\nmore stuff")));
    Stream::ptr chunkedStream(new HTTP::ChunkedStream(baseStream));

    Buffer output;
    MORDOR_TEST_ASSERT_EQUAL(chunkedStream->read(output, 15), 10u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 13);
    MORDOR_TEST_ASSERT(output == "helloworld");

    output.clear();
    MORDOR_TEST_ASSERT_EQUAL(chunkedStream->read(output, 15), 0u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 18);

    output.clear();
    MORDOR_TEST_ASSERT_EQUAL(chunkedStream->read(output, 15), 0u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 18);

    output.clear();
    MORDOR_TEST_ASSERT_EQUAL(baseStream->read(output, 15), 10u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 28);
    MORDOR_TEST_ASSERT(output == "more stuff");
}

MORDOR_UNITTEST(ChunkedStream, invalidChunkSize)
{
    Stream::ptr baseStream(new MemoryStream(Buffer("hello\r\n0\r\n")));
    Stream::ptr chunkedStream(new HTTP::ChunkedStream(baseStream));

    try {
        Buffer output;
        chunkedStream->read(output, 15);
        MORDOR_NOTREACHED();
    } catch (const HTTP::InvalidChunkException &ex) {
        MORDOR_TEST_ASSERT_EQUAL(ex.type(), HTTP::InvalidChunkException::HEADER);
        MORDOR_TEST_ASSERT_EQUAL(ex.line(), "hello");
    }
}

MORDOR_UNITTEST(ChunkedStream, invalidChunkData)
{
    Stream::ptr baseStream(new MemoryStream(Buffer("3\r\nhello\r\n0\r\n")));
    Stream::ptr chunkedStream(new HTTP::ChunkedStream(baseStream));

    Buffer output;
    MORDOR_TEST_ASSERT_EQUAL(chunkedStream->read(output, 15), 3u);
    MORDOR_TEST_ASSERT(output == "hel");
    try {
        chunkedStream->read(output, 15);        
        MORDOR_NOTREACHED();
    } catch (const HTTP::InvalidChunkException &ex) {
        MORDOR_TEST_ASSERT_EQUAL(ex.type(), HTTP::InvalidChunkException::FOOTER);
        MORDOR_TEST_ASSERT_EQUAL(ex.line(), "lo");
    }
}

MORDOR_UNITTEST(ChunkedStream, writeEmpty)
{
    MemoryStream::ptr baseStream(new MemoryStream());
    Stream::ptr chunkedStream(new HTTP::ChunkedStream(baseStream));

    MORDOR_TEST_ASSERT(chunkedStream->supportsRead());
    MORDOR_TEST_ASSERT(chunkedStream->supportsWrite());
    MORDOR_TEST_ASSERT(!chunkedStream->supportsSeek());
    MORDOR_TEST_ASSERT(!chunkedStream->supportsTell());
    MORDOR_TEST_ASSERT(!chunkedStream->supportsSize());
    MORDOR_TEST_ASSERT(!chunkedStream->supportsTruncate());
    MORDOR_TEST_ASSERT(!chunkedStream->supportsFind());
    MORDOR_TEST_ASSERT(!chunkedStream->supportsUnread());

    chunkedStream->close();
    MORDOR_TEST_ASSERT(baseStream->buffer() == "0\r\n");
}

MORDOR_UNITTEST(ChunkedStream, write)
{
    MemoryStream::ptr baseStream(new MemoryStream());
    Stream::ptr chunkedStream(new HTTP::ChunkedStream(baseStream));

    MORDOR_TEST_ASSERT(chunkedStream->supportsRead());
    MORDOR_TEST_ASSERT(chunkedStream->supportsWrite());
    MORDOR_TEST_ASSERT(!chunkedStream->supportsSeek());
    MORDOR_TEST_ASSERT(!chunkedStream->supportsTell());
    MORDOR_TEST_ASSERT(!chunkedStream->supportsSize());
    MORDOR_TEST_ASSERT(!chunkedStream->supportsTruncate());
    MORDOR_TEST_ASSERT(!chunkedStream->supportsFind());
    MORDOR_TEST_ASSERT(!chunkedStream->supportsUnread());

    MORDOR_TEST_ASSERT_EQUAL(chunkedStream->write("hello"), 5u);
    MORDOR_TEST_ASSERT(baseStream->buffer() == "5\r\nhello\r\n");

    MORDOR_TEST_ASSERT_EQUAL(chunkedStream->write("helloworld"), 10u);
    MORDOR_TEST_ASSERT(baseStream->buffer() == "5\r\nhello\r\na\r\nhelloworld\r\n");

    chunkedStream->close();
    MORDOR_TEST_ASSERT(baseStream->buffer() == "5\r\nhello\r\na\r\nhelloworld\r\n0\r\n");
}
