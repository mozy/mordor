// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/streams/memory.h"
#include "mordor/streams/test.h"
#include "mordor/streams/transfer.h"
#include "mordor/test/test.h"

using namespace Mordor;

MORDOR_UNITTEST(TransferStream, exactLengthMultipleReads)
{
    Stream::ptr inStream(new MemoryStream());
    TestStream::ptr testStream(new TestStream(inStream));
    testStream->maxReadSize(2);
    MemoryStream outStream;
    inStream->write("hello", 5);
    inStream->write("world", 5);
    inStream->seek(0, Stream::BEGIN);
    transferStream(testStream, outStream, 7);
    MORDOR_TEST_ASSERT_EQUAL(outStream.size(), 7);
    MORDOR_TEST_ASSERT_EQUAL(inStream->tell(), 7);
}

MORDOR_UNITTEST(TransferStream, untilEof)
{
    MemoryStream inStream(Buffer("hello"));
    MemoryStream outStream;
    MORDOR_TEST_ASSERT_EQUAL(transferStream(inStream, outStream, 10, UNTILEOF),
        5ull);
    inStream.seek(0);
    MORDOR_TEST_ASSERT_EXCEPTION(transferStream(inStream, outStream, 10,
        EXACT), UnexpectedEofException);
    inStream.seek(0);
    MORDOR_TEST_ASSERT_EXCEPTION(transferStream(inStream, outStream, 10),
        UnexpectedEofException);
}

MORDOR_UNITTEST(TransferStream, inferEof)
{
    MemoryStream inStream(Buffer("hello"));
    MemoryStream outStream;
    MORDOR_TEST_ASSERT_EQUAL(transferStream(inStream, outStream), 5ull);
}
