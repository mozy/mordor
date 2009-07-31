// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/streams/buffer.h"
#include "mordor/test/test.h"

TEST_WITH_SUITE(Buffer, copyConstructor)
{
    Buffer buf1;
    buf1.copyIn("hello");
    Buffer buf2(buf1);
    TEST_ASSERT(buf1 == "hello");
    TEST_ASSERT(buf2 == "hello");
    TEST_ASSERT_EQUAL(buf1.writeAvailable(), 0u);
    TEST_ASSERT_EQUAL(buf2.writeAvailable(), 0u);
}

TEST_WITH_SUITE(Buffer, copyConstructorImmutability)
{
    Buffer buf1;
    buf1.reserve(10);
    Buffer buf2(buf1);
    buf1.copyIn("hello");
    buf2.copyIn("tommy");
    TEST_ASSERT_EQUAL(buf1.readAvailable(), 5u);
    TEST_ASSERT_GREATER_THAN_OR_EQUAL(buf1.writeAvailable(), 5u);
    TEST_ASSERT_EQUAL(buf2.readAvailable(), 5u);
    TEST_ASSERT_EQUAL(buf2.writeAvailable(), 0u);
    TEST_ASSERT(buf1 == "hello");
    TEST_ASSERT(buf2 == "tommy");
}

TEST_WITH_SUITE(Buffer, truncate)
{
    Buffer buf("hello");
    buf.truncate(3);
    TEST_ASSERT(buf == "hel");
}

TEST_WITH_SUITE(Buffer, truncateMultipleSegments1)
{
    Buffer buf("hello");
    buf.copyIn("world");
    buf.truncate(3);
    TEST_ASSERT(buf == "hel");
}

TEST_WITH_SUITE(Buffer, truncateMultipleSegments2)
{
    Buffer buf("hello");
    buf.copyIn("world");
    buf.truncate(8);
    TEST_ASSERT(buf == "hellowor");
}

TEST_WITH_SUITE(Buffer, truncateBeforeWriteSegments)
{
    Buffer buf("hello");
    buf.reserve(5);
    buf.truncate(3);
    TEST_ASSERT(buf == "hel");
    TEST_ASSERT_GREATER_THAN_OR_EQUAL(buf.writeAvailable(), 5u);
}

TEST_WITH_SUITE(Buffer, truncateAtWriteSegments)
{
    Buffer buf("hello");
    buf.reserve(10);
    buf.copyIn("world");
    buf.truncate(8);
    TEST_ASSERT(buf == "hellowor");
    TEST_ASSERT_GREATER_THAN_OR_EQUAL(buf.writeAvailable(), 10u);
}
