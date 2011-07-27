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

// test cases from http://tools.ietf.org/html/rfc4231
MORDOR_UNITTEST(HMAC, HMAC_SHA256_RFC4231_1)
{
    std::string hmac = hmacSha256("Hi There", std::string(20, '\x0b'));

    MORDOR_TEST_ASSERT_EQUAL(
         hexstringFromData(hmac.c_str(), SHA256_DIGEST_LENGTH),
         "b0344c61d8db38535ca8afceaf0bf12b"
         "881dc200c9833da726e9376c2e32cff7");
}

MORDOR_UNITTEST(HMAC, HMAC_SHA256_RFC4231_2)
{
    std::string hmac = hmacSha256("what do ya want for nothing?", "Jefe");

    MORDOR_TEST_ASSERT_EQUAL(
        hexstringFromData(hmac.c_str(), SHA256_DIGEST_LENGTH),
        "5bdcc146bf60754e6a042426089575c7"
        "5a003f089d2739839dec58b964ec3843");
}

MORDOR_UNITTEST(HMAC, HMAC_SHA256_RFC4231_3)
{
    std::string hmac = hmacSha256(std::string(50, '\xdd'),
                                  std::string(20, '\xaa'));

    MORDOR_TEST_ASSERT_EQUAL(
        hexstringFromData(hmac.c_str(), SHA256_DIGEST_LENGTH),
        "773ea91e36800e46854db8ebd09181a7"
        "2959098b3ef8c122d9635514ced565fe");
}

MORDOR_UNITTEST(HMAC, HMAC_SHA256_RFC4231_4)
{
    std::string hmac = hmacSha256(
        std::string(50, '\xcd'),
        "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
        "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19");

    MORDOR_TEST_ASSERT_EQUAL(
        hexstringFromData(hmac.c_str(), SHA256_DIGEST_LENGTH),
        "82558a389a443c0ea4cc819899f2083a"
        "85f0faa3e578f8077a2e3ff46729665b");
}

// RFC 4231 Test Case 5 deals with output truncation, omitted

MORDOR_UNITTEST(HMAC, HMAC_SHA256_RFC4231_6)
{
    std::string hmac = hmacSha256(
        "Test Using Larger Than Block-Size Key - Hash Key First",
        std::string(131, '\xaa'));

    MORDOR_TEST_ASSERT_EQUAL(
        hexstringFromData(hmac.c_str(), SHA256_DIGEST_LENGTH),
        "60e431591ee0b67f0d8a26aacbf5b77f"
        "8e0bc6213728c5140546040f0ee37f54");
}

MORDOR_UNITTEST(HMAC, HMAC_SHA256_RFC4231_7)
{
    std::string hmac = hmacSha256(
        "This is a test using a larger than block-size key and a "
        "larger than block-size data. The key needs to be hashed "
        "before being used by the HMAC algorithm.",
        std::string(131, '\xaa'));

    MORDOR_TEST_ASSERT_EQUAL(
        hexstringFromData(hmac.c_str(), SHA256_DIGEST_LENGTH),
        "9b09ffa71b942fcb27635fbcd5b0e944"
        "bfdc63644f0713938a7f51535c3a35e2");
}
