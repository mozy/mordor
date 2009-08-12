// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/streams/memory.h"
#include "mordor/common/streams/buffered.h"
#include "mordor/test/test.h"

TEST_WITH_SUITE(BufferedStream, read)
{
    MemoryStream::ptr baseStream(new MemoryStream(Buffer("01234567890123456789")));
    BufferedStream::ptr bufferedStream(new BufferedStream(baseStream));
    bufferedStream->bufferSize(5);

    TEST_ASSERT_EQUAL(baseStream->size(), 20);
    TEST_ASSERT_EQUAL(bufferedStream->size(), 20);

    Buffer output;

    TEST_ASSERT_EQUAL(bufferedStream->read(output, 0), 0u);
    // Nothing has been read yet
    TEST_ASSERT_EQUAL(output.readAvailable(), 0u);
    TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 0);
    TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 0);

    TEST_ASSERT_EQUAL(bufferedStream->read(output, 2), 2u);
    TEST_ASSERT(output == "01");
    // baseStream should have had a full buffer read from it
    TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 5);
    // But the bufferedStream is hiding it
    TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 2);

    output.clear();
    TEST_ASSERT_EQUAL(bufferedStream->read(output, 2), 2u);
    TEST_ASSERT(output == "23");
    // baseStream stays at the same position, because the read should have been
    // satisfied by the buffer
    TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 5);
    TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 4);
    
    output.clear();
    TEST_ASSERT_EQUAL(bufferedStream->read(output, 7), 7u);
    TEST_ASSERT(output == "4567890");
    // baseStream should have had two buffer-fuls read
    TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 15);
    TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 11);

    output.clear();
    TEST_ASSERT_EQUAL(bufferedStream->read(output, 2), 2u);
    TEST_ASSERT(output == "12");
    // baseStream stays at the same position, because the read should have been
    // satisfied by the buffer
    TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 15);
    TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 13);

    output.clear();
    TEST_ASSERT_EQUAL(bufferedStream->read(output, 10), 7u);
    TEST_ASSERT(output == "3456789");
    TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 20);
    TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 20);

    // Make sure the buffered stream gives us EOF properly
    output.clear();
    TEST_ASSERT_EQUAL(bufferedStream->read(output, 10), 0u);
    TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 20);
    TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 20);
}

TEST_WITH_SUITE(BufferedStream, write)
{
    MemoryStream::ptr baseStream(new MemoryStream());
    BufferedStream::ptr bufferedStream(new BufferedStream(baseStream));
    bufferedStream->bufferSize(5);

    TEST_ASSERT_EQUAL(baseStream->size(), 0);
    TEST_ASSERT_EQUAL(bufferedStream->size(), 0);

    TEST_ASSERT_EQUAL(bufferedStream->write("abc", 3), 3u);
    TEST_ASSERT_EQUAL(baseStream->size(), 0);
    TEST_ASSERT_EQUAL(bufferedStream->size(), 3);

    TEST_ASSERT_EQUAL(bufferedStream->write("de", 2), 2u);
    TEST_ASSERT_EQUAL(baseStream->size(), 5);
    TEST_ASSERT_EQUAL(bufferedStream->size(), 5);

    TEST_ASSERT_EQUAL(bufferedStream->write("fgh", 3), 3u);
    TEST_ASSERT_EQUAL(baseStream->size(), 5);
    TEST_ASSERT_EQUAL(bufferedStream->size(), 8);

    bufferedStream->flush();
    TEST_ASSERT_EQUAL(baseStream->size(), 8);
    TEST_ASSERT_EQUAL(bufferedStream->size(), 8);
    TEST_ASSERT(baseStream->buffer() == "abcdefgh");
}

TEST_WITH_SUITE(BufferedStream, unread)
{
    MemoryStream::ptr baseStream(new MemoryStream(Buffer("01234567890123456789")));
    BufferedStream::ptr bufferedStream(new BufferedStream(baseStream));
    bufferedStream->bufferSize(5);

    TEST_ASSERT_EQUAL(baseStream->size(), 20);
    TEST_ASSERT_EQUAL(bufferedStream->size(), 20);

    Buffer output;
    TEST_ASSERT_EQUAL(bufferedStream->read(output, 6), 6u);
    TEST_ASSERT(output == "012345");
    TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 10);
    TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 6);

    output.consume(3);
    bufferedStream->unread(output);
    TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 10);
    TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 3);

    output.clear();
    TEST_ASSERT_EQUAL(bufferedStream->read(output, 6), 6u);
    TEST_ASSERT(output == "345678");
    TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 10);
    TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 9);

    output.clear();
    TEST_ASSERT_EQUAL(bufferedStream->read(output, 6), 6u);
    TEST_ASSERT(output == "901234");
    TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 15);
    TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 15);
}
