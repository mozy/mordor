// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include <iostream>

#include "mordor/exception.h"
#include "mordor/iomanager.h"
#include "mordor/socket.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

MORDOR_SUITE_INVARIANT(Socket)
{
    MORDOR_TEST_ASSERT(!Fiber::getThis());
    MORDOR_TEST_ASSERT(!Scheduler::getThis());
}

MORDOR_UNITTEST(Socket, acceptTimeout)
{
    Fiber::ptr mainfiber(new Fiber());
    IOManager ioManager;
    std::vector<Address::ptr> addresses = Address::lookup("localhost:8000", AF_UNSPEC, SOCK_STREAM);
    MORDOR_TEST_ASSERT(!addresses.empty());
    // TODO: random port
    Socket::ptr listen = addresses.front()->createSocket(ioManager);
    listen->receiveTimeout(1000000);
    unsigned int opt = 1;
    listen->setOption(SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    listen->bind(addresses.front());
    listen->listen();
    unsigned long long start = TimerManager::now();
    MORDOR_TEST_ASSERT_EXCEPTION(listen->accept(), TimedOutException);
    MORDOR_TEST_ASSERT_ABOUT_EQUAL(start + 1000000, TimerManager::now(), 100000);
}

MORDOR_UNITTEST(Socket, receiveTimeout)
{
    Fiber::ptr mainfiber(new Fiber());
    IOManager ioManager;
    std::vector<Address::ptr> addresses = Address::lookup("localhost:8000", AF_UNSPEC, SOCK_STREAM);
    MORDOR_TEST_ASSERT(!addresses.empty());
    // TODO: random port
    Socket::ptr listen = addresses.front()->createSocket(ioManager);
    unsigned int opt = 1;
    listen->setOption(SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    listen->bind(addresses.front());
    listen->listen();
    Socket::ptr connect = addresses.front()->createSocket(ioManager);
    connect->receiveTimeout(1000000);
    connect->connect(addresses.front());
    char buf;
    unsigned long long start = TimerManager::now();
    MORDOR_TEST_ASSERT_EXCEPTION(connect->receive(&buf, 1), TimedOutException);
    MORDOR_TEST_ASSERT_ABOUT_EQUAL(start + 1000000, TimerManager::now(), 100000);
}

class DummyException
{};

template <class Exception>
static void testShutdownException(bool send, bool shutdown, bool otherEnd)
{
    Fiber::ptr mainfiber(new Fiber());
    IOManager ioManager;
    std::vector<Address::ptr> addresses = Address::lookup("localhost:8000", AF_UNSPEC, SOCK_STREAM);
    MORDOR_TEST_ASSERT(!addresses.empty());
    // TODO: random port
    Socket::ptr listen = addresses.front()->createSocket(ioManager);
    unsigned int opt = 1;
    listen->setOption(SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    listen->bind(addresses.front());
    listen->listen();
    Socket::ptr connect = addresses.front()->createSocket(ioManager);
    connect->connect(addresses.front());
    Socket::ptr accept = listen->accept();

    Socket::ptr socketToClose = otherEnd ? accept : connect;
    if (shutdown)
        socketToClose->shutdown(SHUT_RDWR);
    else
        socketToClose->close();

    if (send) {
        if (otherEnd) {
            try {
                connect->sendTimeout(100);
                while (true) {
                    MORDOR_TEST_ASSERT_EQUAL(connect->send("abc", 3), 3u);
                }
            } catch (Exception) {
            }
        } else {
            MORDOR_TEST_ASSERT_EXCEPTION(connect->send("abc", 3), Exception);
        }
    } else {
        unsigned char readBuf[3];
        if (otherEnd) {
            MORDOR_TEST_ASSERT_EQUAL(connect->receive(readBuf, 3), 0u);
        } else {
#ifndef WINDOWS
            // Silly non-Windows letting you receive after you told it no more
            if (shutdown) {
                MORDOR_TEST_ASSERT_EQUAL(connect->receive(readBuf, 3), 0u);
            } else
#endif
            {
                MORDOR_TEST_ASSERT_EXCEPTION(connect->receive(readBuf, 3), Exception);
            }
        }
    }
}

MORDOR_UNITTEST(Socket, sendAfterClose)
{
    testShutdownException<BadHandleException>(true, false, false);
}

MORDOR_UNITTEST(Socket, receiveAfterClose)
{
    testShutdownException<BadHandleException>(false, false, false);
}

MORDOR_UNITTEST(Socket, sendAfterShutdown)
{
    testShutdownException<BrokenPipeException>(true, true, false);
}

MORDOR_UNITTEST(Socket, receiveAfterShutdown)
{
    testShutdownException<BrokenPipeException>(false, true, false);
}

MORDOR_UNITTEST(Socket, sendAfterCloseOtherEnd)
{
#ifdef WINDOWS
    testShutdownException<ConnectionAbortedException>(true, false, true);
#else
    try {
        testShutdownException<BrokenPipeException>(true, false, true);
        // Could also be ConnectionReset on BSDs
    } catch (ConnectionResetException)
    {}
#endif
}

MORDOR_UNITTEST(Socket, receiveAfterCloseOtherEnd)
{
    // Exception is not used; this is special cased in testShutdownException
    testShutdownException<DummyException>(false, false, true);
}

MORDOR_UNITTEST(Socket, sendAfterShutdownOtherEnd)
{
#ifdef WINDOWS
    testShutdownException<ConnectionAbortedException>(true, false, true);
#elif defined(BSD)
    // BSD lets you write to the socket, but it blocks, so we have to check
    // for it blocking
    testShutdownException<OperationAbortedException>(true, true, true);
#else
    testShutdownException<BrokenPipeException>(true, false, true);
#endif
}

MORDOR_UNITTEST(Socket, receiveAfterShutdownOtherEnd)
{
    // Exception is not used; this is special cased in testShutdownException
    testShutdownException<DummyException>(false, true, true);
}

static void testAddress(const char *addr, const char *expected = NULL)
{
    if (!expected)
        expected = addr;
    std::ostringstream os;
    std::vector<Address::ptr> address = Address::lookup(addr);
    os << *address.front();
    MORDOR_TEST_ASSERT_EQUAL(os.str(), expected);
}

MORDOR_UNITTEST(Address, formatAddresses)
{
    testAddress("127.0.0.1", "127.0.0.1:0");
    testAddress("127.0.0.1:80");
    testAddress("::", "[::]:0");
    testAddress("[::]:80", "[::]:80");
    testAddress("::1", "[::1]:0");
    testAddress("[2001:470:1f05:273:20c:29ff:feb3:5ddf]:0");
    testAddress("[2001:470::273:20c:0:0:5ddf]:0");
    testAddress("[2001:470:0:0:273:20c::5ddf]:0", "[2001:470::273:20c:0:5ddf]:0");
}

static void cancelMe(Socket::ptr sock)
{
    sock->cancelAccept();
}

MORDOR_UNITTEST(Socket, cancelAccept)
{
    Fiber::ptr mainfiber(new Fiber());
    IOManager ioManager;
    std::vector<Address::ptr> addresses = Address::lookup("localhost:8000", AF_UNSPEC, SOCK_STREAM);
    MORDOR_TEST_ASSERT(!addresses.empty());
    // TODO: random port
    Socket::ptr listen = addresses.front()->createSocket(ioManager);
    unsigned int opt = 1;
    listen->setOption(SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    listen->bind(addresses.front());
    listen->listen();

    // cancelMe will get run when this fiber yields because it would block
    ioManager.schedule(Fiber::ptr(new Fiber(boost::bind(&cancelMe, listen))));
    MORDOR_TEST_ASSERT_EXCEPTION(listen->accept(), OperationAbortedException);
}
