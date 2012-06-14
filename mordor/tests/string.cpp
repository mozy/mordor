// Copyright (c) 2010 - Mozy, Inc.

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

MORDOR_UNITTEST(String, base64decode)
{
    MORDOR_TEST_ASSERT_EQUAL(base64decode("H+LksF7FISl/Sw=="),
        "\x1f\xe2\xe4\xb0^\xc5!)\x7fK");
    MORDOR_TEST_ASSERT_EQUAL(base64decode("H-LksF7FISl_Sw==", "-_"),
        "\x1f\xe2\xe4\xb0^\xc5!)\x7fK");
    MORDOR_TEST_ASSERT_EQUAL(urlsafeBase64decode("H-LksF7FISl_Sw=="),
        "\x1f\xe2\xe4\xb0^\xc5!)\x7fK");
}

MORDOR_UNITTEST(String, base64encode)
{
    MORDOR_TEST_ASSERT_EQUAL(base64encode("\x1f\xe2\xe4\xb0^\xc5!)\x7fK"),
        "H+LksF7FISl/Sw==");
    MORDOR_TEST_ASSERT_EQUAL(base64encode("\x1f\xe2\xe4\xb0^\xc5!)\x7fK",
        "-_"), "H-LksF7FISl_Sw==");
    MORDOR_TEST_ASSERT_EQUAL(urlsafeBase64encode(
        "\x1f\xe2\xe4\xb0^\xc5!)\x7fK"), "H-LksF7FISl_Sw==");
}
