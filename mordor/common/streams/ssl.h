#ifndef __SSL_STREAM_H__
#define __SSL_STREAM_H__
// Copyright (c) 2009 - Decho Corp

#include "filter.h"

#include <vector>

#include <openssl/ssl.h>

class OpenSSLException : public std::runtime_error
{
public:
    OpenSSLException(const std::string &message, const char *function = NULL)
        : std::runtime_error(message),
          m_function(function)
    {}

    const char *function() const;

private:
    const char *m_function;
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

    void close(CloseType type = BOTH);
    size_t read(Buffer &b, size_t len);
    size_t write(const Buffer &b, size_t len);
    void flush();

    void accept();
    void connect();

    void verifyPeerCertificate();
    void verifyPeerCertificate(const std::string &hostname);

private:
    void flushBuffer();
    void wantRead();

private:
    boost::shared_ptr<SSL_CTX> m_ctx;
    boost::shared_ptr<SSL> m_ssl;
    Buffer m_readBuffer;
    BIO *m_readBio, *m_writeBio;
};

#endif
