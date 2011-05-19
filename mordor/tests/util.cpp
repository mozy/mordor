// Copyright (c) 2009 - Mozy, Inc.

#include <boost/bind.hpp>

#include "mordor/util.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;


MORDOR_UNITTEST(Util, muldiv64)
{
    MORDOR_TEST_ASSERT_EQUAL(muldiv64(0, 1, 1), 0ULL);
    MORDOR_TEST_ASSERT_EQUAL(muldiv64(1, 0, 2), 0ULL);
    MORDOR_TEST_ASSERT_EQUAL(muldiv64(1, 2, 3), 0ULL);
    MORDOR_TEST_ASSERT_EQUAL(muldiv64(3, 2, 1), 6ULL);
    MORDOR_TEST_ASSERT_EQUAL(muldiv64(43849324ULL, 11289432, 65463), 7562042093ULL);
    
    // this computation would overflow without muldiv64()
    MORDOR_TEST_ASSERT_EQUAL(muldiv64(0x1111111111111111ULL, 5000000, 1000000), 0x5555555555555555ULL);

}
