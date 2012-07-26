#include "mordor/streams/counter.h"
#include "mordor/streams/memory.h"
#include "mordor/test/test.h"

namespace Mordor {

MORDOR_UNITTEST(CounterStream, countReadWrite)
{
    MemoryStream::ptr mem(new MemoryStream);
    CounterStream::ptr counter(new CounterStream(mem));
    MORDOR_TEST_ASSERT_EQUAL(counter->bytesRead(), 0U);
    MORDOR_TEST_ASSERT_EQUAL(counter->bytesWritten(), 0U);
    counter->write("1234567890", 10);
    MORDOR_TEST_ASSERT_EQUAL(counter->bytesRead(), 0U);
    MORDOR_TEST_ASSERT_EQUAL(counter->bytesWritten(), 10U);
    mem->seek(0);
    Buffer buffer;
    counter->read(buffer, 5);
    MORDOR_TEST_ASSERT_EQUAL(counter->bytesRead(), 5U);
    MORDOR_TEST_ASSERT_EQUAL(counter->bytesWritten(), 10U);
    MORDOR_TEST_ASSERT_EQUAL(buffer.readAvailable(), 5U);

}

}
