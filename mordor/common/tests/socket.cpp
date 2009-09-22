// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include <iostream>

#include "mordor/common/exception.h"
#include "mordor/common/iomanager.h"
#include "mordor/common/socket.h"
#include "mordor/test/test.h"

SUITE_INVARIANT(Socket)
{
    TEST_ASSERT(!Fiber::getThis());
    TEST_ASSERT(!Scheduler::getThis());
}

TEST_WITH_SUITE(Socket, acceptTimeout)
{
    Fiber::ptr mainfiber(new Fiber());
    IOManager ioManager;
    std::vector<Address::ptr> addresses = Address::lookup("localhost:8000", AF_UNSPEC, SOCK_STREAM);
    TEST_ASSERT(!addresses.empty());
    // TODO: random port
    Socket::ptr listen = addresses.front()->createSocket(ioManager);
    listen->receiveTimeout(1000000);
    unsigned int opt = 1;
    listen->setOption(SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    listen->bind(addresses.front());
    listen->listen();
    unsigned long long start = TimerManager::now();
    TEST_ASSERT_EXCEPTION(listen->accept(), OperationAbortedException);
    TEST_ASSERT_ABOUT_EQUAL(start + 1000000, TimerManager::now(), 100000);
}

TEST_WITH_SUITE(Socket, receiveTimeout)
{
    Fiber::ptr mainfiber(new Fiber());
    IOManager ioManager;
    std::vector<Address::ptr> addresses = Address::lookup("localhost:8000", AF_UNSPEC, SOCK_STREAM);
    TEST_ASSERT(!addresses.empty());
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
    TEST_ASSERT_EXCEPTION(connect->receive(&buf, 1), OperationAbortedException);
    TEST_ASSERT_ABOUT_EQUAL(start + 1000000, TimerManager::now(), 100000);
}

class DummyException
{};

template <class Exception>
static void testShutdownException(bool send, bool shutdown, bool otherEnd)
{
    Fiber::ptr mainfiber(new Fiber());
    IOManager ioManager;
    std::vector<Address::ptr> addresses = Address::lookup("localhost:8000", AF_UNSPEC, SOCK_STREAM);
    TEST_ASSERT(!addresses.empty());
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
                    TEST_ASSERT_EQUAL(connect->send("abc", 3), 3u);
                }
            } catch (Exception) {
            }
        } else {
            TEST_ASSERT_EXCEPTION(connect->send("abc", 3), Exception);
        }
    } else {
        unsigned char readBuf[3];
        if (otherEnd) {
            TEST_ASSERT_EQUAL(connect->receive(readBuf, 3), 0u);
        } else {
#ifndef WINDOWS
            // Silly non-Windows letting you receive after you told it no more
            if (shutdown) {
                TEST_ASSERT_EQUAL(connect->receive(readBuf, 3), 0u);
            } else
#endif
            {
                TEST_ASSERT_EXCEPTION(connect->receive(readBuf, 3), Exception);
            }
        }
    }
}

TEST_WITH_SUITE(Socket, sendAfterClose)
{
    testShutdownException<BadHandleException>(true, false, false);
}

TEST_WITH_SUITE(Socket, receiveAfterClose)
{
    testShutdownException<BadHandleException>(false, false, false);
}

TEST_WITH_SUITE(Socket, sendAfterShutdown)
{
    testShutdownException<BrokenPipeException>(true, true, false);
}

TEST_WITH_SUITE(Socket, receiveAfterShutdown)
{
    testShutdownException<BrokenPipeException>(false, true, false);
}

TEST_WITH_SUITE(Socket, sendAfterCloseOtherEnd)
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

TEST_WITH_SUITE(Socket, receiveAfterCloseOtherEnd)
{
    // Exception is not used; this is special cased in testShutdownException
    testShutdownException<DummyException>(false, false, true);
}

TEST_WITH_SUITE(Socket, sendAfterShutdownOtherEnd)
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

TEST_WITH_SUITE(Socket, receiveAfterShutdownOtherEnd)
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
    TEST_ASSERT_EQUAL(os.str(), expected);
}

TEST_WITH_SUITE(Address, formatAddresses)
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

TEST_WITH_SUITE(Socket, cancelAccept)
{
    Fiber::ptr mainfiber(new Fiber());
    IOManager ioManager;
    std::vector<Address::ptr> addresses = Address::lookup("localhost:8000", AF_UNSPEC, SOCK_STREAM);
    TEST_ASSERT(!addresses.empty());
    // TODO: random port
    Socket::ptr listen = addresses.front()->createSocket(ioManager);
    unsigned int opt = 1;
    listen->setOption(SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    listen->bind(addresses.front());
    listen->listen();

    // cancelMe will get run when this fiber yields because it would block
    ioManager.schedule(Fiber::ptr(new Fiber(boost::bind(&cancelMe, listen))));
    TEST_ASSERT_EXCEPTION(listen->accept(), OperationAbortedException);
}
