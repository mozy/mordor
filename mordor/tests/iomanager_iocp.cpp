// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#ifdef WINDOWS

#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include "mordor/iomanager.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

static void handleEvent(bool &fired)
{
    fired = true;
}

static void signalEvent(HANDLE hEvent)
{
    Sleep(500);
    SetEvent(hEvent);
}

MORDOR_UNITTEST(IOManager, eventPreventsStop)
{
    HANDLE hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!hEvent)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateEventW");
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
    MORDOR_TEST_ASSERT(fired);
    CloseHandle(hEvent);
}


#endif
