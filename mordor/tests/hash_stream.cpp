#include "mordor/streams/buffer.h"
#include "mordor/streams/hash.h"
#include "mordor/streams/null.h"
#include "mordor/string.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

#ifndef OPENSSL_NO_SHA256
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
