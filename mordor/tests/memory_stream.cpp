// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "mordor/streams/memory.h"
#include "mordor/version.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

MORDOR_UNITTEST(MemoryStream, basic)
{
    MemoryStream stream;
    Buffer buffer;
    MORDOR_TEST_ASSERT_EQUAL(stream.size(), 0);
    MORDOR_TEST_ASSERT_EQUAL(stream.tell(), 0);
    MORDOR_TEST_ASSERT_EQUAL(stream.write("cody", 4), 4u);
    MORDOR_TEST_ASSERT_EQUAL(stream.size(), 4);
    MORDOR_TEST_ASSERT_EQUAL(stream.tell(), 4);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "");
    MORDOR_TEST_ASSERT_EQUAL(stream.seek(0, Stream::BEGIN), 0);
    MORDOR_TEST_ASSERT_EQUAL(stream.tell(), 0);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "cody");
    MORDOR_TEST_ASSERT_EQUAL(stream.read(buffer, 2), 2u);
    MORDOR_TEST_ASSERT_EQUAL(stream.size(), 4);
    MORDOR_TEST_ASSERT_EQUAL(stream.tell(), 2);
    MORDOR_TEST_ASSERT(buffer == "co");
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "dy");
}

MORDOR_UNITTEST(MemoryStream, absoluteSeek)
{
    MemoryStream stream;
    stream.write("cody", 4u);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "");
    MORDOR_TEST_ASSERT_EXCEPTION(stream.seek(-1, Stream::BEGIN), std::invalid_argument);
#ifdef X86
    MORDOR_TEST_ASSERT_EXCEPTION(stream.seek(0x100000000ull, Stream::BEGIN), std::invalid_argument);
#endif
    MORDOR_TEST_ASSERT_EQUAL(stream.seek(0, Stream::BEGIN), 0);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "cody");
    MORDOR_TEST_ASSERT_EQUAL(stream.seek(2, Stream::BEGIN), 2);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "dy");
    MORDOR_TEST_ASSERT_EQUAL(stream.seek(4, Stream::BEGIN), 4);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "");
    MORDOR_TEST_ASSERT_EQUAL(stream.seek(6, Stream::BEGIN), 6);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "");
}

MORDOR_UNITTEST(MemoryStream, forwardSeek)
{
    MemoryStream stream;
    stream.write("cody", 4u);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "");
    MORDOR_TEST_ASSERT_EQUAL(stream.seek(2, Stream::BEGIN), 2);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "dy");
    MORDOR_TEST_ASSERT_EQUAL(stream.seek(1, Stream::CURRENT), 3);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "y");
    MORDOR_TEST_ASSERT_EQUAL(stream.seek(1, Stream::CURRENT), 4);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "");
    MORDOR_TEST_ASSERT_EQUAL(stream.seek(1, Stream::CURRENT), 5);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "");
#ifdef X86
    MORDOR_TEST_ASSERT_EXCEPTION(stream.seek(0x100000000ull, Stream::CURRENT), std::invalid_argument);
#endif
}

MORDOR_UNITTEST(MemoryStream, backwardSeek)
{
    MemoryStream stream;
    stream.write("cody", 4u);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "");
    MORDOR_TEST_ASSERT_EQUAL(stream.seek(2, Stream::BEGIN), 2);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "dy");
    MORDOR_TEST_ASSERT_EQUAL(stream.seek(-1, Stream::CURRENT), 1);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "ody");
    MORDOR_TEST_ASSERT_EXCEPTION(stream.seek(-2, Stream::CURRENT), std::invalid_argument);
}

MORDOR_UNITTEST(MemoryStream, seekFromEnd)
{
    MemoryStream stream;
    stream.write("cody", 4u);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "");
    MORDOR_TEST_ASSERT_EQUAL(stream.seek(0, Stream::END), 4);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "");
    MORDOR_TEST_ASSERT_EQUAL(stream.seek(1, Stream::END), 5);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "");
    MORDOR_TEST_ASSERT_EQUAL(stream.seek(-2, Stream::END), 2);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "dy");
    // Should catch an optimized forward seek
    MORDOR_TEST_ASSERT_EQUAL(stream.seek(-1, Stream::END), 3);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "y");
    MORDOR_TEST_ASSERT_EXCEPTION(stream.seek(-5, Stream::CURRENT), std::invalid_argument);
}

MORDOR_UNITTEST(MemoryStream, truncate)
{
    MemoryStream stream;
    stream.write("cody", 4u);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "");
    MORDOR_TEST_ASSERT_EQUAL(stream.tell(), 4);
    stream.truncate(4);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "");
    MORDOR_TEST_ASSERT_EQUAL(stream.tell(), 4);
    stream.truncate(3);
    MORDOR_TEST_ASSERT(stream.buffer() == "cod");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "");
    MORDOR_TEST_ASSERT_EQUAL(stream.tell(), 4);
    stream.truncate(5);
    MORDOR_TEST_ASSERT(stream.buffer() == std::string("cod\0\0", 5));
    MORDOR_TEST_ASSERT(stream.readBuffer() == std::string("\0", 1));
    MORDOR_TEST_ASSERT_EQUAL(stream.tell(), 4);
    stream.truncate(0);
    MORDOR_TEST_ASSERT(stream.buffer() == "");
    MORDOR_TEST_ASSERT(stream.readBuffer() == "");
    MORDOR_TEST_ASSERT_EQUAL(stream.tell(), 4);
#ifdef X86
    MORDOR_TEST_ASSERT_EXCEPTION(stream.truncate(0x100000000ull), std::invalid_argument);
#endif
}

MORDOR_UNITTEST(MemoryStream, writeExtension)
{
    MemoryStream stream;
    stream.write("cody", 4u);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT_EQUAL(stream.seek(1, Stream::END), 5);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT_EQUAL(stream.write("cutrer", 6), 6u);
    MORDOR_TEST_ASSERT_EQUAL(stream.size(), 11);
    MORDOR_TEST_ASSERT(stream.buffer() == std::string("cody\0cutrer", 11));
}

MORDOR_UNITTEST(MemoryStream, writep1)
{
    MemoryStream stream;
    stream.write("cody", 4u);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT_EQUAL(stream.seek(1, Stream::BEGIN), 1);
    MORDOR_TEST_ASSERT_EQUAL(stream.write("c", 1), 1u);
    MORDOR_TEST_ASSERT_EQUAL(stream.tell(), 2);
    MORDOR_TEST_ASSERT(stream.buffer() == "ccdy");
}

MORDOR_UNITTEST(MemoryStream, writep2)
{
    MemoryStream stream;
    stream.write("cody", 4u);
    MORDOR_TEST_ASSERT(stream.buffer() == "cody");
    MORDOR_TEST_ASSERT_EQUAL(stream.seek(1, Stream::BEGIN), 1);
    MORDOR_TEST_ASSERT_EQUAL(stream.write("cutrer", 6), 6u);
    MORDOR_TEST_ASSERT_EQUAL(stream.tell(), 7);
    MORDOR_TEST_ASSERT_EQUAL(stream.size(), 7);
    MORDOR_TEST_ASSERT(stream.buffer() == "ccutrer");
}
