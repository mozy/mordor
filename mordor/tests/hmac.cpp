#include "mordor/pch.h"

#include "mordor/string.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

MORDOR_UNITTEST(HMAC, HMAC_MD5_1)
{
    std::string key;
    key.append(16, 0x0b);
    std::string data("Hi There");
    MORDOR_TEST_ASSERT(hmacMd5(data, key) ==
        "\x92\x94\x72\x7a\x36\x38\xbb\x1c\x13\xf4\x8e\xf8\x15\x8b\xfc\x9d");
}

MORDOR_UNITTEST(HMAC, HMAC_MD5_2)
{
    std::string key("Jefe");
    std::string data("what do ya want for nothing?");
    MORDOR_TEST_ASSERT(hmacMd5(data, key) ==
        "\x75\x0c\x78\x3e\x6a\xb0\xb5\x03\xea\xa8\x6e\x31\x0a\x5d\xb7\x38");
}

MORDOR_UNITTEST(HMAC, HMAC_MD5_3)
{
    std::string key;
    key.append(16, 0xaa);
    std::string data;
    data.append(50, 0xdd);
    MORDOR_TEST_ASSERT(hmacMd5(data, key) ==
        "\x56\xbe\x34\x52\x1d\x14\x4c\x88\xdb\xb8\xc7\x33\xf0\xe8\xb3\xf6");
}
