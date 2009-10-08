// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "mordor/common/streams/memory.h"
#include "mordor/common/streams/buffered.h"
#include "mordor/common/streams/test.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

MORDOR_UNITTEST(BufferedStream, read)
{
    MemoryStream::ptr baseStream(new MemoryStream(Buffer("01234567890123456789")));
    BufferedStream::ptr bufferedStream(new BufferedStream(baseStream));
    bufferedStream->bufferSize(5);

    MORDOR_TEST_ASSERT_EQUAL(baseStream->size(), 20);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->size(), 20);

    Buffer output;

    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 0), 0u);
    // Nothing has been read yet
    MORDOR_TEST_ASSERT_EQUAL(output.readAvailable(), 0u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 0);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 0);

    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 2), 2u);
    MORDOR_TEST_ASSERT(output == "01");
    // baseStream should have had a full buffer read from it
    MORDOR_TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 5);
    // But the bufferedStream is hiding it
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 2);

    output.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 2), 2u);
    MORDOR_TEST_ASSERT(output == "23");
    // baseStream stays at the same position, because the read should have been
    // satisfied by the buffer
    MORDOR_TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 5);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 4);
    
    output.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 7), 7u);
    MORDOR_TEST_ASSERT(output == "4567890");
    // baseStream should have had two buffer-fuls read
    MORDOR_TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 15);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 11);

    output.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 2), 2u);
    MORDOR_TEST_ASSERT(output == "12");
    // baseStream stays at the same position, because the read should have been
    // satisfied by the buffer
    MORDOR_TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 15);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 13);

    output.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 10), 7u);
    MORDOR_TEST_ASSERT(output == "3456789");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 20);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 20);

    // Make sure the buffered stream gives us EOF properly
    output.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 10), 0u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 20);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 20);
}

MORDOR_UNITTEST(BufferedStream, partialReadGuarantee)
{
    MemoryStream::ptr baseStream(new MemoryStream(Buffer("01234567890123456789")));
    TestStream::ptr testStream(new TestStream(baseStream));
    BufferedStream::ptr bufferedStream(new BufferedStream(testStream));
    testStream->maxReadSize(2);

    Buffer output;

    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 5), 5u);
    MORDOR_TEST_ASSERT(output == "01234");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 6);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 5);

    bufferedStream->allowPartialReads(true);
    output.clear();
    // Use up the rest of what's buffered
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 5), 1u);
    MORDOR_TEST_ASSERT(output == "5");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 6);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 6);

    output.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 5), 2u);
    MORDOR_TEST_ASSERT(output == "67");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 8);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 8);
}

MORDOR_UNITTEST(BufferedStream, write)
{
    MemoryStream::ptr baseStream(new MemoryStream());
    BufferedStream::ptr bufferedStream(new BufferedStream(baseStream));
    bufferedStream->bufferSize(5);

    MORDOR_TEST_ASSERT_EQUAL(baseStream->size(), 0);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->size(), 0);

    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->write("abc", 3), 3u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->size(), 0);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->size(), 3);

    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->write("de", 2), 2u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->size(), 5);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->size(), 5);

    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->write("fgh", 3), 3u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->size(), 5);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->size(), 8);

    bufferedStream->flush();
    MORDOR_TEST_ASSERT_EQUAL(baseStream->size(), 8);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->size(), 8);
    MORDOR_TEST_ASSERT(baseStream->buffer() == "abcdefgh");
}

MORDOR_UNITTEST(BufferedStream, partialWriteGuarantee)
{
    MemoryStream::ptr baseStream(new MemoryStream());
    TestStream::ptr testStream(new TestStream(baseStream));
    BufferedStream::ptr bufferedStream(new BufferedStream(testStream));
    bufferedStream->bufferSize(5);
    testStream->maxWriteSize(2);

    MORDOR_TEST_ASSERT_EQUAL(baseStream->size(), 0);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->size(), 0);

    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->write("hello", 5), 5u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->size(), 2);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->size(), 5);

    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->write("abcdefghijklmnopqrstuvwxyz", 26), 26u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->size(), 28);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->size(), 31);

    bufferedStream->bufferSize(20);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->write("abcdefghijklmnopqrstuvwxyz", 26), 26u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->size(), 38);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->size(), 57);

    bufferedStream->flush();
    MORDOR_TEST_ASSERT_EQUAL(baseStream->size(), 57);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->size(), 57);
    MORDOR_TEST_ASSERT(baseStream->buffer() == "helloabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz");
}

MORDOR_UNITTEST(BufferedStream, unread)
{
    MemoryStream::ptr baseStream(new MemoryStream(Buffer("01234567890123456789")));
    BufferedStream::ptr bufferedStream(new BufferedStream(baseStream));
    bufferedStream->bufferSize(5);

    MORDOR_TEST_ASSERT_EQUAL(baseStream->size(), 20);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->size(), 20);

    Buffer output;
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 6), 6u);
    MORDOR_TEST_ASSERT(output == "012345");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 10);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 6);

    output.consume(3);
    bufferedStream->unread(output);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 10);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 3);

    output.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 6), 6u);
    MORDOR_TEST_ASSERT(output == "345678");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 10);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 9);

    output.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 6), 6u);
    MORDOR_TEST_ASSERT(output == "901234");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 15);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 15);
}

MORDOR_UNITTEST(BufferedStream, find)
{
    MemoryStream::ptr baseStream(new MemoryStream(Buffer("01234567890123456789")));
    BufferedStream::ptr bufferedStream(new BufferedStream(baseStream));
    bufferedStream->bufferSize(5);
    
    MORDOR_TEST_ASSERT(bufferedStream->supportsFind());
    MORDOR_TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 0);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 0);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->find('0'), 0);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 5);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 0);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->find("01234"), 0);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 5);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 0);
    Buffer output;
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 1), 1u);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->find("0123"), 9);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 15);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 1);
}

MORDOR_UNITTEST(BufferedStream, findSanityChecks)
{
    MemoryStream::ptr baseStream(new MemoryStream(Buffer("01234567890123456789")));
    BufferedStream::ptr bufferedStream(new BufferedStream(baseStream));
    bufferedStream->bufferSize(5);

    // Buffer size is 5, default sanity size is twice the buffer size
    MORDOR_TEST_ASSERT_EXCEPTION(bufferedStream->find('\n'), BufferOverflowException);
    MORDOR_TEST_ASSERT_EXCEPTION(bufferedStream->find("\r\n"), BufferOverflowException);
    MORDOR_TEST_ASSERT_EXCEPTION(bufferedStream->find('\n', 20), UnexpectedEofException);
    MORDOR_TEST_ASSERT_EXCEPTION(bufferedStream->find("\r\n", 20), UnexpectedEofException);

    MORDOR_TEST_ASSERT_LESS_THAN(bufferedStream->find('\n', ~0, false), 0);
    MORDOR_TEST_ASSERT_LESS_THAN(bufferedStream->find("\r\n", ~0, false), 0);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->find('\n', 20, false), -21);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->find("\r\n", 20, false), -21);
}

static void throwRuntimeError()
{
    throw std::runtime_error("");
}

MORDOR_UNITTEST(BufferedStream, errorOnRead)
{
    MemoryStream::ptr baseStream(new MemoryStream(Buffer("01234567890123456789")));
    TestStream::ptr testStream(new TestStream(baseStream));
    BufferedStream::ptr bufferedStream(new BufferedStream(testStream));
    testStream->onRead(&throwRuntimeError);

    MORDOR_TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 0);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 0);

    Buffer output;
    MORDOR_TEST_ASSERT_EXCEPTION(bufferedStream->read(output, 5), std::runtime_error);
    MORDOR_TEST_ASSERT_EQUAL(output.readAvailable(), 0u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 0);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 0);

    testStream->onRead(&throwRuntimeError, 2);
    // Partial read still allowed on exception (it's assumed the next read
    // will be either EOF or error)
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 5), 2u);
    MORDOR_TEST_ASSERT(output == "01");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 2);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 2);

    output.clear();
    // Make sure that's correct
    MORDOR_TEST_ASSERT_EXCEPTION(bufferedStream->read(output, 5), std::runtime_error);
    MORDOR_TEST_ASSERT_EQUAL(output.readAvailable(), 0u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->seek(0, Stream::CURRENT), 2);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(0, Stream::CURRENT), 2);
}

MORDOR_UNITTEST(BufferedStream, errorOnWrite)
{
    MemoryStream::ptr baseStream(new MemoryStream());
    TestStream::ptr testStream(new TestStream(baseStream));
    BufferedStream::ptr bufferedStream(new BufferedStream(testStream));
    bufferedStream->bufferSize(5);

    MORDOR_TEST_ASSERT_EQUAL(baseStream->size(), 0);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->size(), 0);

    testStream->onWrite(&throwRuntimeError);
    MORDOR_TEST_ASSERT_EXCEPTION(bufferedStream->write("hello", 5), std::runtime_error);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->size(), 0);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->size(), 0);

    testStream->onWrite(&throwRuntimeError, 3);
    // Exception swallowed; only 3 written to underlying stream (flush will
    // either clear buffer, or expose error); partial write guarantee still
    // enforced
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->write("helloworld", 10), 10u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->size(), 3);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->size(), 10);

    MORDOR_TEST_ASSERT_EXCEPTION(bufferedStream->flush(), std::runtime_error);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->size(), 3);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->size(), 10);

    testStream->onWrite(NULL);
    bufferedStream->flush();
    MORDOR_TEST_ASSERT_EQUAL(baseStream->size(), 10);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->size(), 10);
}
