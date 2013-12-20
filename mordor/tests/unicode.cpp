#include "mordor/pch.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "mordor/string.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

// TODO: use C++11 string literals for UTF-{8,16,32} strings
MORDOR_UNITTEST(Unicode, toUtf8)
{
    MORDOR_TEST_ASSERT_EQUAL(toUtf8((utf16char)L'\0'), std::string("\0", 1));
    MORDOR_TEST_ASSERT_EQUAL(toUtf8((utf16char)L'\x1'), "\x1");
    MORDOR_TEST_ASSERT_EQUAL(toUtf8((utf16char)L'\x7f'), "\x7f");
    MORDOR_TEST_ASSERT_EQUAL(toUtf8((utf16char)L'\x80'), "\xc2\x80");
    MORDOR_TEST_ASSERT_EQUAL(toUtf8((utf16char)L'\x7ff'), "\xdf\xbf");
    MORDOR_TEST_ASSERT_EQUAL(toUtf8((utf16char)L'\x800'), "\xe0\xa0\x80");
    MORDOR_TEST_ASSERT_EQUAL(toUtf8((utf16char)L'\xffff'), "\xef\xbf\xbf");
    MORDOR_TEST_ASSERT_EQUAL(toUtf8((utf32char)0x10000), "\xf0\x90\x80\x80");
    MORDOR_TEST_ASSERT_EQUAL(toUtf8((utf32char)0x10ffff), "\xf4\x8f\xbf\xbf");

    MORDOR_TEST_ASSERT_EQUAL(toUtf8((utf16char)L'$'), "$");
    MORDOR_TEST_ASSERT_EQUAL(toUtf8((utf16char)L'\xa2'), "\xc2\xa2");
    MORDOR_TEST_ASSERT_EQUAL(toUtf8((utf16char)L'\x20ac'), "\xe2\x82\xac");
    MORDOR_TEST_ASSERT_EQUAL(toUtf8((utf32char)0x024b62), "\xf0\xa4\xad\xa2");
}

MORDOR_UNITTEST(Unicode, surrogatePairs)
{
    MORDOR_TEST_ASSERT_EQUAL(toUtf32(L'\xd800', L'\xdc00'), 0x10000);
    MORDOR_TEST_ASSERT_EQUAL(toUtf32(L'\xdbff', L'\xdffd'), 0x10fffd);
}

#if defined(WINDOWS) || defined(OSX) || defined(HAVE_ICU)
MORDOR_UNITTEST(Unicode, badUtf8Exception)
{
    MORDOR_TEST_ASSERT_EXCEPTION(toUtf16("\xc0\xc1"), InvalidUnicodeException);
}

MORDOR_UNITTEST(Unicode, toUtf16)
{
    std::wstring wide(L"\x4eba\x8270\x4e0d\x62c6");
    utf16string utf16(wide.length(), ' ');
    utf16.assign(wide.begin(), wide.end());
    MORDOR_TEST_ASSERT(toUtf16(std::string("\xe4\xba\xba\xe8\x89\xb0\xe4\xb8\x8d\xe6\x8b\x86")) == utf16);
}

#endif
