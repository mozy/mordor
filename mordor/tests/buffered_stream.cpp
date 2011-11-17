// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/streams/memory.h"
#include "mordor/streams/buffered.h"
#include "mordor/streams/null.h"
#include "mordor/streams/singleplex.h"
#include "mordor/streams/test.h"
#include "mordor/test/test.h"
#include "mordor/thread.h"

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
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 0);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 0);

    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 2), 2u);
    MORDOR_TEST_ASSERT(output == "01");
    // baseStream should have had a full buffer read from it
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 5);
    // But the bufferedStream is hiding it
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 2);

    output.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 2), 2u);
    MORDOR_TEST_ASSERT(output == "23");
    // baseStream stays at the same position, because the read should have been
    // satisfied by the buffer
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 5);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 4);

    output.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 7), 7u);
    MORDOR_TEST_ASSERT(output == "4567890");
    // baseStream should have had two buffer-fuls read
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 15);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 11);

    output.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 2), 2u);
    MORDOR_TEST_ASSERT(output == "12");
    // baseStream stays at the same position, because the read should have been
    // satisfied by the buffer
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 15);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 13);

    output.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 10), 7u);
    MORDOR_TEST_ASSERT(output == "3456789");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 20);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 20);

    // Make sure the buffered stream gives us EOF properly
    output.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 10), 0u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 20);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 20);
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
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 6);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 5);

    bufferedStream->allowPartialReads(true);
    output.clear();
    // Use up the rest of what's buffered
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 5), 1u);
    MORDOR_TEST_ASSERT(output == "5");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 6);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 6);

    output.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 5), 2u);
    MORDOR_TEST_ASSERT(output == "67");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 8);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 8);
}

MORDOR_UNITTEST(BufferedStream, partialReadRawPointer)
{
    MemoryStream::ptr baseStream(new MemoryStream(Buffer("0123456789")));
    TestStream::ptr testStream(new TestStream(baseStream));
    BufferedStream::ptr bufferedStream(new BufferedStream(testStream));
    testStream->maxReadSize(2);

    char output[6];
    memset(output, 0, 6);

    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 5), 5u);
    MORDOR_TEST_ASSERT_EQUAL((const char *)output, "01234");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 6);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 5);
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
    Stream::ptr baseStream(new MemoryStream(Buffer("01234567890123456789")));
    baseStream.reset(new SingleplexStream(baseStream, SingleplexStream::READ));
    BufferedStream::ptr bufferedStream(new BufferedStream(baseStream));
    bufferedStream->bufferSize(5);

    MORDOR_TEST_ASSERT_EQUAL(baseStream->size(), 20);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->size(), 20);

    Buffer output;
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 6), 6u);
    MORDOR_TEST_ASSERT(output == "012345");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 10);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 6);

    output.consume(3);
    bufferedStream->unread(output);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 10);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 3);

    output.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 6), 6u);
    MORDOR_TEST_ASSERT(output == "345678");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 10);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 9);

    output.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 6), 6u);
    MORDOR_TEST_ASSERT(output == "901234");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 15);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 15);
}

MORDOR_UNITTEST(BufferedStream, find)
{
    MemoryStream::ptr baseStream(new MemoryStream(Buffer("01234567890123456789")));
    BufferedStream::ptr bufferedStream(new BufferedStream(baseStream));
    bufferedStream->bufferSize(5);

    MORDOR_TEST_ASSERT(bufferedStream->supportsFind());
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 0);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 0);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->find('0'), 0);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 5);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 0);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->find("01234"), 0);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 5);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 0);
    Buffer output;
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 1), 1u);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->find("0123"), 9);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 15);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 1);
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

    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 0);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 0);

    Buffer output;
    MORDOR_TEST_ASSERT_EXCEPTION(bufferedStream->read(output, 5), std::runtime_error);
    MORDOR_TEST_ASSERT_EQUAL(output.readAvailable(), 0u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 0);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 0);

    testStream->onRead(&throwRuntimeError, 2);
    // Partial read still allowed on exception (it's assumed the next read
    // will be either EOF or error)
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(output, 5), 2u);
    MORDOR_TEST_ASSERT(output == "01");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 2);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 2);

    output.clear();
    // Make sure that's correct
    MORDOR_TEST_ASSERT_EXCEPTION(bufferedStream->read(output, 5), std::runtime_error);
    MORDOR_TEST_ASSERT_EQUAL(output.readAvailable(), 0u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 2);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 2);
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

MORDOR_UNITTEST(BufferedStream, seek)
{
    MemoryStream::ptr baseStream(new MemoryStream(Buffer("abcdefghijklmnopqrstuvwxyz0123456789")));
    BufferedStream::ptr bufferedStream(new BufferedStream(baseStream));
    bufferedStream->bufferSize(5);

    // We all start out at 0
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 0);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 0);

    // Seeking to current position doesn't do anything
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(0), 0);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 0);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 0);

    // Seeking with nothing in the buffer sets both streams to exactly
    // where we ask
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(1), 1);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 1);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 1);

    // Make sure the data corresponds to where we are
    Buffer buffer;
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(buffer, 2), 2u);
    MORDOR_TEST_ASSERT(buffer == "bc");
    // Buffered readahead of 5 + 1 (current pos)
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 6);
    // But we're really at 3
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 3);
    // Seeking to current position doesn't do anything
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(3), 3);
    // Even to the underlying stream
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 6);
    buffer.clear();
    // Double-check the actual data again
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(buffer, 2), 2u);
    MORDOR_TEST_ASSERT(buffer == "de");
    // It should have been buffered and not touch the baseStream
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 6);
    // But did change our position
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 5);

    // Seeking outside of the buffer should clear it (and seek the baseStream)
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(0), 0);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 0);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 0);

    // Double-check actual data
    buffer.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(buffer, 2), 2u);
    MORDOR_TEST_ASSERT(buffer == "ab");
    // Buffered read-ahead
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 5);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 2);

    // Seek to the end of the buffer
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(5), 5);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 5);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 5);

    // Double-check actual data
    buffer.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(buffer, 2), 2u);
    MORDOR_TEST_ASSERT(buffer == "fg");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 10);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 7);

    // Seek into the buffer should keep the buffer, and not touch the baseStream
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(8), 8);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 10);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 8);
    buffer.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(buffer, 2), 2u);
    MORDOR_TEST_ASSERT(buffer == "ij");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 10);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 10);

    // Relative seek, with no buffer
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(1, Stream::CURRENT), 11);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 11);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 11);
    buffer.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(buffer, 2), 2u);
    MORDOR_TEST_ASSERT(buffer == "lm");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 16);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 13);

    // Relative forward seek into the buffer should keep the buffer, and not touch baseStream
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(1, Stream::CURRENT), 14);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 16);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 14);
    buffer.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(buffer, 2), 2u);
    MORDOR_TEST_ASSERT(buffer == "op");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 16);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 16);

    // Buffer some data
    bufferedStream->bufferSize(20);
    buffer.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(buffer, 2), 2u);
    MORDOR_TEST_ASSERT(buffer == "qr");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 36);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 18);

    // Absolute seek to beginning of buffer does nothing
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(18), 18);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 36);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 18);
    buffer.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(buffer, 2), 2u);
    MORDOR_TEST_ASSERT(buffer == "st");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 36);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 20);

    // Relative backward seek beyond buffer should discard buffer, and seek baseStream
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->seek(-18, Stream::CURRENT), 2);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 2);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 2);
    buffer.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(buffer, 4), 4u);
    MORDOR_TEST_ASSERT(buffer == "cdef");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 22);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 6);
}

MORDOR_UNITTEST(BufferedStream, readAndWrite)
{
    MemoryStream::ptr baseStream(new MemoryStream(Buffer("abcdefghijklmnopqrstuvwxyz0123456789")));
    BufferedStream::ptr bufferedStream(new BufferedStream(baseStream));
    bufferedStream->bufferSize(5);

    Buffer buffer;
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(buffer, 2), 2u);
    MORDOR_TEST_ASSERT(buffer == "ab");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 5);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 2);

    // Buffer a write (doesn't touch the read buffer yet)
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->write("CD", 2), 2u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 5);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 4);
    // Now flush the write, and it should re-seek the baseStream
    bufferedStream->flush();
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 4);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 4);
    MORDOR_TEST_ASSERT(baseStream->buffer() == "abCDefghijklmnopqrstuvwxyz0123456789");
    // Read some into the buffer
    buffer.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(buffer, 2), 2u);
    MORDOR_TEST_ASSERT(buffer == "ef");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 9);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 6);
    // Buffer a write (doesn't touch the read buffer yet)
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->write("GH", 2), 2u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 9);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 8);
    // Check that the read buffer is in sync (implicitly flushes)
    buffer.clear();
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->read(buffer, 2), 2u);
    MORDOR_TEST_ASSERT(buffer == "ij");
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 13);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 10);
    MORDOR_TEST_ASSERT(baseStream->buffer() == "abCDefGHijklmnopqrstuvwxyz0123456789");
    // Manual flush does nothing
    bufferedStream->flush();
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 13);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 10);

    // Extra-large write flushes in progress
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->write("KLMNOPQRS", 9), 9u);
    MORDOR_TEST_ASSERT_EQUAL(baseStream->tell(), 19);
    MORDOR_TEST_ASSERT_EQUAL(bufferedStream->tell(), 19);
    MORDOR_TEST_ASSERT(baseStream->buffer() == "abCDefGHijKLMNOPQRStuvwxyz0123456789");
}

MORDOR_UNITTEST(BufferedStream, partiallyBufferedReadRawBuffer)
{
    MemoryStream::ptr baseStream(new MemoryStream("0123456789"));
    BufferedStream::ptr bufferedStream(new BufferedStream(baseStream));
    bufferedStream->bufferSize(3);
    Stream::ptr stream = bufferedStream;
    char buffer[3];
    buffer[2] = '\0';
    MORDOR_TEST_ASSERT_EQUAL(stream->read(buffer, 2), 2u);
    MORDOR_TEST_ASSERT_EQUAL((const char *)buffer, "01");
    MORDOR_TEST_ASSERT_EQUAL(stream->read(buffer, 2), 2u);
    MORDOR_TEST_ASSERT_EQUAL((const char *)buffer, "23");
}

namespace
{
    void doWrite(Stream::ptr stream, size_t totalSize)
    {
        size_t write = 0;
        char data[4096] = {0};
        while (write < totalSize) {
            int writeSize = rand() % 2048 + 2048;
            write += stream->write(&data, writeSize);
        }
    }

    void doRead(Stream::ptr stream, size_t totalSize)
    {
        size_t read = 0;
        char data[4096] = {0};
        while(read < totalSize) {
            int readSize = rand() % 2048 + 2048;
            stream->read(&data, readSize);
            read += readSize;
        }
    }

    void parallelReadWrite(Stream::ptr stream)
    {
        size_t totalSize = 16 * 1024 * 1024ull; // 16MB
        boost::shared_ptr<Thread> writeThread(new Thread(boost::bind(doWrite, stream, totalSize)));
        boost::shared_ptr<Thread> readThread(new Thread(boost::bind(doRead, stream, totalSize)));
        writeThread->join();
        readThread->join();
    }

    class SeeklessStream : public FilterStream
    {
    public:
        SeeklessStream(Stream::ptr parent, bool own=true) : FilterStream(parent, own) {}
        bool supportsSeek() { return false; }
        size_t read(Buffer &buffer, size_t length) { return parent()->read(buffer, length); }
        size_t read(void *buffer, size_t length) { return parent()->read(buffer, length); }
        size_t write(const Buffer &buffer, size_t length) { return parent()->write(buffer, length); }
        size_t write(const void *buffer, size_t length) { return parent()->write(buffer, length); }
    };
}

MORDOR_UNITTEST(BufferedStream, parallelReadWriteNullStream)
{
    // NullStream is a read-write thread-safe stream
    parallelReadWrite(NullStream::get_ptr());
}

/* Comment out because BufferedStream is not read-write thread-safe currently for seekable stream
MORDOR_UNITTEST(BufferedStream, parallelReadWriteBufferedStream)
{
    // wrapping with BufferedStream, it is no longer read-write thread-safe
    parallelReadWrite(Stream::ptr(new BufferedStream(NullStream::get_ptr())));
}
*/

MORDOR_UNITTEST(BufferedStream, parallelReadWriteSeeklessStream)
{
    // wrapping with BufferedStream, it is still read-write thread-safe
    // as long as parent stream is seekless
    parallelReadWrite(Stream::ptr(new BufferedStream(
        Stream::ptr(new SeeklessStream(NullStream::get_ptr())))));
}
