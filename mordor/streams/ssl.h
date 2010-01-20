#ifndef __MORDOR_SSL_STREAM_H__
#define __MORDOR_SSL_STREAM_H__
// Copyright (c) 2009 - Decho Corp

#include "filter.h"

#include <vector>

#include <openssl/ssl.h>

#include "mordor/scheduler.h"

namespace Mordor {

class OpenSSLException : public std::runtime_error
{
public:
    OpenSSLException(const std::string &message)
        : std::runtime_error(message)
    {}

    OpenSSLException();   // queries OpenSSL for the error code
};

class CertificateVerificationException : public OpenSSLException
{
public:
    CertificateVerificationException(long verifyResult)
        : OpenSSLException(constructMessage(verifyResult)),
          m_verifyResult(verifyResult)
    {}
    CertificateVerificationException(long verifyResult,
        const std::string &message)
        : OpenSSLException(message),
          m_verifyResult(verifyResult)
    {}

    long verifyResult() const { return m_verifyResult; }

private:
    static std::string constructMessage(long verifyResult);

private:
    long m_verifyResult;
};

class SSLStream : public MutatingFilterStream
{
public:
    typedef boost::shared_ptr<SSLStream> ptr;

public:
    SSLStream(Stream::ptr parent, bool client = true, bool own = true, SSL_CTX *ctx = NULL);

    bool supportsHalfClose() { return false; }

    void close(CloseType type = BOTH);
    size_t read(Buffer &b, size_t len);
    size_t write(const Buffer &b, size_t len);
    void flush(bool flushParent = true);

    void accept();
    void connect();

    void verifyPeerCertificate();
    void verifyPeerCertificate(const std::string &hostname);

private:
    void flushBuffer(bool flushParent = false);
    void wantRead(FiberMutex::ScopedLock &lock);

private:
    // Need a mutex to serialize access to SSL context from both read/write
    // and to serialize reads to the underlying stream (write can cause reads)
    FiberMutex m_mutex;
    bool m_inRead;
    FiberCondition m_readCondition;
    boost::shared_ptr<SSL_CTX> m_ctx;
    boost::shared_ptr<SSL> m_ssl;
    Buffer m_readBuffer;
    BIO *m_readBio, *m_writeBio;
};

}

#endif
