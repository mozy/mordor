// Copyright (c) 2009 - Decho Corp.

#include <cassert>
#include <stdio.h>

#include "mordor/common/fiber.h"
#include "mordor/common/scheduler.h"

int main()
{
    Fiber::ptr mainFiber(new Fiber());
    WorkerPool poolA(1, true), poolB(1, false);

    printf("In pool %c\n", Scheduler::getThis() == &poolA ? 'A' : 'B');
    poolB.switchTo();
    printf("In pool %c\n", Scheduler::getThis() == &poolA ? 'A' : 'B');
    poolA.switchTo();
    printf("In pool %c\n", Scheduler::getThis() == &poolA ? 'A' : 'B');
    poolB.switchTo();
    printf("In pool %c\n", Scheduler::getThis() == &poolA ? 'A' : 'B');
    poolB.switchTo();
    printf("In pool %c\n", Scheduler::getThis() == &poolA ? 'A' : 'B');
    int x = 0;
    while (x < 10000) {
        if ((x++ % 2) == 0)
            poolA.switchTo();
        else
            poolB.switchTo();
        printf("In pool %c\n", Scheduler::getThis() == &poolA ? 'A' : 'B');
    }
    poolA.switchTo();
    printf("In pool %c\n", Scheduler::getThis() == &poolA ? 'A' : 'B');
    poolB.stop();
    printf("done\n");
    poolA.stop();
    return 0;
}
