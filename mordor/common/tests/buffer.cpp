// Copyright (c) 2009 - Decho Corp.

#include <boost/bind.hpp>

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

TEST_WITH_SUITE(Buffer, compareEmpty)
{
    Buffer buf1, buf2;
    TEST_ASSERT(buf1 == buf2);
    TEST_ASSERT(!(buf1 != buf2));
}

TEST_WITH_SUITE(Buffer, compareSimpleInequality)
{
    Buffer buf1, buf2("h");
    TEST_ASSERT(buf1 != buf2);
    TEST_ASSERT(!(buf1 == buf2));
}

TEST_WITH_SUITE(Buffer, compareIdentical)
{
    Buffer buf1("hello"), buf2("hello");
    TEST_ASSERT(buf1 == buf2);
    TEST_ASSERT(!(buf1 != buf2));
}

TEST_WITH_SUITE(Buffer, compareLotsOfSegmentsOnTheLeft)
{
    Buffer buf1, buf2("hello world!");
    buf1.copyIn("he");
    buf1.copyIn("l");
    buf1.copyIn("l");
    buf1.copyIn("o wor");
    buf1.copyIn("ld!");
    TEST_ASSERT(buf1 == buf2);
    TEST_ASSERT(!(buf1 != buf2));
}

TEST_WITH_SUITE(Buffer, compareLotOfSegmentsOnTheRight)
{
    Buffer buf1("hello world!"), buf2;
    buf2.copyIn("he");
    buf2.copyIn("l");
    buf2.copyIn("l");
    buf2.copyIn("o wor");
    buf2.copyIn("ld!");
    TEST_ASSERT(buf1 == buf2);
    TEST_ASSERT(!(buf1 != buf2));
}

TEST_WITH_SUITE(Buffer, compareLotsOfSegments)
{
    Buffer buf1, buf2;
    buf1.copyIn("he");
    buf1.copyIn("l");
    buf1.copyIn("l");
    buf1.copyIn("o wor");
    buf1.copyIn("ld!");
    buf2.copyIn("he");
    buf2.copyIn("l");
    buf2.copyIn("l");
    buf2.copyIn("o wor");
    buf2.copyIn("ld!");
    TEST_ASSERT(buf1 == buf2);
    TEST_ASSERT(!(buf1 != buf2));
}

TEST_WITH_SUITE(Buffer, compareLotsOfMismatchedSegments)
{
    Buffer buf1, buf2;
    buf1.copyIn("hel");
    buf1.copyIn("lo ");
    buf1.copyIn("wo");
    buf1.copyIn("rld!");
    buf2.copyIn("he");
    buf2.copyIn("l");
    buf2.copyIn("l");
    buf2.copyIn("o wor");
    buf2.copyIn("ld!");
    TEST_ASSERT(buf1 == buf2);
    TEST_ASSERT(!(buf1 != buf2));
}

TEST_WITH_SUITE(Buffer, compareLotsOfSegmentsOnTheLeftInequality)
{
    Buffer buf1, buf2("hello world!");
    buf1.copyIn("he");
    buf1.copyIn("l");
    buf1.copyIn("l");
    buf1.copyIn("o wor");
    buf1.copyIn("ld! ");
    TEST_ASSERT(buf1 != buf2);
    TEST_ASSERT(!(buf1 == buf2));
}

TEST_WITH_SUITE(Buffer, compareLotOfSegmentsOnTheRightInequality)
{
    Buffer buf1("hello world!"), buf2;
    buf2.copyIn("he");
    buf2.copyIn("l");
    buf2.copyIn("l");
    buf2.copyIn("o wor");
    buf2.copyIn("ld! ");
    TEST_ASSERT(buf1 != buf2);
    TEST_ASSERT(!(buf1 == buf2));
}

TEST_WITH_SUITE(Buffer, compareLotsOfSegmentsInequality)
{
    Buffer buf1, buf2;
    buf1.copyIn("he");
    buf1.copyIn("l");
    buf1.copyIn("l");
    buf1.copyIn("o wor");
    buf1.copyIn("ld!");
    buf2.copyIn("he");
    buf2.copyIn("l");
    buf2.copyIn("l");
    buf2.copyIn("o wor");
    buf2.copyIn("ld! ");
    TEST_ASSERT(buf1 != buf2);
    TEST_ASSERT(!(buf1 == buf2));
}

TEST_WITH_SUITE(Buffer, compareLotsOfMismatchedSegmentsInequality)
{
    Buffer buf1, buf2;
    buf1.copyIn("hel");
    buf1.copyIn("lo ");
    buf1.copyIn("wo");
    buf1.copyIn("rld!");
    buf2.copyIn("he");
    buf2.copyIn("l");
    buf2.copyIn("l");
    buf2.copyIn("o wor");
    buf2.copyIn("ld! ");
    TEST_ASSERT(buf1 != buf2);
    TEST_ASSERT(!(buf1 == buf2));
}

TEST_WITH_SUITE(Buffer, reserveWithReadAvailable)
{
    Buffer buf1("hello");
    buf1.reserve(10);
    TEST_ASSERT_EQUAL(buf1.readAvailable(), 5u);
    TEST_ASSERT_GREATER_THAN_OR_EQUAL(buf1.writeAvailable(), 10u);
}

TEST_WITH_SUITE(Buffer, reserveWithWriteAvailable)
{
    Buffer buf1;
    buf1.reserve(5);
    // Internal knowledge that reserve doubles the reservation
    TEST_ASSERT_EQUAL(buf1.writeAvailable(), 10u);
    buf1.reserve(11);
    TEST_ASSERT_EQUAL(buf1.writeAvailable(), 22u);
}

TEST_WITH_SUITE(Buffer, reserveWithReadAndWriteAvailable)
{
    Buffer buf1("hello");
    buf1.reserve(5);
    // Internal knowledge that reserve doubles the reservation
    TEST_ASSERT_EQUAL(buf1.readAvailable(), 5u);
    TEST_ASSERT_EQUAL(buf1.writeAvailable(), 10u);
    buf1.reserve(11);
    TEST_ASSERT_EQUAL(buf1.readAvailable(), 5u);
    TEST_ASSERT_EQUAL(buf1.writeAvailable(), 22u);
}

static void
visitor1(const void *b, size_t len)
{
    NOTREACHED();
}

TEST_WITH_SUITE(Buffer, visitEmpty)
{
    Buffer b;
    b.visit(&visitor1);
}

TEST_WITH_SUITE(Buffer, visitNonEmpty0)
{
    Buffer b("hello");
    b.visit(&visitor1, 0);
}

static void
visitor2(const void *b, size_t len, int &sequence)
{
    TEST_ASSERT_EQUAL(++sequence, 1);
    TEST_ASSERT_EQUAL(len, 5);
    TEST_ASSERT(memcmp(b, "hello", 5) == 0);
}

TEST_WITH_SUITE(Buffer, visitSingleSegment)
{
    Buffer b("hello");
    int sequence = 0;
    b.visit(boost::bind(&visitor2, _1, _2, boost::ref(sequence)));
    TEST_ASSERT_EQUAL(++sequence, 2);
}

static void
visitor3(const void *b, size_t len, int &sequence)
{
    switch (len) {
        case 1:
            TEST_ASSERT_EQUAL(++sequence, 1);
            TEST_ASSERT(memcmp(b, "a", 1) == 0);
            break;
        case 2:
            TEST_ASSERT_EQUAL(++sequence, 2);
            TEST_ASSERT(memcmp(b, "bc", 2) == 0);
            break;
        default:
            NOTREACHED();
    }
}

TEST_WITH_SUITE(Buffer, visitMultipleSegments)
{
    Buffer b;
    int sequence = 0;
    b.copyIn("a");
    b.copyIn("bc");
    b.visit(boost::bind(&visitor3, _1, _2, boost::ref(sequence)));
    TEST_ASSERT_EQUAL(++sequence, 3);
}

TEST_WITH_SUITE(Buffer, visitMultipleSegmentsPartial)
{
    Buffer b;
    int sequence = 0;
    b.copyIn("a");
    b.copyIn("bcd");
    b.visit(boost::bind(&visitor3, _1, _2, boost::ref(sequence)), 3);
    TEST_ASSERT_EQUAL(++sequence, 3);
}

TEST_WITH_SUITE(Buffer, visitWithWriteSegment)
{
    Buffer b("hello");
    b.reserve(5);
    int sequence = 0;
    b.visit(boost::bind(&visitor2, _1, _2, boost::ref(sequence)));
    TEST_ASSERT_EQUAL(++sequence, 2);
}

TEST_WITH_SUITE(Buffer, visitWithMixedSegment)
{
    Buffer b;
    b.reserve(10);
    b.copyIn("hello");
    int sequence = 0;
    b.visit(boost::bind(&visitor2, _1, _2, boost::ref(sequence)));
    TEST_ASSERT_EQUAL(++sequence, 2);
}

#ifdef DEBUG
TEST_WITH_SUITE(Buffer, visitMoreThanThereIs)
{
    Buffer b;
    TEST_ASSERT_ASSERTED(b.visit(&visitor1, 1));
}
#endif
