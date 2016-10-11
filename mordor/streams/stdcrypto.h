#ifndef __MORDOR_STDCRYPTO_STREAM__
#define __MORDOR_STDCRYPTO_STREAM__
// Copyright (c) 2016 - Mozy, Inc.

#include "buffer.h"
#include "filter.h"

namespace Mordor {

// encryption/decryption stream compatible with openssl implementation
// * only need passphrase as input parameter, key and iv will be derived from the passphrase
// * optional salt can be generated as well
class StdCryptoStream : public MutatingFilterStream
{
public:
    typedef boost::shared_ptr<StdCryptoStream> ptr;

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

    StdCryptoStream(Stream::ptr parent, const EVP_CIPHER *cipher, const std::string &pass,
        bool nosalt = false, Direction = INFER, Operation = AUTO, bool own = true);
    ~StdCryptoStream() {};

    bool supportsRead() { return m_dir == READ; }
    bool supportsWrite() { return m_dir == WRITE; }
    bool supportsSeek() { return false; }
    bool supportsSize() { return false; }

    void close(CloseType type = BOTH);
    void flush(bool flushParent = true);
    using MutatingFilterStream::read;
    size_t read(Buffer &buffer, size_t len);
    using MutatingFilterStream::write;
    size_t write(const Buffer &buffer, size_t len);

private:
    Stream::ptr buildCryptoStream(Stream::ptr p, const std::string &salt);

private:
    Buffer m_buffer;
    const EVP_CIPHER * m_cipher;
    const std::string m_pass;
    bool m_nosalt;
    Direction m_dir;
    Operation m_op;
    bool m_extractSalt;
    Stream::ptr m_cryptoStream;
};

}

#endif
