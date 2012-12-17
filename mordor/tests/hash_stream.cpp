#include "mordor/streams/buffer.h"
#include "mordor/streams/hash.h"
#include "mordor/streams/null.h"
#include "mordor/string.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

MORDOR_UNITTEST(MD5Stream, empty)
{
    HashStream::ptr hashStream(new MD5Stream(NullStream::get_ptr()));
    MORDOR_TEST_ASSERT_EQUAL(hashStream->hashSize(), (size_t)MD5_DIGEST_LENGTH);
    std::string hexHash = hexstringFromData(hashStream->hash());
    MORDOR_TEST_ASSERT_EQUAL(hexHash,
        std::string("d41d8cd98f00b204e9800998ecf8427e"));
}

MORDOR_UNITTEST(MD5Stream, knownMD5)
{
    HashStream::ptr hashStream(new MD5Stream(NullStream::get_ptr()));
    std::string data("The quick brown fox jumps over the lazy dog");
    hashStream->write(data, data.size());
    std::string hexHash = hexstringFromData(hashStream->hash());
    MORDOR_TEST_ASSERT_EQUAL(hexHash,
        std::string("9e107d9d372bb6826bd81d3542a419d6"));
}


#ifndef OPENSSL_NO_SHA0
MORDOR_UNITTEST(SHA0Stream, empty)
{
    HashStream::ptr hashStream(new SHA0Stream(NullStream::get_ptr()));
    MORDOR_TEST_ASSERT_EQUAL(hashStream->hashSize(), (size_t)SHA_DIGEST_LENGTH);
    std::string hexHash = hexstringFromData(hashStream->hash());
    MORDOR_TEST_ASSERT_EQUAL(hexHash,
        std::string("f96cea198ad1dd5617ac084a3d92c6107708c0ef"));
}

MORDOR_UNITTEST(SHA0Stream, knownSha0)
{
    HashStream::ptr hashStream(new SHA0Stream(NullStream::get_ptr()));
    std::string data("The quick brown fox jumps over the lazy dog");
    hashStream->write(data, data.size());
    std::string hexHash = hexstringFromData(hashStream->hash());
    MORDOR_TEST_ASSERT_EQUAL(hexHash,
        std::string("b03b401ba92d77666221e843feebf8c561cea5f7"));
}
#endif

#ifndef OPENSSL_NO_SHA1
MORDOR_UNITTEST(SHA1Stream, empty)
{
    HashStream::ptr hashStream(new SHA1Stream(NullStream::get_ptr()));
    MORDOR_TEST_ASSERT_EQUAL(hashStream->hashSize(), (size_t)SHA_DIGEST_LENGTH);
    std::string hexHash = hexstringFromData(hashStream->hash());
    MORDOR_TEST_ASSERT_EQUAL(hexHash,
        std::string("da39a3ee5e6b4b0d3255bfef95601890afd80709"));
}

MORDOR_UNITTEST(SHA1Stream, knownSha1)
{
    HashStream::ptr hashStream(new SHA1Stream(NullStream::get_ptr()));
    std::string data("The quick brown fox jumps over the lazy dog");
    hashStream->write(data, data.size());
    std::string hexHash = hexstringFromData(hashStream->hash());
    MORDOR_TEST_ASSERT_EQUAL(hexHash,
        std::string("2fd4e1c67a2d28fced849ee1bb76e7391b93eb12"));
}
#endif

#ifndef OPENSSL_NO_SHA256
MORDOR_UNITTEST(SHA224Stream, empty)
{
    HashStream::ptr hashStream(new SHA224Stream(NullStream::get_ptr()));
    MORDOR_TEST_ASSERT_EQUAL(hashStream->hashSize(), (size_t)SHA224_DIGEST_LENGTH);
    std::string hexHash = hexstringFromData(hashStream->hash());
    MORDOR_TEST_ASSERT_EQUAL(hexHash,
        std::string("d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f"));
}

MORDOR_UNITTEST(SHA224Stream, knownSha224)
{
    HashStream::ptr hashStream(new SHA224Stream(NullStream::get_ptr()));
    std::string data("The quick brown fox jumps over the lazy dog");
    hashStream->write(data, data.size());
    std::string hexHash = hexstringFromData(hashStream->hash());
    MORDOR_TEST_ASSERT_EQUAL(hexHash,
        std::string("730e109bd7a8a32b1cb9d9a09aa2325d2430587ddbc0c38bad911525"));
}

MORDOR_UNITTEST(SHA256Stream, empty)
{
    HashStream::ptr hashStream(new SHA256Stream(NullStream::get_ptr()));
    MORDOR_TEST_ASSERT_EQUAL(hashStream->hashSize(), (size_t)SHA256_DIGEST_LENGTH);
    std::string hexHash = hexstringFromData(hashStream->hash());
    MORDOR_TEST_ASSERT_EQUAL(hexHash,
        std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
}

MORDOR_UNITTEST(SHA256Stream, knownSha256)
{
    HashStream::ptr hashStream(new SHA256Stream(NullStream::get_ptr()));
    std::string data("The quick brown fox jumps over the lazy dog");
    hashStream->write(data, data.size());
    std::string hexHash = hexstringFromData(hashStream->hash());
    MORDOR_TEST_ASSERT_EQUAL(hexHash,
        std::string("d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592"));
}

MORDOR_UNITTEST(SHA256Stream, resumeShaCtx)
{
    HashStream::ptr tempStream(new SHA256Stream(NullStream::get_ptr()));
    std::string data("The quick brown fox jumps over the lazy dog");
    tempStream->write(data, data.size());
    SHA256_CTX ctx = boost::static_pointer_cast<SHA256Stream>(tempStream)->ctx();
    tempStream.reset();

    HashStream::ptr hashStream(new SHA256Stream(NullStream::get_ptr(), ctx));
    hashStream->write(".", 1);
    std::string hexHash = hexstringFromData(hashStream->hash());
    MORDOR_TEST_ASSERT_EQUAL(hexHash,
        std::string("ef537f25c895bfa782526529a9b63d97aa631564d5d789c2b765448c8635fb6c"));

}

MORDOR_UNITTEST(SHA256Stream, dumpContextAndResume)
{
    HashStream::ptr tempStream(new SHA256Stream(NullStream::get_ptr()));
    std::string data("The quick brown fox jumps over the lazy dog");
    tempStream->write(data, data.size());
    Buffer buffer = tempStream->dumpContext();
    tempStream.reset();

    HashStream::ptr hashStream(new SHA256Stream(NullStream::get_ptr(), buffer));
    hashStream->write(".", 1);
    std::string hexHash = hexstringFromData(hashStream->hash());
    MORDOR_TEST_ASSERT_EQUAL(hexHash,
        std::string("ef537f25c895bfa782526529a9b63d97aa631564d5d789c2b765448c8635fb6c"));
}

#endif
