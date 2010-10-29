// Copyright (c) 2010 - Mozy, Inc.

#include "mordor/iomanager.h"
#include "mordor/streams/buffer.h"
#include "mordor/streams/pipe.h"
#include "mordor/streams/timeout.h"
#include "mordor/test/test.h"

using namespace Mordor;

MORDOR_UNITTEST(TimeoutStream, basicTimeout)
{
    IOManager ioManager;

    std::pair<Stream::ptr, Stream::ptr> streams = pipeStream();
    TimeoutStream::ptr timeout(new TimeoutStream(streams.first, ioManager));

    timeout->readTimeout(0);
    Buffer buffer("test");
    MORDOR_TEST_ASSERT_EXCEPTION(timeout->read(buffer, 4), TimedOutException);
}

MORDOR_UNITTEST(TimeoutStream, timeoutSetAfterOpBegan)
{
    IOManager ioManager;

    std::pair<Stream::ptr, Stream::ptr> streams = pipeStream();
    TimeoutStream::ptr timeout(new TimeoutStream(streams.first, ioManager));

    Buffer buffer("test");
    ioManager.schedule(boost::bind(&TimeoutStream::readTimeout, timeout, 0));
    MORDOR_TEST_ASSERT_EXCEPTION(timeout->read(buffer, 4), TimedOutException);
}

MORDOR_UNITTEST(TimeoutStream, timeoutChangedAfterOpBegan)
{
    IOManager ioManager;

    std::pair<Stream::ptr, Stream::ptr> streams = pipeStream();
    TimeoutStream::ptr timeout(new TimeoutStream(streams.first, ioManager));

    timeout->readTimeout(400000);
    Buffer buffer("test");
    ioManager.schedule(boost::bind(&TimeoutStream::readTimeout, timeout, 200000));
    unsigned long long now = TimerManager::now();
    MORDOR_TEST_ASSERT_EXCEPTION(timeout->read(buffer, 4), TimedOutException);
    MORDOR_TEST_ASSERT_ABOUT_EQUAL(TimerManager::now() - now, 200000u, 50000);
}
