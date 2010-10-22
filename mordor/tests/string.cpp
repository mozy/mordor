// Copyright (c) 2010 - Decho Corporation

#include "mordor/string.h"
#include "mordor/test/test.h"

using namespace Mordor;

MORDOR_UNITTEST(String, dataFromHexstring)
{
    MORDOR_TEST_ASSERT_EQUAL(dataFromHexstring(""), "");
    MORDOR_TEST_ASSERT_EQUAL(dataFromHexstring("00"), std::string("\0", 1));
    MORDOR_TEST_ASSERT_EQUAL(dataFromHexstring("abcd"), "\xab\xcd");
    MORDOR_TEST_ASSERT_EQUAL(dataFromHexstring("01eF"), "\x01\xef");
    MORDOR_TEST_ASSERT_EXCEPTION(dataFromHexstring("0"),
        std::invalid_argument);
    MORDOR_TEST_ASSERT_EXCEPTION(dataFromHexstring("fg"),
        std::invalid_argument);
    MORDOR_TEST_ASSERT_EXCEPTION(dataFromHexstring("fG"),
        std::invalid_argument);
    MORDOR_TEST_ASSERT_EXCEPTION(dataFromHexstring(std::string("\0\0", 2)),
        std::invalid_argument);
}
