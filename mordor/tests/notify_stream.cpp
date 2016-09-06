#include "mordor/exception.h"
#include "mordor/streams/notify.h"
#include "mordor/streams/memory.h"
#include "mordor/test/test.h"
#include "mordor/thread.h"
#include "mordor/workerpool.h"

using namespace Mordor;
using namespace Mordor::Test;

namespace {

struct ReadException  : public Mordor::Exception {};
struct WriteException : public Mordor::Exception {};
struct CloseException : public Mordor::Exception {};
struct FlushException : public Mordor::Exception {};

class ExceptionStream : public Stream
{
public:
    ExceptionStream() {}

    using Stream::read;
    virtual size_t read(Buffer &buffer, size_t length)
    { MORDOR_THROW_EXCEPTION(ReadException()); }

    using Stream::write;
    virtual size_t write(const Buffer &buffer, size_t length)
    { MORDOR_THROW_EXCEPTION(WriteException()); }

    virtual void flush(bool flushParent = false)
    { MORDOR_THROW_EXCEPTION(FlushException()); }

    virtual void close(CloseType type = BOTH)
    { MORDOR_THROW_EXCEPTION(CloseException()); }
};

void onNotify(int &sequence)
{
    ++sequence;
}

void onNotifySwitchThread(int &sequence, WorkerPool &workerPool)
{
    workerPool.switchTo();
    ++sequence;
}

}

MORDOR_UNITTEST(NotifyStream, basic)
{
    int sequence = 0;
    NotifyStream stream(Stream::ptr(new MemoryStream()));
    MORDOR_TEST_ASSERT_EQUAL(sequence, 0);
    Buffer buffer;
    stream.notifyOnEof = boost::bind(onNotify, boost::ref(sequence));
    // hit EOF
    MORDOR_TEST_ASSERT_EQUAL(stream.read(buffer, 65536), 0U);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
    stream.notifyOnEof.clear();

    stream.notifyOnFlush = boost::bind(onNotify, boost::ref(sequence));
    stream.flush();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 2);
    stream.notifyOnFlush.clear();

    stream.notifyOnClose(boost::bind(onNotify, boost::ref(sequence)));
    stream.close();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 3);
    stream.notifyOnClose(NULL);
}

static void onNotifyClose(int &sequence, Stream::CloseType type, Stream::CloseType expected)
{
    MORDOR_TEST_ASSERT_EQUAL(type, expected);
    ++sequence;
}

MORDOR_UNITTEST(NotifyStream, notifyOnClose2)
{
    int sequence = 0;
    NotifyStream stream(Stream::ptr(new MemoryStream()));
    MORDOR_TEST_ASSERT_EQUAL(sequence, 0);
    stream.notifyOnClose2(boost::bind(onNotifyClose, boost::ref(sequence), _1, Stream::READ));
    stream.close(Stream::READ);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
    stream.notifyOnClose2(boost::bind(onNotifyClose, boost::ref(sequence), _1, Stream::WRITE));
    stream.close(Stream::WRITE);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 2);
    stream.notifyOnClose2(NULL);
}

MORDOR_UNITTEST(NotifyStream, notifyOnExceptionSameThread)
{
    int sequence = 0;
    NotifyStream stream(Stream::ptr(new ExceptionStream()));
    stream.notifyOnException = boost::bind(onNotify, boost::ref(sequence));
    MORDOR_TEST_ASSERT_EQUAL(sequence, 0);
    Buffer buffer;

    MORDOR_TEST_ASSERT_EXCEPTION(stream.read(buffer, 65536), ReadException);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);

    MORDOR_TEST_ASSERT_EXCEPTION(stream.write(buffer, 65536), WriteException);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 2);

    MORDOR_TEST_ASSERT_EXCEPTION(stream.flush(true), FlushException);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 3);

    MORDOR_TEST_ASSERT_EXCEPTION(stream.close(), CloseException);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 4);
}

MORDOR_UNITTEST(NotifyStream, notifyExceptionThreadSwitch)
{
    WorkerPool poolA(1, true), poolB(1, false);
    int sequence = 0;
    NotifyStream stream(Stream::ptr(new ExceptionStream()));
    stream.notifyOnException = boost::bind(onNotifySwitchThread,
                                           boost::ref(sequence),
                                           boost::ref(poolB));
    MORDOR_TEST_ASSERT_EQUAL(sequence, 0);

    // in poolA
    tid_t tidA = gettid();
    MORDOR_TEST_ASSERT_EQUAL(Scheduler::getThis(), &poolA);
    Buffer buffer;
    MORDOR_TEST_ASSERT_EXCEPTION(stream.write(buffer, 65536), WriteException);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
    // now in poolB
    tid_t tidB = gettid();
    MORDOR_TEST_ASSERT_EQUAL(Scheduler::getThis(), &poolB);
    MORDOR_TEST_ASSERT_NOT_EQUAL(tidA, tidB);
    poolA.switchTo();
    poolB.stop();
    poolA.stop();
}
