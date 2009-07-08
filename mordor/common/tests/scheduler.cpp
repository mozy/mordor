// Copyright (c) 2009 - Decho Corp.

#include "mordor/test/test.h"

#include "mordor/common/scheduler.h"

TEST_WITH_SUITE(Scheduler, switcherExceptions)
{
    Fiber::ptr mainFiber(new Fiber());
    WorkerPool poolA, poolB(1, false);

    TEST_ASSERT_EQUAL(Scheduler::getThis(), &poolA);
    TEST_ASSERT_NOT_EQUAL(Scheduler::getThis(), &poolB);
    
    TEST_ASSERT_EXCEPTION({
        SchedulerSwitcher switcher(&poolB);
        TEST_ASSERT_EQUAL(Scheduler::getThis(), &poolB);
        TEST_ASSERT_NOT_EQUAL(Scheduler::getThis(), &poolA);
        throw std::runtime_error("pass through context switch");
    }, std::runtime_error);

    TEST_ASSERT_EQUAL(Scheduler::getThis(), &poolA);
    TEST_ASSERT_NOT_EQUAL(Scheduler::getThis(), &poolB);
    poolB.stop();
    poolA.stop();
}
