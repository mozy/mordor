// Copyright (c) 2010 - Mozy, Inc.

#include "mordor/endian.h"
#include "mordor/test/test.h"

using namespace Mordor;

MORDOR_UNITTEST(Endian, endian)
{
    MORDOR_TEST_ASSERT_EQUAL(byteswap((unsigned short)0x0123u), 0x2301u);
    MORDOR_TEST_ASSERT_EQUAL(byteswap(0x01234567u), 0x67452301u);
    MORDOR_TEST_ASSERT_EQUAL(byteswap(0x0123456789abcdefull), 0xefcdab8967452301ull);

    MORDOR_TEST_ASSERT_EQUAL(byteswap((unsigned short)0x0123u),
        byteswapOnLittleEndian(byteswapOnBigEndian((unsigned short)0x0123u)));
    MORDOR_TEST_ASSERT_EQUAL(byteswap(0x01234567u),
        byteswapOnLittleEndian(byteswapOnBigEndian(0x01234567u)));
    MORDOR_TEST_ASSERT_EQUAL(byteswap(0x0123456789abcdefull),
        byteswapOnLittleEndian(byteswapOnBigEndian(0x0123456789abcdefull)));
}
