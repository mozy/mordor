// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "mordor/common/streams/memory.h"
#include "mordor/common/version.h"
#include "mordor/test/test.h"

TEST_WITH_SUITE(MemoryStream, basic)
{
    MemoryStream stream;
    Buffer buffer;
    TEST_ASSERT_EQUAL(stream.size(), 0);
    TEST_ASSERT_EQUAL(stream.seek(0, Stream::CURRENT), 0);
    TEST_ASSERT_EQUAL(stream.write("cody", 4), 4u);
    TEST_ASSERT_EQUAL(stream.size(), 4);
    TEST_ASSERT_EQUAL(stream.seek(0, Stream::CURRENT), 4);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT(stream.readBuffer() == "");
    TEST_ASSERT_EQUAL(stream.seek(0, Stream::BEGIN), 0);
    TEST_ASSERT_EQUAL(stream.seek(0, Stream::CURRENT), 0);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT(stream.readBuffer() == "cody");
    TEST_ASSERT_EQUAL(stream.read(buffer, 2), 2u);
    TEST_ASSERT_EQUAL(stream.size(), 4);
    TEST_ASSERT_EQUAL(stream.seek(0, Stream::CURRENT), 2);
    TEST_ASSERT(buffer == "co");
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT(stream.readBuffer() == "dy");
}

TEST_WITH_SUITE(MemoryStream, absoluteSeek)
{
    MemoryStream stream;
    stream.write("cody", 4u);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT(stream.readBuffer() == "");
    TEST_ASSERT_EXCEPTION(stream.seek(-1, Stream::BEGIN), std::invalid_argument);
#ifdef X86
    TEST_ASSERT_EXCEPTION(stream.seek(0x100000000ull, Stream::BEGIN), std::invalid_argument);
#endif
    TEST_ASSERT_EQUAL(stream.seek(0, Stream::BEGIN), 0);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT(stream.readBuffer() == "cody");
    TEST_ASSERT_EQUAL(stream.seek(2, Stream::BEGIN), 2);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT(stream.readBuffer() == "dy");
    TEST_ASSERT_EQUAL(stream.seek(4, Stream::BEGIN), 4);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT(stream.readBuffer() == "");
    TEST_ASSERT_EQUAL(stream.seek(6, Stream::BEGIN), 6);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT(stream.readBuffer() == "");
}

TEST_WITH_SUITE(MemoryStream, forwardSeek)
{
    MemoryStream stream;
    stream.write("cody", 4u);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT(stream.readBuffer() == "");
    TEST_ASSERT_EQUAL(stream.seek(2, Stream::BEGIN), 2);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT(stream.readBuffer() == "dy");
    TEST_ASSERT_EQUAL(stream.seek(1, Stream::CURRENT), 3);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT(stream.readBuffer() == "y");
    TEST_ASSERT_EQUAL(stream.seek(1, Stream::CURRENT), 4);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT(stream.readBuffer() == "");
    TEST_ASSERT_EQUAL(stream.seek(1, Stream::CURRENT), 5);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT(stream.readBuffer() == "");
#ifdef X86
    TEST_ASSERT_EXCEPTION(stream.seek(0x100000000ull, Stream::CURRENT), std::invalid_argument);
#endif
}

TEST_WITH_SUITE(MemoryStream, backwardSeek)
{
    MemoryStream stream;
    stream.write("cody", 4u);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT(stream.readBuffer() == "");
    TEST_ASSERT_EQUAL(stream.seek(2, Stream::BEGIN), 2);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT(stream.readBuffer() == "dy");
    TEST_ASSERT_EQUAL(stream.seek(-1, Stream::CURRENT), 1);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT(stream.readBuffer() == "ody");
    TEST_ASSERT_EXCEPTION(stream.seek(-2, Stream::CURRENT), std::invalid_argument);
}

TEST_WITH_SUITE(MemoryStream, seekFromEnd)
{
    MemoryStream stream;
    stream.write("cody", 4u);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT(stream.readBuffer() == "");
    TEST_ASSERT_EQUAL(stream.seek(0, Stream::END), 4);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT(stream.readBuffer() == "");
    TEST_ASSERT_EQUAL(stream.seek(1, Stream::END), 5);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT(stream.readBuffer() == "");
    TEST_ASSERT_EQUAL(stream.seek(-2, Stream::END), 2);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT(stream.readBuffer() == "dy");
    // Should catch an optimized forward seek
    TEST_ASSERT_EQUAL(stream.seek(-1, Stream::END), 3);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT(stream.readBuffer() == "y");
    TEST_ASSERT_EXCEPTION(stream.seek(-5, Stream::CURRENT), std::invalid_argument);
}

TEST_WITH_SUITE(MemoryStream, truncate)
{
    MemoryStream stream;
    stream.write("cody", 4u);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT(stream.readBuffer() == "");
    TEST_ASSERT_EQUAL(stream.seek(0, Stream::CURRENT), 4);
    stream.truncate(4);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT(stream.readBuffer() == "");
    TEST_ASSERT_EQUAL(stream.seek(0, Stream::CURRENT), 4);
    stream.truncate(3);
    TEST_ASSERT(stream.buffer() == "cod");
    TEST_ASSERT(stream.readBuffer() == "");
    TEST_ASSERT_EQUAL(stream.seek(0, Stream::CURRENT), 4);
    stream.truncate(5);
    TEST_ASSERT(stream.buffer() == std::string("cod\0\0", 5));
    TEST_ASSERT(stream.readBuffer() == std::string("\0", 1));
    TEST_ASSERT_EQUAL(stream.seek(0, Stream::CURRENT), 4);
    stream.truncate(0);
    TEST_ASSERT(stream.buffer() == "");
    TEST_ASSERT(stream.readBuffer() == "");
    TEST_ASSERT_EQUAL(stream.seek(0, Stream::CURRENT), 4);
#ifdef X86
    TEST_ASSERT_EXCEPTION(stream.truncate(0x100000000ull), std::invalid_argument);
#endif
}

TEST_WITH_SUITE(MemoryStream, writeExtension)
{
    MemoryStream stream;
    stream.write("cody", 4u);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT_EQUAL(stream.seek(1, Stream::END), 5);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT_EQUAL(stream.write("cutrer", 6), 6u);
    TEST_ASSERT_EQUAL(stream.size(), 11);
    TEST_ASSERT(stream.buffer() == std::string("cody\0cutrer", 11));
}

TEST_WITH_SUITE(MemoryStream, writep1)
{
    MemoryStream stream;
    stream.write("cody", 4u);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT_EQUAL(stream.seek(1, Stream::BEGIN), 1);
    TEST_ASSERT_EQUAL(stream.write("c", 1), 1u);
    TEST_ASSERT_EQUAL(stream.seek(0, Stream::CURRENT), 2);
    TEST_ASSERT(stream.buffer() == "ccdy");
}

TEST_WITH_SUITE(MemoryStream, writep2)
{
    MemoryStream stream;
    stream.write("cody", 4u);
    TEST_ASSERT(stream.buffer() == "cody");
    TEST_ASSERT_EQUAL(stream.seek(1, Stream::BEGIN), 1);
    TEST_ASSERT_EQUAL(stream.write("cutrer", 6), 6u);
    TEST_ASSERT_EQUAL(stream.seek(0, Stream::CURRENT), 7);
    TEST_ASSERT_EQUAL(stream.size(), 7);
    TEST_ASSERT(stream.buffer() == "ccutrer");
}
