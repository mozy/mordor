// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include <iostream>

#include "mordor/exception.h"
#include "mordor/iomanager.h"
#include "mordor/socket.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

namespace {
struct Connection
{
    Socket::ptr connect;
    Socket::ptr listen;
    Socket::ptr accept;
    IPAddress::ptr address;
};
}

static void acceptOne(Connection &conns)
{
    MORDOR_ASSERT(conns.listen);
    conns.accept = conns.listen->accept();
}

Connection
establishConn(IOManager &ioManager)
{
    Connection result;
    std::vector<Address::ptr> addresses = Address::lookup("localhost", AF_UNSPEC, SOCK_STREAM);
    MORDOR_TEST_ASSERT(!addresses.empty());
    result.address = boost::dynamic_pointer_cast<IPAddress>(addresses.front());
    result.listen = result.address->createSocket(ioManager);
    unsigned int opt = 1;
    result.listen->setOption(SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    while (true) {
        try {
            // Random port > 1000
            result.address->port(rand() % 50000 + 1000);
            result.listen->bind(result.address);
            break;
        } catch (AddressInUseException &) {
        }
    }
    result.listen->listen();
    result.connect = result.address->createSocket(ioManager);
    return result;
}

MORDOR_SUITE_INVARIANT(Socket)
{
    MORDOR_TEST_ASSERT(!Scheduler::getThis());
}

MORDOR_UNITTEST(Socket, acceptTimeout)
{
    IOManager ioManager;
    Connection conns = establishConn(ioManager);
    conns.listen->receiveTimeout(1000000);
    unsigned long long start = TimerManager::now();
    MORDOR_TEST_ASSERT_EXCEPTION(conns.listen->accept(), TimedOutException);
    MORDOR_TEST_ASSERT_ABOUT_EQUAL(start + 1000000, TimerManager::now(), 100000);
    MORDOR_TEST_ASSERT_EXCEPTION(conns.listen->accept(), TimedOutException);
}

MORDOR_UNITTEST(Socket, receiveTimeout)
{
    IOManager ioManager;
    Connection conns = establishConn(ioManager);
    conns.connect->receiveTimeout(1000000);
    ioManager.schedule(boost::bind(&acceptOne, boost::ref(conns)));
    conns.connect->connect(conns.address);
    ioManager.dispatch();
    char buf;
    unsigned long long start = TimerManager::now();
    MORDOR_TEST_ASSERT_EXCEPTION(conns.connect->receive(&buf, 1), TimedOutException);
    MORDOR_TEST_ASSERT_ABOUT_EQUAL(start + 1000000, TimerManager::now(), 100000);
    MORDOR_TEST_ASSERT_EXCEPTION(conns.connect->receive(&buf, 1), TimedOutException);
}

MORDOR_UNITTEST(Socket, sendTimeout)
{
    IOManager ioManager;
    Connection conns = establishConn(ioManager);
    conns.connect->sendTimeout(1000000);
    ioManager.schedule(boost::bind(&acceptOne, boost::ref(conns)));
    conns.connect->connect(conns.address);
    ioManager.dispatch();
    char buf[65536];
    memset(buf, 0, sizeof(buf));
    unsigned long long start = TimerManager::now();
    MORDOR_TEST_ASSERT_EXCEPTION(while (true) conns.connect->send(buf, sizeof(buf)), TimedOutException);
    MORDOR_TEST_ASSERT_ABOUT_EQUAL(start + 1000000, TimerManager::now(), 100000);
    MORDOR_TEST_ASSERT_EXCEPTION(conns.connect->send(buf, sizeof(buf)), TimedOutException);
}

class DummyException
{};

template <class Exception>
static void testShutdownException(bool send, bool shutdown, bool otherEnd)
{
    IOManager ioManager;
    Connection conns = establishConn(ioManager);
    ioManager.schedule(boost::bind(&acceptOne, boost::ref(conns)));
    conns.connect->connect(conns.address);
    ioManager.dispatch();

    Socket::ptr &socketToClose = otherEnd ? conns.accept : conns.connect;
    if (shutdown)
        socketToClose->shutdown(SHUT_RDWR);
    else
        socketToClose.reset();

    if (send) {
        if (otherEnd) {
            try {
                conns.connect->sendTimeout(100);
                while (true) {
                    MORDOR_TEST_ASSERT_EQUAL(conns.connect->send("abc", 3), 3u);
                }
            } catch (Exception) {
            }
        } else {
            MORDOR_TEST_ASSERT_EXCEPTION(conns.connect->send("abc", 3), Exception);
        }
    } else {
        unsigned char readBuf[3];
        if (otherEnd) {
            MORDOR_TEST_ASSERT_EQUAL(conns.connect->receive(readBuf, 3), 0u);
        } else {
#ifndef WINDOWS
            // Silly non-Windows letting you receive after you told it no more
            if (shutdown) {
                MORDOR_TEST_ASSERT_EQUAL(conns.connect->receive(readBuf, 3), 0u);
            } else
#endif
            {
                MORDOR_TEST_ASSERT_EXCEPTION(conns.connect->receive(readBuf, 3), Exception);
            }
        }
    }
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
    testShutdownException<TimedOutException>(true, true, true);
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
    IOManager ioManager;
    Connection conns = establishConn(ioManager);

    // cancelMe will get run when this fiber yields because it would block
    ioManager.schedule(Fiber::ptr(new Fiber(boost::bind(&cancelMe, conns.listen))));
    MORDOR_TEST_ASSERT_EXCEPTION(conns.listen->accept(), OperationAbortedException);
}

MORDOR_UNITTEST(Socket, cancelSend)
{
    IOManager ioManager;
    Connection conns = establishConn(ioManager);
    ioManager.schedule(boost::bind(&acceptOne, boost::ref(conns)));
    conns.connect->connect(conns.address);
    ioManager.dispatch();

    boost::scoped_array<char> array(new char[65536]);
    struct iovec iov;
    iov.iov_base = array.get();
    iov.iov_len = 65536;
    conns.connect->cancelSend();
    MORDOR_TEST_ASSERT_EXCEPTION(while (conns.connect->send(iov.iov_base, 65536)) {}, OperationAbortedException);
    MORDOR_TEST_ASSERT_EXCEPTION(conns.connect->send(&iov, 1), OperationAbortedException);
}

MORDOR_UNITTEST(Socket, cancelReceive)
{
    IOManager ioManager;
    Connection conns = establishConn(ioManager);
    ioManager.schedule(boost::bind(&acceptOne, boost::ref(conns)));
    conns.connect->connect(conns.address);
    ioManager.dispatch();

    char buf[3];
    struct iovec iov;
    iov.iov_base = buf;
    iov.iov_len = 3;
    conns.connect->cancelReceive();
    MORDOR_TEST_ASSERT_EXCEPTION(conns.connect->receive(iov.iov_base, 3), OperationAbortedException);
    MORDOR_TEST_ASSERT_EXCEPTION(conns.connect->receive(&iov, 1), OperationAbortedException);
}

MORDOR_UNITTEST(Socket, sendReceive)
{
    IOManager ioManager;
    Connection conns = establishConn(ioManager);
    ioManager.schedule(boost::bind(&acceptOne, boost::ref(conns)));
    conns.connect->connect(conns.address);
    ioManager.dispatch();

    const char *sendbuf = "abcd";
    char receivebuf[5];
    memset(receivebuf, 0, 5);
    MORDOR_TEST_ASSERT_EQUAL(conns.connect->send(sendbuf, 1), 1u);
    MORDOR_TEST_ASSERT_EQUAL(conns.accept->receive(receivebuf, 1), 1u);
    MORDOR_TEST_ASSERT_EQUAL(receivebuf[0], 'a');
    receivebuf[0] = 0;
    iovec iov[2];
    iov[0].iov_base = (void *)&sendbuf[0];
    iov[1].iov_base = (void *)&sendbuf[2];
    iov[0].iov_len = 2;
    iov[1].iov_len = 2;
    MORDOR_TEST_ASSERT_EQUAL(conns.connect->send(iov, 2), 4u);
    iov[0].iov_base = &receivebuf[0];
    iov[1].iov_base = &receivebuf[2];
    MORDOR_TEST_ASSERT_EQUAL(conns.accept->receive(iov, 2), 4u);
    MORDOR_TEST_ASSERT_EQUAL(sendbuf, (const char *)receivebuf);
}

static void receiveFiber(Socket::ptr listen, size_t &sent, int &sequence)
{
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
    Socket::ptr sock = listen->accept();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 3);
    char receivebuf[5];
    memset(receivebuf, 0, 5);
    Scheduler::getThis()->yieldTo();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 5);
    MORDOR_TEST_ASSERT_EQUAL(sock->receive(receivebuf, 2), 2u);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 7);
    MORDOR_TEST_ASSERT_EQUAL((const char*)receivebuf, "ab");
    memset(receivebuf, 0, 5);
    iovec iov[2];
    iov[0].iov_base = &receivebuf[0];
    iov[1].iov_base = &receivebuf[2];
    iov[0].iov_len = 2;
    iov[1].iov_len = 2;
    Scheduler::getThis()->yieldTo();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 9);
    MORDOR_TEST_ASSERT_EQUAL(sock->receive(iov, 2), 4u);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 11);
    MORDOR_TEST_ASSERT_EQUAL((const char*)receivebuf, "abcd");

    // Let the main fiber take control again until he "blocks"
    Scheduler::getThis()->yieldTo();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 13);
    boost::shared_array<char> receivebuf2;
    if (sent > 0u) {
        receivebuf2.reset(new char[sent]);
        while (sent > 0u)
            sent -= sock->receive(receivebuf2.get(), sent);
    }
    // Let the main fiber update sent
    Scheduler::getThis()->yieldTo();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 15);

    if (sent > 0u) {
        receivebuf2.reset(new char[sent]);
        while (sent > 0u)
            sent -= sock->receive(receivebuf2.get(), sent);
    }

    // Let the main fiber take control again until he "blocks"
    Scheduler::getThis()->yieldTo();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 17);
    if (sent > 0u) {
        receivebuf2.reset(new char[std::max<size_t>(sent, 3u)]);
        iov[0].iov_base = &receivebuf2.get()[0];
        iov[1].iov_base = &receivebuf2.get()[2];
        while (sent > 0u) {
            size_t iovs = 2;
            if (sent > 2) {
                iov[1].iov_len = (iov_len_t)std::max<size_t>(sent, 3u) - 2;
            } else {
                iov[0].iov_len = (iov_len_t)sent;
                iovs = 1;
            }
            sent -= sock->receive(iov, iovs);
        }
    }
    // Let the main fiber update sent
    Scheduler::getThis()->yieldTo();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 19);

    if (sent > 0u) {
        receivebuf2.reset(new char[std::max<size_t>(sent, 3u)]);
        iov[0].iov_base = &receivebuf2.get()[0];
        iov[1].iov_base = &receivebuf2.get()[2];
        iov[0].iov_len = 2;
        iov[1].iov_len = (iov_len_t)std::max<size_t>(sent, 3u) - 2;
        while (sent > 0u)
            sent -= sock->receive(iov, 2);
    }
}

MORDOR_UNITTEST(Socket, sendReceiveForceAsync)
{
    IOManager ioManager;
    Connection conns = establishConn(ioManager);
    int sequence = 0;
    size_t sent = 0;

    Fiber::ptr otherfiber(new Fiber(boost::bind(&receiveFiber, conns.listen,
        boost::ref(sent), boost::ref(sequence))));

    ioManager.schedule(otherfiber);
    // Wait for otherfiber to "block", and return control to us
    ioManager.schedule(Fiber::getThis());
    ioManager.yieldTo();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 2);
    // Unblock him, then wait for him to block again
    conns.connect->connect(conns.address);
    ioManager.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 4);
    ioManager.schedule(otherfiber);
    ioManager.schedule(Fiber::getThis());
    ioManager.yieldTo();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 6);

    // Again
    const char *sendbuf = "abcd";
    MORDOR_TEST_ASSERT_EQUAL(conns.connect->send(sendbuf, 2), 2u);
    ioManager.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 8);
    ioManager.schedule(otherfiber);
    ioManager.schedule(Fiber::getThis());
    ioManager.yieldTo();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 10);

    // And again
    iovec iov[2];
    iov[0].iov_base = (void *)&sendbuf[0];
    iov[1].iov_base = (void *)&sendbuf[2];
    iov[0].iov_len = 2;
    iov[1].iov_len = 2;
    MORDOR_TEST_ASSERT_EQUAL(conns.connect->send(iov, 2), 4u);
    ioManager.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 12);

    char sendbuf2[65536];
    memset(sendbuf2, 1, 65536);
    // Keep sending until the other fiber said we blocked
    ioManager.schedule(otherfiber);
    while (true) {
        sent += conns.connect->send(sendbuf2, 65536);
        if (sequence == 13)
            break;
    }
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 14);
    ioManager.schedule(otherfiber);
    ioManager.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 16);
    MORDOR_TEST_ASSERT_EQUAL(sent, 0u);

    iov[0].iov_base = &sendbuf2[0];
    iov[1].iov_base = &sendbuf2[2];
    iov[1].iov_len = 65536 - 2;
    // Keep sending until the other fiber said we blocked
    ioManager.schedule(otherfiber);
    while (true) {
        sent += conns.connect->send(iov, 2);
        if (sequence == 17)
            break;
    }
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 18);
    ioManager.schedule(otherfiber);
    ioManager.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 20);
}
