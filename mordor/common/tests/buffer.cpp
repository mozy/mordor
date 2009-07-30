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
