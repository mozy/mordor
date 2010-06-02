#include "mordor/pch.h"

#include "mordor/string.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

MORDOR_UNITTEST(unicode, toUtf8)
{
    MORDOR_TEST_ASSERT_EQUAL(toUtf8(L'\0'), std::string("\0", 1));
    MORDOR_TEST_ASSERT_EQUAL(toUtf8(L'\x1'), "\x1");
    MORDOR_TEST_ASSERT_EQUAL(toUtf8(L'\x7f'), "\x7f");
    MORDOR_TEST_ASSERT_EQUAL(toUtf8(L'\x80'), "\xc2\x80");
    MORDOR_TEST_ASSERT_EQUAL(toUtf8(L'\x7ff'), "\xdf\xbf");
    MORDOR_TEST_ASSERT_EQUAL(toUtf8(L'\x800'), "\xe0\xa0\x80");
    MORDOR_TEST_ASSERT_EQUAL(toUtf8(L'\xffff'), "\xef\xbf\xbf");
    MORDOR_TEST_ASSERT_EQUAL(toUtf8(0x10000), "\xf0\x90\x80\x80");
    MORDOR_TEST_ASSERT_EQUAL(toUtf8(0x10ffff), "\xf4\x8f\xbf\xbf");

    MORDOR_TEST_ASSERT_EQUAL(toUtf8(L'$'), "$");
    MORDOR_TEST_ASSERT_EQUAL(toUtf8(L'\xa2'), "\xc2\xa2");
    MORDOR_TEST_ASSERT_EQUAL(toUtf8(L'\x20ac'), "\xe2\x82\xac");
    MORDOR_TEST_ASSERT_EQUAL(toUtf8(0x024b62), "\xf0\xa4\xad\xa2");
}

MORDOR_UNITTEST(unicode, surrogatePairs)
{
    MORDOR_TEST_ASSERT_EQUAL(toUtf32(L'\xd800', L'\xdc00'), 0x10000);
    MORDOR_TEST_ASSERT_EQUAL(toUtf32(L'\xdbff', L'\xdffd'), 0x10fffd);
}
