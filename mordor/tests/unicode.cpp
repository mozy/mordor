#include "mordor/pch.h"

#include "mordor/string.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

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

#if defined(WINDOWS) || defined(OSX) || defined(HAVE_ICONV)

MORDOR_UNITTEST(Unicode, badUtf8Exception)
{
    MORDOR_TEST_ASSERT_EXCEPTION(toUtf16("\xc0\xc1"), InvalidUnicodeException);
}

#endif

#if defined(OSX)
MORDOR_UNITTEST(Unicode, toUtf8OSX)
{
    // Two characters, each requiring 3 bytes for utf8 representation (see rm122624)
    unsigned char chinese[] = {0xE4, 0xBD, 0xA0, 0xE5, 0xA5, 0xBD, 0};
    CFStringRef cfStr = CFStringCreateWithCString(kCFAllocatorDefault, (const char *)chinese, kCFStringEncodingUTF8);
    std::string converted = toUtf8(cfStr);
    MORDOR_TEST_ASSERT_EQUAL(converted, std::string((const char *)chinese));
    CFRelease(cfStr);
}
#endif
