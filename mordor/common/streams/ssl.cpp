// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "ssl.h"

#include "mordor/common/version.h"

#ifdef MSVC
#pragma comment(lib, "libeay32")
#pragma comment(lib, "ssleay32")
#endif

struct SSLInitializer {
    SSLInitializer()
    {
        SSL_library_init();
    }
};

static SSLInitializer g_init;

SSLStream::SSLStream(Stream::ptr parent, bool client, bool own)
: OpenSSLStream(BIO_new_stream(parent, own))
{
    ASSERT(parent);
    m_ctx = SSL_CTX_new(client ? SSLv23_client_method() :
        SSLv23_server_method());
    ASSERT(m_ctx);
    // TODO: exception
    m_ssl = SSL_new(m_ctx);
    // TODO: exception
    ASSERT(m_ssl);
    SSL_set_bio(m_ssl, OpenSSLStream::parent(), OpenSSLStream::parent());
    ownsParent(false);
    if (client) {
        SSL_set_connect_state(m_ssl);
    } else {
        SSL_set_accept_state(m_ssl);
    }
}

SSLStream::~SSLStream()
{
    SSL_free(m_ssl);
    SSL_CTX_free(m_ctx);
}

void
SSLStream::close(CloseType type)
{
    SSL_shutdown(m_ssl);
}

size_t
SSLStream::read(Buffer &b, size_t len)
{
    std::vector<iovec> bufs = b.writeBufs(len);
    int toRead = (int)std::min<size_t>(0x0fffffff, bufs[0].iov_len);
    int result = SSL_read(m_ssl, bufs[0].iov_base, toRead);
    if (result > 0) {
        b.produce(result);
        return result;
    }
    // TODO: exception
    return 0;
}

size_t
SSLStream::write(const Buffer &b, size_t len)
{
    std::vector<iovec> bufs = b.readBufs(len);
    int toWrite = (int)std::min<size_t>(0x0fffffff, bufs[0].iov_len);
    int result = SSL_write(m_ssl, bufs[0].iov_base, toWrite);
    if (result > 0) {
        return result;
    }
    // TODO: exception
    return 0;
}
