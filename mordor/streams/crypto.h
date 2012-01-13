#ifndef __MORDOR_CRYPTO_STREAM__
#define __MORDOR_CRYPTO_STREAM__
// Copyright (c) 2011 - Mozy, Inc.

#include <openssl/evp.h>
#include "filter.h"
#include "buffer.h"

namespace Mordor {

// encryption/decryption stream using OpenSSL EVP API
// supports all four permutations of (encrypt, decrypt) and (read, write),
// although only one per instance

class CryptoStream : public MutatingFilterStream
{
public:
    typedef boost::shared_ptr<CryptoStream> ptr;
    static const std::string RANDOM_IV;

    enum Direction {
        INFER,  // check whether parent supportsRead() or supportsWrite();
                // exactly one of these must be true
        READ,
        WRITE
    };
    enum Operation {
        AUTO,   // encrypt on WRITE; decrypt on READ
        DECRYPT,
        ENCRYPT
    };

    // cipher is e.g. EVP_aes_256_cbc() (see openssl/evp.h for all options)
    // key and iv are *binary* and must be the correct length for the cipher
    // if iv is required and not supplied (or set to RANDOM_IV),
    // then the initialization vector will:
    //  * on encrypt, generated randomly and prepended to the ciphertext
    //  * on decrypt, extracted from the beginning of the ciphertext
    // WARNING: due to older versions of OpenSSL reporting a nonzero IV size
    //   for ECB cipher contexts, you should explicitly supply an empty IV
    //   instead of RANDOM_IV when operating in ECB mode
    CryptoStream(Stream::ptr parent, const EVP_CIPHER *cipher, const std::string &key,
        const std::string &iv = RANDOM_IV, Direction = INFER, Operation = AUTO,
        bool own = true);
    ~CryptoStream();

    bool supportsRead() { return m_dir == READ; }
    bool supportsWrite() { return m_dir == WRITE; }
    bool supportsSeek() { return false; }
    bool supportsSize() { return false; }

    void close(CloseType type = BOTH);
    using MutatingFilterStream::read;
    size_t read(Buffer &buffer, size_t len);
    using MutatingFilterStream::write;
    size_t write(const Buffer &buffer, size_t len);

private:
    size_t cipher(const Buffer &src, Buffer &dst, size_t len, size_t skip = 0);    // ciphers len bytes from src
    size_t final(Buffer &dst);  // finalizes the cipher and writes the last few bytes to dst
    void write_buffer(Buffer &buffer);    // writes and consumes entire buffer
    void init_iv();
    void finalize();

    std::string m_iv;
    Direction m_dir;
    Operation m_op;
    Buffer m_buf, m_tmp;
    EVP_CIPHER_CTX m_ctx;
    int m_blocksize;
    bool m_eof;
    size_t m_iv_to_extract;
};

}

#endif
