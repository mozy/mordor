// Copyright (c) 2016 - Mozy, Inc.
#include <openssl/evp.h>

#include "mordor/streams/stdcrypto.h"
#include "mordor/streams/hash.h"
#include "mordor/streams/limited.h"
#include "mordor/streams/memory.h"
#include "mordor/streams/null.h"
#include "mordor/streams/random.h"
#include "mordor/streams/ssl.h"
#include "mordor/streams/transfer.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

static const std::string passphrase("01234567");

static const unsigned char plaintext[54] = {
0x44, 0x6f, 0x6e, 0x27, 0x74, 0x20, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x20, 0x79, 0x6f, 0x75, 0x72,
0x20, 0x63, 0x68, 0x69, 0x63, 0x6b, 0x65, 0x6e, 0x73, 0x20, 0x62, 0x65, 0x66, 0x6f, 0x72, 0x65,
0x20, 0x74, 0x68, 0x65, 0x20, 0x65, 0x67, 0x67, 0x73, 0x20, 0x68, 0x61, 0x76, 0x65, 0x20, 0x68,
0x61, 0x74, 0x63, 0x68, 0x65, 0x64
};

// openssl enc -e -aes-256-cbc -in test.txt -out test.enc -nosalt -pass pass:01234567
static const unsigned char ciphertext[64] = {
0x38, 0xea, 0xa0, 0x67, 0xfc, 0x79, 0x04, 0xd6, 0xb3, 0xea, 0x43, 0x51, 0x4d, 0x54, 0x4a, 0x7f,
0x40, 0xd8, 0x48, 0xc0, 0xd7, 0x1d, 0x47, 0x38, 0x9c, 0x7e, 0x56, 0xd9, 0x6d, 0xf2, 0x06, 0xe4,
0x7d, 0x0a, 0x6f, 0x55, 0x0d, 0xcf, 0xc5, 0xff, 0x1b, 0x0b, 0x22, 0x83, 0xe7, 0x08, 0xd8, 0x68,
0xec, 0x7d, 0x2c, 0x49, 0x1d, 0x1e, 0x2f, 0x38, 0x26, 0x9a, 0xcb, 0x98, 0x45, 0x5b, 0x0b, 0x28
};

// openssl enc -e -aes-256-cbc -in test.txt -out test.enc -pass pass:01234567
static const unsigned char ciphertext_s1[80] = {
0x53, 0x61, 0x6c, 0x74, 0x65, 0x64, 0x5f, 0x5f, 0x98, 0xdc, 0x38, 0xac, 0xe1, 0x6a, 0x82, 0xa4,
0x56, 0x66, 0x68, 0x5b, 0xb9, 0x28, 0xda, 0xd0, 0x11, 0x79, 0x11, 0x7f, 0x17, 0xb8, 0x97, 0x90,
0x01, 0xe5, 0x76, 0xa5, 0xb6, 0x0a, 0x4c, 0x1c, 0x8c, 0xc8, 0x21, 0xd8, 0x9f, 0x6d, 0x8f, 0x1a,
0x1b, 0xa0, 0x5d, 0x91, 0xcd, 0x11, 0x40, 0x57, 0xce, 0xcb, 0x91, 0x7e, 0x30, 0xc4, 0x64, 0x70,
0xb0, 0xf1, 0xbc, 0xad, 0xee, 0x62, 0x43, 0xca, 0x2a, 0xed, 0xd3, 0xa6, 0x53, 0x3f, 0xb2, 0x7f
};

// openssl enc -e -aes-256-cbc -in test.txt -out test.enc -pass pass:01234567
// same command, different salt
static const unsigned char ciphertext_s2[80] = {
0x53, 0x61, 0x6c, 0x74, 0x65, 0x64, 0x5f, 0x5f, 0x4b, 0xec, 0xea, 0x5b, 0x85, 0xfa, 0xfc, 0xeb,
0x1a, 0xb7, 0xba, 0x22, 0xf3, 0x56, 0x66, 0x44, 0x7a, 0x4c, 0xe3, 0xad, 0x32, 0x2f, 0x4d, 0xb0,
0x43, 0xd3, 0x5e, 0xcd, 0xff, 0x66, 0x52, 0xa7, 0x65, 0xd8, 0xb6, 0x6e, 0xb0, 0x97, 0x92, 0xa6,
0x08, 0x5d, 0x59, 0xeb, 0xef, 0xf6, 0x0d, 0x69, 0xd5, 0x63, 0x66, 0x61, 0xdd, 0x57, 0x9f, 0xca,
0x1d, 0xc7, 0xcc, 0xd7, 0x10, 0xc2, 0xbe, 0xce, 0xa9, 0xc6, 0x51, 0xb1, 0x50, 0x06, 0x3f, 0x41
};

MORDOR_UNITTEST(StdCryptoStream, encryptWriteNoSalt)
{
    Buffer plain;
    plain.copyIn(plaintext, sizeof(plaintext));
    Buffer cipher;
    cipher.copyIn(ciphertext, sizeof(ciphertext));

    MemoryStream src(plain);
    MemoryStream::ptr sink(new MemoryStream);
    StdCryptoStream cryptor(sink, EVP_aes_256_cbc(), passphrase, true, StdCryptoStream::WRITE);
    transferStream(src, cryptor);
    cryptor.close();
    const Buffer &test = sink->buffer();
    MORDOR_TEST_ASSERT(test == cipher);
}

MORDOR_UNITTEST(StdCryptoStream, encryptReadNoSalt)
{
    Buffer plain;
    plain.copyIn(plaintext, sizeof(plaintext));
    Buffer cipher;
    cipher.copyIn(ciphertext, sizeof(ciphertext));

    MemoryStream::ptr src(new MemoryStream(plain));
    StdCryptoStream cryptor(src, EVP_aes_256_cbc(), passphrase, true, StdCryptoStream::READ, StdCryptoStream::ENCRYPT);
    MemoryStream sink;
    transferStream(cryptor, sink);
    const Buffer &test = sink.buffer();
    MORDOR_TEST_ASSERT(test == cipher);
}

MORDOR_UNITTEST(StdCryptoStream, decryptWriteNoSalt)
{
    Buffer plain;
    plain.copyIn(plaintext, sizeof(plaintext));
    Buffer cipher;
    cipher.copyIn(ciphertext, sizeof(ciphertext));

    MemoryStream src(cipher);
    MemoryStream::ptr sink(new MemoryStream);
    StdCryptoStream decryptor(sink, EVP_aes_256_cbc(), passphrase, true, StdCryptoStream::WRITE, StdCryptoStream::DECRYPT);
    transferStream(src, decryptor);
    decryptor.close();

    const Buffer &test = sink->buffer();
    MORDOR_TEST_ASSERT(test == plain);
}

MORDOR_UNITTEST(StdCryptoStream, decryptReadNoSalt)
{
    Buffer plain;
    plain.copyIn(plaintext, sizeof(plaintext));
    Buffer cipher;
    cipher.copyIn(ciphertext, sizeof(ciphertext));

    MemoryStream::ptr src(new MemoryStream(cipher));
    StdCryptoStream decryptor(src, EVP_aes_256_cbc(), passphrase, true, StdCryptoStream::READ);
    MemoryStream sink;
    transferStream(decryptor, sink);
    const Buffer &test = sink.buffer();
    MORDOR_TEST_ASSERT(test == plain);
}

MORDOR_UNITTEST(StdCryptoStream, decryptWriteWithSalt)
{
    Buffer plain;
    plain.copyIn(plaintext, sizeof(plaintext));
    Buffer cipher;
    cipher.copyIn(ciphertext_s1, sizeof(ciphertext_s1));

    MemoryStream src(cipher);
    MemoryStream::ptr sink(new MemoryStream);
    StdCryptoStream decryptor(sink, EVP_aes_256_cbc(), passphrase, false, StdCryptoStream::WRITE, StdCryptoStream::DECRYPT);
    transferStream(src, decryptor);
    decryptor.close();

    const Buffer &test = sink->buffer();
    MORDOR_TEST_ASSERT(test == plain);
}

MORDOR_UNITTEST(StdCryptoStream, decryptWriteWithSaltEofException)
{
    // empty source stream can't be decrypted with salt
    MemoryStream src;
    MemoryStream::ptr sink(new MemoryStream);
    StdCryptoStream decryptor(sink, EVP_aes_256_cbc(), passphrase, false, StdCryptoStream::WRITE, StdCryptoStream::DECRYPT);
    transferStream(src, decryptor);
    MORDOR_TEST_ASSERT_EXCEPTION(decryptor.close(), UnexpectedEofException);
}

MORDOR_UNITTEST(StdCryptoStream, decryptReadWithSalt)
{
    Buffer plain;
    plain.copyIn(plaintext, sizeof(plaintext));
    Buffer cipher;
    cipher.copyIn(ciphertext_s1, sizeof(ciphertext_s2));

    MemoryStream::ptr src(new MemoryStream(cipher));
    StdCryptoStream decryptor(src, EVP_aes_256_cbc(), passphrase, false, StdCryptoStream::READ);
    MemoryStream sink;
    transferStream(decryptor, sink);
    const Buffer &test = sink.buffer();
    MORDOR_TEST_ASSERT(test == plain);
}

// test the stream in all four modes of operation
// hashDecR <- decR <- hashEncR <- encR <- hashOrig <- random
// encW -> hashEncW -> decW -> hashDecW -> null
static void TestStreaming(long long test_bytes, bool nosalt)
{
    // read side
    Stream::ptr random(new RandomStream);
    Stream::ptr source(new LimitedStream(random, test_bytes));
    HashStream::ptr hashOrig(new MD5Stream(source));
    Stream::ptr encR(
        new StdCryptoStream(hashOrig, EVP_aes_256_cbc(), passphrase, nosalt, StdCryptoStream::READ, StdCryptoStream::ENCRYPT));
    HashStream::ptr hashEncR(new MD5Stream(encR));
    Stream::ptr decR(
        new StdCryptoStream(hashEncR, EVP_aes_256_cbc(), passphrase, nosalt, StdCryptoStream::READ, StdCryptoStream::DECRYPT));
    HashStream::ptr hashDecR(new MD5Stream(decR));

    // write side
    HashStream::ptr hashDecW(new MD5Stream(NullStream::get_ptr()));
    Stream::ptr decW(
        new StdCryptoStream(hashDecW, EVP_aes_256_cbc(), passphrase, nosalt, StdCryptoStream::WRITE, StdCryptoStream::DECRYPT));
    HashStream::ptr hashEncW(new MD5Stream(decW));
    Stream::ptr encW(
        new StdCryptoStream(hashEncW, EVP_aes_256_cbc(), passphrase, nosalt, StdCryptoStream::WRITE, StdCryptoStream::ENCRYPT));

    transferStream(hashDecR, encW);
    encW->close();
    decW->close();

    // make sure the decrypted data matches the original, and does _not_ match the encrypted
    // (because otherwise any non-mutating filter stream would pass this test...)
    MORDOR_TEST_ASSERT(hashOrig->hash() != hashEncR->hash());
    MORDOR_TEST_ASSERT(hashOrig->hash() == hashDecR->hash());
    MORDOR_TEST_ASSERT(hashOrig->hash() == hashDecW->hash());
}

MORDOR_UNITTEST(StdCryptoStream, streaming)
{
    static const long long sizes[] = { 0, 1, 15, 16, 17, 131071, 131072, 131073 };
    size_t nsizes = sizeof(sizes) / sizeof(sizes[0]);

    for(size_t i = 0; i < nsizes; ++i) {
        TestStreaming(sizes[i], true);
        TestStreaming(sizes[i], false);
    }
}
