#ifndef __SSL_STREAM_H__
#define __SSL_STREAM_H__
// Copyright (c) 2009 - Decho Corp

#include "openssl.h"

#include <openssl/ssl.h>

class SSLStream : public OpenSSLStream
{
public:
    SSLStream(Stream::ptr parent, bool client = true, bool own = true);
    ~SSLStream();

    void close(CloseType type = BOTH);
    size_t read(Buffer &b, size_t len);
    size_t write(const Buffer &b, size_t len);

private:
    SSL_CTX *m_ctx;
    SSL *m_ssl;
};

#endif
