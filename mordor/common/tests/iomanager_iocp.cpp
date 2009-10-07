// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#ifdef WINDOWS

#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include "mordor/common/iomanager.h"
#include "mordor/test/test.h"

static void handleEvent(bool &fired)
{
    fired = true;
}

static void signalEvent(HANDLE hEvent)
{
    Sleep(500);
    SetEvent(hEvent);
}

TEST_WITH_SUITE(IOManager, eventPreventsStop)
{
    HANDLE hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!hEvent)
        THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateEventW");
    bool fired = false;
    try {
        Fiber::ptr mainfiber(new Fiber());
        IOManager ioManager;
        ioManager.registerEvent(hEvent, boost::bind(&handleEvent,
            boost::ref(fired)));
        boost::thread thread(boost::bind(&signalEvent, hEvent));
    } catch (...) {
        CloseHandle(hEvent);
    }
    TEST_ASSERT(fired);
    CloseHandle(hEvent);
}


#endif
