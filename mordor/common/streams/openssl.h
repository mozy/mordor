#ifndef __OPENSSL_STREAM_H__
#define __OPENSSL_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include <openssl/bio.h>

#include "stream.h"

class OpenSSLStream : public Stream
{
public:
    OpenSSLStream(BIO *bio, bool own = true);
    ~OpenSSLStream();

    bool supportsRead() { return true; }
    bool supportsWrite() { return true; }
    bool supportsSeek() { return true; }

    BIO *parent() { return m_bio; }

    void close(CloseType type = BOTH);
    size_t read(Buffer &b, size_t len);
    size_t write(const Buffer &b, size_t len);
    long long seek(long long offset, Anchor anchor);
    void flush();

protected:
    void parent(BIO *bio) { m_bio = bio; }
    void ownsParent(bool own) { m_own = own; }

private:
    BIO *m_bio;
    bool m_own;
};

#define BIO_TYPE_STREAM    (100|0x0400)

#define BIO_C_SET_STREAM   1000
#define BIO_C_GET_STREAM   1001

extern BIO_METHOD *BIO_s_stream();
extern BIO *BIO_new_stream(Stream::ptr parent, bool own);
#define BIO_set_stream(b,stream,c) BIO_ctrl(b, BIO_C_SET_STREAM, c, &stream);
#define BIO_get_stream(b,stream) BIO_ctrl(b, BIO_C_GET_STREAM, 0, stream);

#endif
