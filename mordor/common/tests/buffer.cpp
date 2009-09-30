// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include <boost/bind.hpp>

#include "mordor/common/streams/buffer.h"
#include "mordor/test/test.h"

TEST_WITH_SUITE(Buffer, copyInString)
{
    Buffer b;
    b.copyIn("hello");
    TEST_ASSERT_EQUAL(b.readAvailable(), 5u);
    TEST_ASSERT_EQUAL(b.writeAvailable(), 0u);
    TEST_ASSERT_EQUAL(b.segments(), 1u);
    TEST_ASSERT(b == "hello");
}

TEST_WITH_SUITE(Buffer, copyInOtherBuffer)
{
    Buffer b1, b2("hello");
    b1.copyIn(b2);
    TEST_ASSERT_EQUAL(b1.readAvailable(), 5u);
    TEST_ASSERT_EQUAL(b1.writeAvailable(), 0u);
    TEST_ASSERT_EQUAL(b1.segments(), 1u);
    TEST_ASSERT(b1 == "hello");
}

TEST_WITH_SUITE(Buffer, copyInPartial)
{
    Buffer b1, b2("hello");
    b1.copyIn(b2, 3);
    TEST_ASSERT_EQUAL(b1.readAvailable(), 3u);
    TEST_ASSERT_EQUAL(b1.writeAvailable(), 0u);
    TEST_ASSERT_EQUAL(b1.segments(), 1u);
    TEST_ASSERT(b1 == "hel");
}

TEST_WITH_SUITE(Buffer, copyInStringToReserved)
{
    Buffer b;
    b.reserve(5);
    b.copyIn("hello");
    TEST_ASSERT_EQUAL(b.readAvailable(), 5u);
    TEST_ASSERT_EQUAL(b.segments(), 1u);
    TEST_ASSERT(b == "hello");
}

TEST_WITH_SUITE(Buffer, copyInStringAfterAnotherSegment)
{
    Buffer b("hello");
    b.copyIn("world");
    TEST_ASSERT_EQUAL(b.readAvailable(), 10u);
    TEST_ASSERT_EQUAL(b.writeAvailable(), 0u);
    TEST_ASSERT_EQUAL(b.segments(), 2u);
    TEST_ASSERT(b == "helloworld");
}

TEST_WITH_SUITE(Buffer, copyInStringToReservedAfterAnotherSegment)
{
    Buffer b("hello");
    b.reserve(5);
    b.copyIn("world");
    TEST_ASSERT_EQUAL(b.readAvailable(), 10u);
    TEST_ASSERT_EQUAL(b.segments(), 2u);
    TEST_ASSERT(b == "helloworld");
}

TEST_WITH_SUITE(Buffer, copyInStringToSplitSegment)
{
    Buffer b;
    b.reserve(10);
    b.copyIn("hello");
    TEST_ASSERT_EQUAL(b.readAvailable(), 5u);
    TEST_ASSERT_GREATER_THAN_OR_EQUAL(b.writeAvailable(), 5u);
    TEST_ASSERT_EQUAL(b.segments(), 1u);
    b.copyIn("world");
    TEST_ASSERT_EQUAL(b.readAvailable(), 10u);
    TEST_ASSERT_EQUAL(b.segments(), 1u);
    TEST_ASSERT(b == "helloworld");
}

TEST_WITH_SUITE(Buffer, copyInWithReserve)
{
    Buffer b1, b2("hello");
    b1.reserve(10);
    TEST_ASSERT_GREATER_THAN_OR_EQUAL(b1.writeAvailable(), 10u);
    TEST_ASSERT_EQUAL(b1.segments(), 1u);
    size_t writeAvailable = b1.writeAvailable();
    b1.copyIn(b2);
    TEST_ASSERT_EQUAL(b1.readAvailable(), 5u);
    // Shouldn't have eaten any
    TEST_ASSERT_EQUAL(b1.writeAvailable(), writeAvailable);
    TEST_ASSERT_EQUAL(b1.segments(), 2u);
    TEST_ASSERT(b1 == "hello");
}

TEST_WITH_SUITE(Buffer, copyInToSplitSegment)
{
    Buffer b1, b2("world");
    b1.reserve(10);
    b1.copyIn("hello");
    TEST_ASSERT_EQUAL(b1.readAvailable(), 5u);
    TEST_ASSERT_GREATER_THAN_OR_EQUAL(b1.writeAvailable(), 5u);
    TEST_ASSERT_EQUAL(b1.segments(), 1u);
    size_t writeAvailable = b1.writeAvailable();
    b1.copyIn(b2, 5);
    TEST_ASSERT_EQUAL(b1.readAvailable(), 10u);
    // Shouldn't have eaten any
    TEST_ASSERT_EQUAL(b1.writeAvailable(), writeAvailable);
    TEST_ASSERT_EQUAL(b1.segments(), 3u);
    TEST_ASSERT(b1 == "helloworld");
}

#ifdef DEBUG
TEST_WITH_SUITE(Buffer, copyInMoreThanThereIs)
{
    Buffer b1, b2;
    TEST_ASSERT_ASSERTED(b1.copyIn(b2, 1));
    b2.copyIn("hello");
    TEST_ASSERT_ASSERTED(b1.copyIn(b2, 6));
}
#endif

TEST_WITH_SUITE(Buffer, copyInMerge)
{
    Buffer b1, b2("hello");
    b1.copyIn(b2, 2);
    b2.consume(2);
    b1.copyIn(b2, 3);
    TEST_ASSERT_EQUAL(b1.readAvailable(), 5u);
    TEST_ASSERT_EQUAL(b1.segments(), 1u);
    TEST_ASSERT(b1 == "hello");
}

TEST_WITH_SUITE(Buffer, copyInMergePlus)
{
    Buffer b1, b2("hello");
    b2.copyIn("world");
    TEST_ASSERT_EQUAL(b2.segments(), 2u);
    b1.copyIn(b2, 2);
    b2.consume(2);
    b1.copyIn(b2, 4);
    TEST_ASSERT_EQUAL(b1.readAvailable(), 6u);
    TEST_ASSERT_EQUAL(b1.segments(), 2u);
    TEST_ASSERT(b1 == "hellow");
}

TEST_WITH_SUITE(Buffer, noSplitOnTruncate)
{
    Buffer b1;
    b1.reserve(10);
    b1.copyIn("hello");
    b1.truncate(5);
    TEST_ASSERT_GREATER_THAN_OR_EQUAL(b1.writeAvailable(), 5u);
    b1.copyIn("world");
    TEST_ASSERT_EQUAL(b1.segments(), 1u);
    TEST_ASSERT(b1 == "helloworld");
}

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
    TEST_ASSERT_EQUAL(len, 5u);
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

TEST_WITH_SUITE(Buffer, findCharEmpty)
{
    Buffer b;
    TEST_ASSERT_EQUAL(b.segments(), 0u);
    TEST_ASSERT_EQUAL(b.find('\n'), -1);
    TEST_ASSERT_EQUAL(b.find('\n', 0), -1);

#ifdef DEBUG
    TEST_ASSERT_ASSERTED(b.find('\n', 1));
#endif

    // Put a write segment on the end
    b.reserve(10);
    TEST_ASSERT_EQUAL(b.segments(), 1u);
    TEST_ASSERT_EQUAL(b.find('\n'), -1);

#ifdef DEBUG
    TEST_ASSERT_ASSERTED(b.find('\n', 1));
#endif
}

TEST_WITH_SUITE(Buffer, findCharSimple)
{
    Buffer b("\nhello");
    TEST_ASSERT_EQUAL(b.segments(), 1u);

    TEST_ASSERT_EQUAL(b.find('\r'), -1);
    TEST_ASSERT_EQUAL(b.find('\n'), 0);
    TEST_ASSERT_EQUAL(b.find('h'), 1);
    TEST_ASSERT_EQUAL(b.find('e'), 2);
    TEST_ASSERT_EQUAL(b.find('l'), 3);
    TEST_ASSERT_EQUAL(b.find('o'), 5);

    TEST_ASSERT_EQUAL(b.find('\r', 2), -1);
    TEST_ASSERT_EQUAL(b.find('\n', 2), 0);
    TEST_ASSERT_EQUAL(b.find('h', 2), 1);
    TEST_ASSERT_EQUAL(b.find('e', 2), -1);
    TEST_ASSERT_EQUAL(b.find('l', 2), -1);
    TEST_ASSERT_EQUAL(b.find('o', 2), -1);

    TEST_ASSERT_EQUAL(b.find('\n', 0), -1);
}

TEST_WITH_SUITE(Buffer, findCharTwoSegments)
{
    Buffer b("\nhe");
    b.copyIn("llo");
    TEST_ASSERT_EQUAL(b.segments(), 2u);

    TEST_ASSERT_EQUAL(b.find('\r'), -1);
    TEST_ASSERT_EQUAL(b.find('\n'), 0);
    TEST_ASSERT_EQUAL(b.find('h'), 1);
    TEST_ASSERT_EQUAL(b.find('e'), 2);
    TEST_ASSERT_EQUAL(b.find('l'), 3);
    TEST_ASSERT_EQUAL(b.find('o'), 5);

    TEST_ASSERT_EQUAL(b.find('\r', 2), -1);
    TEST_ASSERT_EQUAL(b.find('\n', 2), 0);
    TEST_ASSERT_EQUAL(b.find('h', 2), 1);
    TEST_ASSERT_EQUAL(b.find('e', 2), -1);
    TEST_ASSERT_EQUAL(b.find('l', 2), -1);
    TEST_ASSERT_EQUAL(b.find('o', 2), -1);

    TEST_ASSERT_EQUAL(b.find('\r', 4), -1);
    TEST_ASSERT_EQUAL(b.find('\n', 4), 0);
    TEST_ASSERT_EQUAL(b.find('h', 4), 1);
    TEST_ASSERT_EQUAL(b.find('e', 4), 2);
    TEST_ASSERT_EQUAL(b.find('l', 4), 3);
    TEST_ASSERT_EQUAL(b.find('o', 4), -1);

    // Put a write segment on the end
    b.reserve(10);
    TEST_ASSERT_EQUAL(b.segments(), 3u);

    TEST_ASSERT_EQUAL(b.find('\r'), -1);
    TEST_ASSERT_EQUAL(b.find('\n'), 0);
    TEST_ASSERT_EQUAL(b.find('h'), 1);
    TEST_ASSERT_EQUAL(b.find('e'), 2);
    TEST_ASSERT_EQUAL(b.find('l'), 3);
    TEST_ASSERT_EQUAL(b.find('o'), 5);

    TEST_ASSERT_EQUAL(b.find('\r', 2), -1);
    TEST_ASSERT_EQUAL(b.find('\n', 2), 0);
    TEST_ASSERT_EQUAL(b.find('h', 2), 1);
    TEST_ASSERT_EQUAL(b.find('e', 2), -1);
    TEST_ASSERT_EQUAL(b.find('l', 2), -1);
    TEST_ASSERT_EQUAL(b.find('o', 2), -1);

    TEST_ASSERT_EQUAL(b.find('\r', 4), -1);
    TEST_ASSERT_EQUAL(b.find('\n', 4), 0);
    TEST_ASSERT_EQUAL(b.find('h', 4), 1);
    TEST_ASSERT_EQUAL(b.find('e', 4), 2);
    TEST_ASSERT_EQUAL(b.find('l', 4), 3);
    TEST_ASSERT_EQUAL(b.find('o', 4), -1);
}

TEST_WITH_SUITE(Buffer, findCharMixedSegment)
{
    Buffer b("\nhe");
    b.reserve(10);
    b.copyIn("llo");
    TEST_ASSERT_EQUAL(b.segments(), 2u);

    TEST_ASSERT_EQUAL(b.find('\r'), -1);
    TEST_ASSERT_EQUAL(b.find('\n'), 0);
    TEST_ASSERT_EQUAL(b.find('h'), 1);
    TEST_ASSERT_EQUAL(b.find('e'), 2);
    TEST_ASSERT_EQUAL(b.find('l'), 3);
    TEST_ASSERT_EQUAL(b.find('o'), 5);

    TEST_ASSERT_EQUAL(b.find('\r', 2), -1);
    TEST_ASSERT_EQUAL(b.find('\n', 2), 0);
    TEST_ASSERT_EQUAL(b.find('h', 2), 1);
    TEST_ASSERT_EQUAL(b.find('e', 2), -1);
    TEST_ASSERT_EQUAL(b.find('l', 2), -1);
    TEST_ASSERT_EQUAL(b.find('o', 2), -1);

    TEST_ASSERT_EQUAL(b.find('\r', 4), -1);
    TEST_ASSERT_EQUAL(b.find('\n', 4), 0);
    TEST_ASSERT_EQUAL(b.find('h', 4), 1);
    TEST_ASSERT_EQUAL(b.find('e', 4), 2);
    TEST_ASSERT_EQUAL(b.find('l', 4), 3);
    TEST_ASSERT_EQUAL(b.find('o', 4), -1);
}

TEST_WITH_SUITE(Buffer, findStringEmpty)
{
    Buffer b;

    TEST_ASSERT_EQUAL(b.find("h"), -1);
    TEST_ASSERT_EQUAL(b.find("h", 0), -1);
#ifdef DEBUG
    TEST_ASSERT_ASSERTED(b.find(""));
    TEST_ASSERT_ASSERTED(b.find("h", 1));
#endif

    // Put a write segment on the end
    b.reserve(10);
    TEST_ASSERT_EQUAL(b.segments(), 1u);
    TEST_ASSERT_EQUAL(b.find("h"), -1);
    TEST_ASSERT_EQUAL(b.find("h", 0), -1);

#ifdef DEBUG
    TEST_ASSERT_ASSERTED(b.find(""));
    TEST_ASSERT_ASSERTED(b.find("h", 1));
#endif
}

TEST_WITH_SUITE(Buffer, findStringSimple)
{
    Buffer b("helloworld");
    TEST_ASSERT_EQUAL(b.segments(), 1u);

    TEST_ASSERT_EQUAL(b.find("abc"), -1);
    TEST_ASSERT_EQUAL(b.find("helloworld"), 0);
    TEST_ASSERT_EQUAL(b.find("helloworld2"), -1);
    TEST_ASSERT_EQUAL(b.find("elloworld"), 1);
    TEST_ASSERT_EQUAL(b.find("helloworl"), 0);
    TEST_ASSERT_EQUAL(b.find("h"), 0);
    TEST_ASSERT_EQUAL(b.find("l"), 2);
    TEST_ASSERT_EQUAL(b.find("o"), 4);
    TEST_ASSERT_EQUAL(b.find("lo"), 3);
    TEST_ASSERT_EQUAL(b.find("d"), 9);

    TEST_ASSERT_EQUAL(b.find("abc", 5), -1);
    TEST_ASSERT_EQUAL(b.find("helloworld", 5), -1);
    TEST_ASSERT_EQUAL(b.find("hello", 5), 0);
    TEST_ASSERT_EQUAL(b.find("ello", 5), 1);
    TEST_ASSERT_EQUAL(b.find("helloworld2", 5), -1);
    TEST_ASSERT_EQUAL(b.find("elloworld", 5), -1);
    TEST_ASSERT_EQUAL(b.find("hell", 5), 0);
    TEST_ASSERT_EQUAL(b.find("h", 5), 0);
    TEST_ASSERT_EQUAL(b.find("l", 5), 2);
    TEST_ASSERT_EQUAL(b.find("o", 5), 4);
    TEST_ASSERT_EQUAL(b.find("lo", 5), 3);
    TEST_ASSERT_EQUAL(b.find("ow", 5), -1);

    TEST_ASSERT_EQUAL(b.find("h", 0), -1);
}

TEST_WITH_SUITE(Buffer, findStringTwoSegments)
{
    Buffer b("hello");
    b.copyIn("world");
    TEST_ASSERT_EQUAL(b.segments(), 2u);

    TEST_ASSERT_EQUAL(b.find("abc"), -1);
    TEST_ASSERT_EQUAL(b.find("helloworld"), 0);
    TEST_ASSERT_EQUAL(b.find("helloworld2"), -1);
    TEST_ASSERT_EQUAL(b.find("elloworld"), 1);
    TEST_ASSERT_EQUAL(b.find("helloworl"), 0);
    TEST_ASSERT_EQUAL(b.find("h"), 0);
    TEST_ASSERT_EQUAL(b.find("l"), 2);
    TEST_ASSERT_EQUAL(b.find("o"), 4);
    TEST_ASSERT_EQUAL(b.find("lo"), 3);
    TEST_ASSERT_EQUAL(b.find("d"), 9);

    TEST_ASSERT_EQUAL(b.find("abc", 7), -1);
    TEST_ASSERT_EQUAL(b.find("helloworld", 7), -1);
    TEST_ASSERT_EQUAL(b.find("hellowo", 7), 0);
    TEST_ASSERT_EQUAL(b.find("ellowo", 7), 1);
    TEST_ASSERT_EQUAL(b.find("helloworld2", 7), -1);
    TEST_ASSERT_EQUAL(b.find("elloworld", 7), -1);
    TEST_ASSERT_EQUAL(b.find("hellow", 7), 0);
    TEST_ASSERT_EQUAL(b.find("h", 7), 0);
    TEST_ASSERT_EQUAL(b.find("l", 7), 2);
    TEST_ASSERT_EQUAL(b.find("o", 7), 4);
    TEST_ASSERT_EQUAL(b.find("lo", 7), 3);
    TEST_ASSERT_EQUAL(b.find("or", 7), -1);

    TEST_ASSERT_EQUAL(b.find("h", 0), -1);
}

TEST_WITH_SUITE(Buffer, findStringAcrossMultipleSegments)
{
    Buffer b("hello");
    b.copyIn("world");
    b.copyIn("foo");
    TEST_ASSERT_EQUAL(b.segments(), 3u);

    TEST_ASSERT_EQUAL(b.find("lloworldfo"), 2);
}

TEST_WITH_SUITE(Buffer, findStringLongFalsePositive)
{
    Buffer b("100000011");

    TEST_ASSERT_EQUAL(b.find("000011"), 3);
}

TEST_WITH_SUITE(Buffer, findStringFalsePositiveAcrossMultipleSegments)
{
    Buffer b("10");
    b.copyIn("00");
    b.copyIn("00");
    b.copyIn("00");
    b.copyIn("11");
    TEST_ASSERT_EQUAL(b.segments(), 5u);

    TEST_ASSERT_EQUAL(b.find("000011"), 4);
}
