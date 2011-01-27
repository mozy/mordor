// Copyright (c) 2010 - Mozy, Inc.

#include "mordor/atomic.h"
#include "mordor/test/test.h"

using namespace Mordor;

MORDOR_UNITTEST(Atomic, compareAndSwapNoSignExtension)
{
    intptr_t storage = 0xb2d015c0;
    MORDOR_TEST_ASSERT_EQUAL(
        atomicCompareAndSwap(storage, (intptr_t)0xb2d015c1, (intptr_t)0xb2d015c0),
        (intptr_t)0xb2d015c0);
}
