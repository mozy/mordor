#ifndef __SSL_STREAM_H__
#define __SSL_STREAM_H__
// Copyright (c) 2009 - Decho Corp

#include "filter.h"

#include <vector>

#include <openssl/ssl.h>

class OpenSSLException : public std::runtime_error
{
public:
    OpenSSLException()
        : std::runtime_error(constructMessage())
    {}

private:
    static std::string constructMessage();
};

class SSLStream : public MutatingFilterStream
{
public:
    SSLStream(Stream::ptr parent, bool client = true, bool own = true);

    void close(CloseType type = BOTH);
    size_t read(Buffer &b, size_t len);
    size_t write(const Buffer &b, size_t len);
    void flush();

    void accept();
    void connect();

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
