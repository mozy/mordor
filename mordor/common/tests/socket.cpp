// Copyright (c) 2009 - Decho Corp.
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
    Socket::ptr s = addresses.front()->createSocket(ioManager);
    s->receiveTimeout(1000000);
    s->bind(addresses.front());
    s->listen();
    unsigned long long start = TimerManager::now();
    TEST_ASSERT_EXCEPTION(s->accept(), OperationAbortedException);
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
