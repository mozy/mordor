// Copyright (c) 2010 - Mozy, Inc.

#include "mordor/iomanager.h"
#include "mordor/streams/buffer.h"
#include "mordor/streams/delay.h"
#include "mordor/streams/pipe.h"
#include "mordor/streams/timeout.h"
#include "mordor/test/test.h"

using namespace Mordor;

MORDOR_UNITTEST(TimeoutStream, readTimeout)
{
    IOManager ioManager;

    std::pair<Stream::ptr, Stream::ptr> streams = pipeStream();
    TimeoutStream::ptr timeout(new TimeoutStream(streams.first, ioManager));
    timeout->readTimeout(0);

    Buffer rb("test");
    MORDOR_TEST_ASSERT_EXCEPTION(timeout->read(rb, 4), TimedOutException);
}

MORDOR_UNITTEST(TimeoutStream, writeTimeout)
{
    IOManager ioManager;

    std::pair<Stream::ptr, Stream::ptr> streams = pipeStream(1);
    DelayStream::ptr delay(new DelayStream(streams.second, &ioManager, 1000));
    TimeoutStream::ptr timeout(new TimeoutStream(delay, ioManager));
    timeout->writeTimeout(0);

    Buffer wb("test");
    size_t len = 4;
    try {
        while (len > 0)
            len -= timeout->write(wb, len);
    } catch (TimedOutException &) {
        return;
    }
    MORDOR_NOTREACHED();
}

MORDOR_UNITTEST(TimeoutStream, idleTimeout)
{
    IOManager ioManager;

    std::pair<Stream::ptr, Stream::ptr> streams = pipeStream();
    TimeoutStream::ptr timeout(new TimeoutStream(streams.first, ioManager));
    timeout->idleTimeout(0);

    Buffer rb("test");
    MORDOR_TEST_ASSERT_EXCEPTION(timeout->read(rb, 4), TimedOutException);
}

MORDOR_UNITTEST(TimeoutStream, timeoutSetAfterOpBegan)
{
    IOManager ioManager;

    std::pair<Stream::ptr, Stream::ptr> streams = pipeStream();
    TimeoutStream::ptr timeout(new TimeoutStream(streams.first, ioManager));

    Buffer rb("test");
    ioManager.schedule(boost::bind(&TimeoutStream::readTimeout, timeout, 0));
    MORDOR_TEST_ASSERT_EXCEPTION(timeout->read(rb, 4), TimedOutException);
}

MORDOR_UNITTEST(TimeoutStream, timeoutChangedAfterOpBegan)
{
    IOManager ioManager;

    std::pair<Stream::ptr, Stream::ptr> streams = pipeStream();
    TimeoutStream::ptr timeout(new TimeoutStream(streams.first, ioManager));

    timeout->readTimeout(400000);
    Buffer rb("test");
    ioManager.schedule(boost::bind(&TimeoutStream::readTimeout, timeout, 200000));
    unsigned long long now = TimerManager::now();
    MORDOR_TEST_ASSERT_EXCEPTION(timeout->read(rb, 4), TimedOutException);
    MORDOR_TEST_ASSERT_ABOUT_EQUAL(TimerManager::now() - now, 200000u, 50000);
}
