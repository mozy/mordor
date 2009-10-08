// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include <boost/bind.hpp>

#include "mordor/common/streams/buffer.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

MORDOR_UNITTEST(Buffer, copyInString)
{
    Buffer b;
    b.copyIn("hello");
    MORDOR_TEST_ASSERT_EQUAL(b.readAvailable(), 5u);
    MORDOR_TEST_ASSERT_EQUAL(b.writeAvailable(), 0u);
    MORDOR_TEST_ASSERT_EQUAL(b.segments(), 1u);
    MORDOR_TEST_ASSERT(b == "hello");
}

MORDOR_UNITTEST(Buffer, copyInOtherBuffer)
{
    Buffer b1, b2("hello");
    b1.copyIn(b2);
    MORDOR_TEST_ASSERT_EQUAL(b1.readAvailable(), 5u);
    MORDOR_TEST_ASSERT_EQUAL(b1.writeAvailable(), 0u);
    MORDOR_TEST_ASSERT_EQUAL(b1.segments(), 1u);
    MORDOR_TEST_ASSERT(b1 == "hello");
}

MORDOR_UNITTEST(Buffer, copyInPartial)
{
    Buffer b1, b2("hello");
    b1.copyIn(b2, 3);
    MORDOR_TEST_ASSERT_EQUAL(b1.readAvailable(), 3u);
    MORDOR_TEST_ASSERT_EQUAL(b1.writeAvailable(), 0u);
    MORDOR_TEST_ASSERT_EQUAL(b1.segments(), 1u);
    MORDOR_TEST_ASSERT(b1 == "hel");
}

MORDOR_UNITTEST(Buffer, copyInStringToReserved)
{
    Buffer b;
    b.reserve(5);
    b.copyIn("hello");
    MORDOR_TEST_ASSERT_EQUAL(b.readAvailable(), 5u);
    MORDOR_TEST_ASSERT_EQUAL(b.segments(), 1u);
    MORDOR_TEST_ASSERT(b == "hello");
}

MORDOR_UNITTEST(Buffer, copyInStringAfterAnotherSegment)
{
    Buffer b("hello");
    b.copyIn("world");
    MORDOR_TEST_ASSERT_EQUAL(b.readAvailable(), 10u);
    MORDOR_TEST_ASSERT_EQUAL(b.writeAvailable(), 0u);
    MORDOR_TEST_ASSERT_EQUAL(b.segments(), 2u);
    MORDOR_TEST_ASSERT(b == "helloworld");
}

MORDOR_UNITTEST(Buffer, copyInStringToReservedAfterAnotherSegment)
{
    Buffer b("hello");
    b.reserve(5);
    b.copyIn("world");
    MORDOR_TEST_ASSERT_EQUAL(b.readAvailable(), 10u);
    MORDOR_TEST_ASSERT_EQUAL(b.segments(), 2u);
    MORDOR_TEST_ASSERT(b == "helloworld");
}

MORDOR_UNITTEST(Buffer, copyInStringToSplitSegment)
{
    Buffer b;
    b.reserve(10);
    b.copyIn("hello");
    MORDOR_TEST_ASSERT_EQUAL(b.readAvailable(), 5u);
    MORDOR_TEST_ASSERT_GREATER_THAN_OR_EQUAL(b.writeAvailable(), 5u);
    MORDOR_TEST_ASSERT_EQUAL(b.segments(), 1u);
    b.copyIn("world");
    MORDOR_TEST_ASSERT_EQUAL(b.readAvailable(), 10u);
    MORDOR_TEST_ASSERT_EQUAL(b.segments(), 1u);
    MORDOR_TEST_ASSERT(b == "helloworld");
}

MORDOR_UNITTEST(Buffer, copyInWithReserve)
{
    Buffer b1, b2("hello");
    b1.reserve(10);
    MORDOR_TEST_ASSERT_GREATER_THAN_OR_EQUAL(b1.writeAvailable(), 10u);
    MORDOR_TEST_ASSERT_EQUAL(b1.segments(), 1u);
    size_t writeAvailable = b1.writeAvailable();
    b1.copyIn(b2);
    MORDOR_TEST_ASSERT_EQUAL(b1.readAvailable(), 5u);
    // Shouldn't have eaten any
    MORDOR_TEST_ASSERT_EQUAL(b1.writeAvailable(), writeAvailable);
    MORDOR_TEST_ASSERT_EQUAL(b1.segments(), 2u);
    MORDOR_TEST_ASSERT(b1 == "hello");
}

MORDOR_UNITTEST(Buffer, copyInToSplitSegment)
{
    Buffer b1, b2("world");
    b1.reserve(10);
    b1.copyIn("hello");
    MORDOR_TEST_ASSERT_EQUAL(b1.readAvailable(), 5u);
    MORDOR_TEST_ASSERT_GREATER_THAN_OR_EQUAL(b1.writeAvailable(), 5u);
    MORDOR_TEST_ASSERT_EQUAL(b1.segments(), 1u);
    size_t writeAvailable = b1.writeAvailable();
    b1.copyIn(b2, 5);
    MORDOR_TEST_ASSERT_EQUAL(b1.readAvailable(), 10u);
    // Shouldn't have eaten any
    MORDOR_TEST_ASSERT_EQUAL(b1.writeAvailable(), writeAvailable);
    MORDOR_TEST_ASSERT_EQUAL(b1.segments(), 3u);
    MORDOR_TEST_ASSERT(b1 == "helloworld");
}

#ifdef DEBUG
MORDOR_UNITTEST(Buffer, copyInMoreThanThereIs)
{
    Buffer b1, b2;
    MORDOR_TEST_ASSERT_ASSERTED(b1.copyIn(b2, 1));
    b2.copyIn("hello");
    MORDOR_TEST_ASSERT_ASSERTED(b1.copyIn(b2, 6));
}
#endif

MORDOR_UNITTEST(Buffer, copyInMerge)
{
    Buffer b1, b2("hello");
    b1.copyIn(b2, 2);
    b2.consume(2);
    b1.copyIn(b2, 3);
    MORDOR_TEST_ASSERT_EQUAL(b1.readAvailable(), 5u);
    MORDOR_TEST_ASSERT_EQUAL(b1.segments(), 1u);
    MORDOR_TEST_ASSERT(b1 == "hello");
}

MORDOR_UNITTEST(Buffer, copyInMergePlus)
{
    Buffer b1, b2("hello");
    b2.copyIn("world");
    MORDOR_TEST_ASSERT_EQUAL(b2.segments(), 2u);
    b1.copyIn(b2, 2);
    b2.consume(2);
    b1.copyIn(b2, 4);
    MORDOR_TEST_ASSERT_EQUAL(b1.readAvailable(), 6u);
    MORDOR_TEST_ASSERT_EQUAL(b1.segments(), 2u);
    MORDOR_TEST_ASSERT(b1 == "hellow");
}

MORDOR_UNITTEST(Buffer, noSplitOnTruncate)
{
    Buffer b1;
    b1.reserve(10);
    b1.copyIn("hello");
    b1.truncate(5);
    MORDOR_TEST_ASSERT_GREATER_THAN_OR_EQUAL(b1.writeAvailable(), 5u);
    b1.copyIn("world");
    MORDOR_TEST_ASSERT_EQUAL(b1.segments(), 1u);
    MORDOR_TEST_ASSERT(b1 == "helloworld");
}

MORDOR_UNITTEST(Buffer, copyConstructor)
{
    Buffer buf1;
    buf1.copyIn("hello");
    Buffer buf2(buf1);
    MORDOR_TEST_ASSERT(buf1 == "hello");
    MORDOR_TEST_ASSERT(buf2 == "hello");
    MORDOR_TEST_ASSERT_EQUAL(buf1.writeAvailable(), 0u);
    MORDOR_TEST_ASSERT_EQUAL(buf2.writeAvailable(), 0u);
}

MORDOR_UNITTEST(Buffer, copyConstructorImmutability)
{
    Buffer buf1;
    buf1.reserve(10);
    Buffer buf2(buf1);
    buf1.copyIn("hello");
    buf2.copyIn("tommy");
    MORDOR_TEST_ASSERT_EQUAL(buf1.readAvailable(), 5u);
    MORDOR_TEST_ASSERT_GREATER_THAN_OR_EQUAL(buf1.writeAvailable(), 5u);
    MORDOR_TEST_ASSERT_EQUAL(buf2.readAvailable(), 5u);
    MORDOR_TEST_ASSERT_EQUAL(buf2.writeAvailable(), 0u);
    MORDOR_TEST_ASSERT(buf1 == "hello");
    MORDOR_TEST_ASSERT(buf2 == "tommy");
}

MORDOR_UNITTEST(Buffer, truncate)
{
    Buffer buf("hello");
    buf.truncate(3);
    MORDOR_TEST_ASSERT(buf == "hel");
}

MORDOR_UNITTEST(Buffer, truncateMultipleSegments1)
{
    Buffer buf("hello");
    buf.copyIn("world");
    buf.truncate(3);
    MORDOR_TEST_ASSERT(buf == "hel");
}

MORDOR_UNITTEST(Buffer, truncateMultipleSegments2)
{
    Buffer buf("hello");
    buf.copyIn("world");
    buf.truncate(8);
    MORDOR_TEST_ASSERT(buf == "hellowor");
}

MORDOR_UNITTEST(Buffer, truncateBeforeWriteSegments)
{
    Buffer buf("hello");
    buf.reserve(5);
    buf.truncate(3);
    MORDOR_TEST_ASSERT(buf == "hel");
    MORDOR_TEST_ASSERT_GREATER_THAN_OR_EQUAL(buf.writeAvailable(), 5u);
}

MORDOR_UNITTEST(Buffer, truncateAtWriteSegments)
{
    Buffer buf("hello");
    buf.reserve(10);
    buf.copyIn("world");
    buf.truncate(8);
    MORDOR_TEST_ASSERT(buf == "hellowor");
    MORDOR_TEST_ASSERT_GREATER_THAN_OR_EQUAL(buf.writeAvailable(), 10u);
}

MORDOR_UNITTEST(Buffer, compareEmpty)
{
    Buffer buf1, buf2;
    MORDOR_TEST_ASSERT(buf1 == buf2);
    MORDOR_TEST_ASSERT(!(buf1 != buf2));
}

MORDOR_UNITTEST(Buffer, compareSimpleInequality)
{
    Buffer buf1, buf2("h");
    MORDOR_TEST_ASSERT(buf1 != buf2);
    MORDOR_TEST_ASSERT(!(buf1 == buf2));
}

MORDOR_UNITTEST(Buffer, compareIdentical)
{
    Buffer buf1("hello"), buf2("hello");
    MORDOR_TEST_ASSERT(buf1 == buf2);
    MORDOR_TEST_ASSERT(!(buf1 != buf2));
}

MORDOR_UNITTEST(Buffer, compareLotsOfSegmentsOnTheLeft)
{
    Buffer buf1, buf2("hello world!");
    buf1.copyIn("he");
    buf1.copyIn("l");
    buf1.copyIn("l");
    buf1.copyIn("o wor");
    buf1.copyIn("ld!");
    MORDOR_TEST_ASSERT(buf1 == buf2);
    MORDOR_TEST_ASSERT(!(buf1 != buf2));
}

MORDOR_UNITTEST(Buffer, compareLotOfSegmentsOnTheRight)
{
    Buffer buf1("hello world!"), buf2;
    buf2.copyIn("he");
    buf2.copyIn("l");
    buf2.copyIn("l");
    buf2.copyIn("o wor");
    buf2.copyIn("ld!");
    MORDOR_TEST_ASSERT(buf1 == buf2);
    MORDOR_TEST_ASSERT(!(buf1 != buf2));
}

MORDOR_UNITTEST(Buffer, compareLotsOfSegments)
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
    MORDOR_TEST_ASSERT(buf1 == buf2);
    MORDOR_TEST_ASSERT(!(buf1 != buf2));
}

MORDOR_UNITTEST(Buffer, compareLotsOfMismatchedSegments)
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
    MORDOR_TEST_ASSERT(buf1 == buf2);
    MORDOR_TEST_ASSERT(!(buf1 != buf2));
}

MORDOR_UNITTEST(Buffer, compareLotsOfSegmentsOnTheLeftInequality)
{
    Buffer buf1, buf2("hello world!");
    buf1.copyIn("he");
    buf1.copyIn("l");
    buf1.copyIn("l");
    buf1.copyIn("o wor");
    buf1.copyIn("ld! ");
    MORDOR_TEST_ASSERT(buf1 != buf2);
    MORDOR_TEST_ASSERT(!(buf1 == buf2));
}

MORDOR_UNITTEST(Buffer, compareLotOfSegmentsOnTheRightInequality)
{
    Buffer buf1("hello world!"), buf2;
    buf2.copyIn("he");
    buf2.copyIn("l");
    buf2.copyIn("l");
    buf2.copyIn("o wor");
    buf2.copyIn("ld! ");
    MORDOR_TEST_ASSERT(buf1 != buf2);
    MORDOR_TEST_ASSERT(!(buf1 == buf2));
}

MORDOR_UNITTEST(Buffer, compareLotsOfSegmentsInequality)
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
    MORDOR_TEST_ASSERT(buf1 != buf2);
    MORDOR_TEST_ASSERT(!(buf1 == buf2));
}

MORDOR_UNITTEST(Buffer, compareLotsOfMismatchedSegmentsInequality)
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
    MORDOR_TEST_ASSERT(buf1 != buf2);
    MORDOR_TEST_ASSERT(!(buf1 == buf2));
}

MORDOR_UNITTEST(Buffer, reserveWithReadAvailable)
{
    Buffer buf1("hello");
    buf1.reserve(10);
    MORDOR_TEST_ASSERT_EQUAL(buf1.readAvailable(), 5u);
    MORDOR_TEST_ASSERT_GREATER_THAN_OR_EQUAL(buf1.writeAvailable(), 10u);
}

MORDOR_UNITTEST(Buffer, reserveWithWriteAvailable)
{
    Buffer buf1;
    buf1.reserve(5);
    // Internal knowledge that reserve doubles the reservation
    MORDOR_TEST_ASSERT_EQUAL(buf1.writeAvailable(), 10u);
    buf1.reserve(11);
    MORDOR_TEST_ASSERT_EQUAL(buf1.writeAvailable(), 22u);
}

MORDOR_UNITTEST(Buffer, reserveWithReadAndWriteAvailable)
{
    Buffer buf1("hello");
    buf1.reserve(5);
    // Internal knowledge that reserve doubles the reservation
    MORDOR_TEST_ASSERT_EQUAL(buf1.readAvailable(), 5u);
    MORDOR_TEST_ASSERT_EQUAL(buf1.writeAvailable(), 10u);
    buf1.reserve(11);
    MORDOR_TEST_ASSERT_EQUAL(buf1.readAvailable(), 5u);
    MORDOR_TEST_ASSERT_EQUAL(buf1.writeAvailable(), 22u);
}

static void
visitor1(const void *b, size_t len)
{
    MORDOR_NOTREACHED();
}

MORDOR_UNITTEST(Buffer, visitEmpty)
{
    Buffer b;
    b.visit(&visitor1);
}

MORDOR_UNITTEST(Buffer, visitNonEmpty0)
{
    Buffer b("hello");
    b.visit(&visitor1, 0);
}

static void
visitor2(const void *b, size_t len, int &sequence)
{
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
    MORDOR_TEST_ASSERT_EQUAL(len, 5u);
    MORDOR_TEST_ASSERT(memcmp(b, "hello", 5) == 0);
}

MORDOR_UNITTEST(Buffer, visitSingleSegment)
{
    Buffer b("hello");
    int sequence = 0;
    b.visit(boost::bind(&visitor2, _1, _2, boost::ref(sequence)));
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 2);
}

static void
visitor3(const void *b, size_t len, int &sequence)
{
    switch (len) {
        case 1:
            MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
            MORDOR_TEST_ASSERT(memcmp(b, "a", 1) == 0);
            break;
        case 2:
            MORDOR_TEST_ASSERT_EQUAL(++sequence, 2);
            MORDOR_TEST_ASSERT(memcmp(b, "bc", 2) == 0);
            break;
        default:
            MORDOR_NOTREACHED();
    }
}

MORDOR_UNITTEST(Buffer, visitMultipleSegments)
{
    Buffer b;
    int sequence = 0;
    b.copyIn("a");
    b.copyIn("bc");
    b.visit(boost::bind(&visitor3, _1, _2, boost::ref(sequence)));
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 3);
}

MORDOR_UNITTEST(Buffer, visitMultipleSegmentsPartial)
{
    Buffer b;
    int sequence = 0;
    b.copyIn("a");
    b.copyIn("bcd");
    b.visit(boost::bind(&visitor3, _1, _2, boost::ref(sequence)), 3);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 3);
}

MORDOR_UNITTEST(Buffer, visitWithWriteSegment)
{
    Buffer b("hello");
    b.reserve(5);
    int sequence = 0;
    b.visit(boost::bind(&visitor2, _1, _2, boost::ref(sequence)));
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 2);
}

MORDOR_UNITTEST(Buffer, visitWithMixedSegment)
{
    Buffer b;
    b.reserve(10);
    b.copyIn("hello");
    int sequence = 0;
    b.visit(boost::bind(&visitor2, _1, _2, boost::ref(sequence)));
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 2);
}

#ifdef DEBUG
MORDOR_UNITTEST(Buffer, visitMoreThanThereIs)
{
    Buffer b;
    MORDOR_TEST_ASSERT_ASSERTED(b.visit(&visitor1, 1));
}
#endif

MORDOR_UNITTEST(Buffer, findCharEmpty)
{
    Buffer b;
    MORDOR_TEST_ASSERT_EQUAL(b.segments(), 0u);
    MORDOR_TEST_ASSERT_EQUAL(b.find('\n'), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('\n', 0), -1);

#ifdef DEBUG
    MORDOR_TEST_ASSERT_ASSERTED(b.find('\n', 1));
#endif

    // Put a write segment on the end
    b.reserve(10);
    MORDOR_TEST_ASSERT_EQUAL(b.segments(), 1u);
    MORDOR_TEST_ASSERT_EQUAL(b.find('\n'), -1);

#ifdef DEBUG
    MORDOR_TEST_ASSERT_ASSERTED(b.find('\n', 1));
#endif
}

MORDOR_UNITTEST(Buffer, findCharSimple)
{
    Buffer b("\nhello");
    MORDOR_TEST_ASSERT_EQUAL(b.segments(), 1u);

    MORDOR_TEST_ASSERT_EQUAL(b.find('\r'), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('\n'), 0);
    MORDOR_TEST_ASSERT_EQUAL(b.find('h'), 1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('e'), 2);
    MORDOR_TEST_ASSERT_EQUAL(b.find('l'), 3);
    MORDOR_TEST_ASSERT_EQUAL(b.find('o'), 5);

    MORDOR_TEST_ASSERT_EQUAL(b.find('\r', 2), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('\n', 2), 0);
    MORDOR_TEST_ASSERT_EQUAL(b.find('h', 2), 1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('e', 2), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('l', 2), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('o', 2), -1);

    MORDOR_TEST_ASSERT_EQUAL(b.find('\n', 0), -1);
}

MORDOR_UNITTEST(Buffer, findCharTwoSegments)
{
    Buffer b("\nhe");
    b.copyIn("llo");
    MORDOR_TEST_ASSERT_EQUAL(b.segments(), 2u);

    MORDOR_TEST_ASSERT_EQUAL(b.find('\r'), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('\n'), 0);
    MORDOR_TEST_ASSERT_EQUAL(b.find('h'), 1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('e'), 2);
    MORDOR_TEST_ASSERT_EQUAL(b.find('l'), 3);
    MORDOR_TEST_ASSERT_EQUAL(b.find('o'), 5);

    MORDOR_TEST_ASSERT_EQUAL(b.find('\r', 2), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('\n', 2), 0);
    MORDOR_TEST_ASSERT_EQUAL(b.find('h', 2), 1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('e', 2), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('l', 2), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('o', 2), -1);

    MORDOR_TEST_ASSERT_EQUAL(b.find('\r', 4), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('\n', 4), 0);
    MORDOR_TEST_ASSERT_EQUAL(b.find('h', 4), 1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('e', 4), 2);
    MORDOR_TEST_ASSERT_EQUAL(b.find('l', 4), 3);
    MORDOR_TEST_ASSERT_EQUAL(b.find('o', 4), -1);

    // Put a write segment on the end
    b.reserve(10);
    MORDOR_TEST_ASSERT_EQUAL(b.segments(), 3u);

    MORDOR_TEST_ASSERT_EQUAL(b.find('\r'), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('\n'), 0);
    MORDOR_TEST_ASSERT_EQUAL(b.find('h'), 1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('e'), 2);
    MORDOR_TEST_ASSERT_EQUAL(b.find('l'), 3);
    MORDOR_TEST_ASSERT_EQUAL(b.find('o'), 5);

    MORDOR_TEST_ASSERT_EQUAL(b.find('\r', 2), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('\n', 2), 0);
    MORDOR_TEST_ASSERT_EQUAL(b.find('h', 2), 1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('e', 2), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('l', 2), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('o', 2), -1);

    MORDOR_TEST_ASSERT_EQUAL(b.find('\r', 4), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('\n', 4), 0);
    MORDOR_TEST_ASSERT_EQUAL(b.find('h', 4), 1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('e', 4), 2);
    MORDOR_TEST_ASSERT_EQUAL(b.find('l', 4), 3);
    MORDOR_TEST_ASSERT_EQUAL(b.find('o', 4), -1);
}

MORDOR_UNITTEST(Buffer, findCharMixedSegment)
{
    Buffer b("\nhe");
    b.reserve(10);
    b.copyIn("llo");
    MORDOR_TEST_ASSERT_EQUAL(b.segments(), 2u);

    MORDOR_TEST_ASSERT_EQUAL(b.find('\r'), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('\n'), 0);
    MORDOR_TEST_ASSERT_EQUAL(b.find('h'), 1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('e'), 2);
    MORDOR_TEST_ASSERT_EQUAL(b.find('l'), 3);
    MORDOR_TEST_ASSERT_EQUAL(b.find('o'), 5);

    MORDOR_TEST_ASSERT_EQUAL(b.find('\r', 2), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('\n', 2), 0);
    MORDOR_TEST_ASSERT_EQUAL(b.find('h', 2), 1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('e', 2), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('l', 2), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('o', 2), -1);

    MORDOR_TEST_ASSERT_EQUAL(b.find('\r', 4), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('\n', 4), 0);
    MORDOR_TEST_ASSERT_EQUAL(b.find('h', 4), 1);
    MORDOR_TEST_ASSERT_EQUAL(b.find('e', 4), 2);
    MORDOR_TEST_ASSERT_EQUAL(b.find('l', 4), 3);
    MORDOR_TEST_ASSERT_EQUAL(b.find('o', 4), -1);
}

MORDOR_UNITTEST(Buffer, findStringEmpty)
{
    Buffer b;

    MORDOR_TEST_ASSERT_EQUAL(b.find("h"), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find("h", 0), -1);
#ifdef DEBUG
    MORDOR_TEST_ASSERT_ASSERTED(b.find(""));
    MORDOR_TEST_ASSERT_ASSERTED(b.find("h", 1));
#endif

    // Put a write segment on the end
    b.reserve(10);
    MORDOR_TEST_ASSERT_EQUAL(b.segments(), 1u);
    MORDOR_TEST_ASSERT_EQUAL(b.find("h"), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find("h", 0), -1);

#ifdef DEBUG
    MORDOR_TEST_ASSERT_ASSERTED(b.find(""));
    MORDOR_TEST_ASSERT_ASSERTED(b.find("h", 1));
#endif
}

MORDOR_UNITTEST(Buffer, findStringSimple)
{
    Buffer b("helloworld");
    MORDOR_TEST_ASSERT_EQUAL(b.segments(), 1u);

    MORDOR_TEST_ASSERT_EQUAL(b.find("abc"), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find("helloworld"), 0);
    MORDOR_TEST_ASSERT_EQUAL(b.find("helloworld2"), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find("elloworld"), 1);
    MORDOR_TEST_ASSERT_EQUAL(b.find("helloworl"), 0);
    MORDOR_TEST_ASSERT_EQUAL(b.find("h"), 0);
    MORDOR_TEST_ASSERT_EQUAL(b.find("l"), 2);
    MORDOR_TEST_ASSERT_EQUAL(b.find("o"), 4);
    MORDOR_TEST_ASSERT_EQUAL(b.find("lo"), 3);
    MORDOR_TEST_ASSERT_EQUAL(b.find("d"), 9);

    MORDOR_TEST_ASSERT_EQUAL(b.find("abc", 5), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find("helloworld", 5), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find("hello", 5), 0);
    MORDOR_TEST_ASSERT_EQUAL(b.find("ello", 5), 1);
    MORDOR_TEST_ASSERT_EQUAL(b.find("helloworld2", 5), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find("elloworld", 5), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find("hell", 5), 0);
    MORDOR_TEST_ASSERT_EQUAL(b.find("h", 5), 0);
    MORDOR_TEST_ASSERT_EQUAL(b.find("l", 5), 2);
    MORDOR_TEST_ASSERT_EQUAL(b.find("o", 5), 4);
    MORDOR_TEST_ASSERT_EQUAL(b.find("lo", 5), 3);
    MORDOR_TEST_ASSERT_EQUAL(b.find("ow", 5), -1);

    MORDOR_TEST_ASSERT_EQUAL(b.find("h", 0), -1);
}

MORDOR_UNITTEST(Buffer, findStringTwoSegments)
{
    Buffer b("hello");
    b.copyIn("world");
    MORDOR_TEST_ASSERT_EQUAL(b.segments(), 2u);

    MORDOR_TEST_ASSERT_EQUAL(b.find("abc"), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find("helloworld"), 0);
    MORDOR_TEST_ASSERT_EQUAL(b.find("helloworld2"), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find("elloworld"), 1);
    MORDOR_TEST_ASSERT_EQUAL(b.find("helloworl"), 0);
    MORDOR_TEST_ASSERT_EQUAL(b.find("h"), 0);
    MORDOR_TEST_ASSERT_EQUAL(b.find("l"), 2);
    MORDOR_TEST_ASSERT_EQUAL(b.find("o"), 4);
    MORDOR_TEST_ASSERT_EQUAL(b.find("lo"), 3);
    MORDOR_TEST_ASSERT_EQUAL(b.find("d"), 9);

    MORDOR_TEST_ASSERT_EQUAL(b.find("abc", 7), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find("helloworld", 7), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find("hellowo", 7), 0);
    MORDOR_TEST_ASSERT_EQUAL(b.find("ellowo", 7), 1);
    MORDOR_TEST_ASSERT_EQUAL(b.find("helloworld2", 7), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find("elloworld", 7), -1);
    MORDOR_TEST_ASSERT_EQUAL(b.find("hellow", 7), 0);
    MORDOR_TEST_ASSERT_EQUAL(b.find("h", 7), 0);
    MORDOR_TEST_ASSERT_EQUAL(b.find("l", 7), 2);
    MORDOR_TEST_ASSERT_EQUAL(b.find("o", 7), 4);
    MORDOR_TEST_ASSERT_EQUAL(b.find("lo", 7), 3);
    MORDOR_TEST_ASSERT_EQUAL(b.find("or", 7), -1);

    MORDOR_TEST_ASSERT_EQUAL(b.find("h", 0), -1);
}

MORDOR_UNITTEST(Buffer, findStringAcrossMultipleSegments)
{
    Buffer b("hello");
    b.copyIn("world");
    b.copyIn("foo");
    MORDOR_TEST_ASSERT_EQUAL(b.segments(), 3u);

    MORDOR_TEST_ASSERT_EQUAL(b.find("lloworldfo"), 2);
}

MORDOR_UNITTEST(Buffer, findStringLongFalsePositive)
{
    Buffer b("100000011");

    MORDOR_TEST_ASSERT_EQUAL(b.find("000011"), 3);
}

MORDOR_UNITTEST(Buffer, findStringFalsePositiveAcrossMultipleSegments)
{
    Buffer b("10");
    b.copyIn("00");
    b.copyIn("00");
    b.copyIn("00");
    b.copyIn("11");
    MORDOR_TEST_ASSERT_EQUAL(b.segments(), 5u);

    MORDOR_TEST_ASSERT_EQUAL(b.find("000011"), 4);
}
