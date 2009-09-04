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
: MutatingFilterStream(parent, own)
{
    ASSERT(parent);
    m_ctx.reset(SSL_CTX_new(client ? SSLv23_client_method() :
        SSLv23_server_method()), &SSL_CTX_free);
    ASSERT(m_ctx);
    SSL_CTX_set_mode(m_ctx.get(), SSL_MODE_ENABLE_PARTIAL_WRITE);
    // TODO: exception
    m_ssl.reset(SSL_new(m_ctx.get()), &SSL_free);
    // TODO: exception
    ASSERT(m_ssl);
    m_readBio = BIO_new_mem_buf((void *)"", 0);
    m_writeBio = BIO_new(BIO_s_mem());
    if (!m_readBio || !m_writeBio) {
        if (m_readBio) BIO_free(m_readBio);
        if (m_writeBio) BIO_free(m_writeBio);
        throw std::bad_alloc();
    }
    BIO_set_mem_eof_return(m_readBio, -1);

    SSL_set_bio(m_ssl.get(), m_readBio, m_writeBio);
    if (client) {
        SSL_set_connect_state(m_ssl.get());
    } else {
        SSL_set_accept_state(m_ssl.get());
    }
}

void
SSLStream::close(CloseType type)
{
    if (type == BOTH) {
        if (!(SSL_get_shutdown(m_ssl.get()) & SSL_SENT_SHUTDOWN)) {
            int result = SSL_shutdown(m_ssl.get());
            if (result == -1) {
                switch (SSL_get_error(m_ssl.get(), result)) {
                    case SSL_ERROR_NONE:
                    case SSL_ERROR_ZERO_RETURN:
                    case SSL_ERROR_WANT_READ:
                    case SSL_ERROR_WANT_WRITE:
                    case SSL_ERROR_WANT_CONNECT:
                    case SSL_ERROR_WANT_ACCEPT:
                    case SSL_ERROR_WANT_X509_LOOKUP:
                        NOTREACHED();
                    case SSL_ERROR_SYSCALL:
                        NOTREACHED();
                    case SSL_ERROR_SSL:
                        throw std::runtime_error("SSL Protocol Error");
                    default:
                        NOTREACHED();
                }                
            }
            flushBuffer();
        }
    }
    parent()->close(type);
}

size_t
SSLStream::read(Buffer &b, size_t len)
{
    flushBuffer();
    std::vector<iovec> bufs = b.writeBufs(len);
    int toRead = (int)std::min<size_t>(0x0fffffff, bufs[0].iov_len);
    while (true) {
        int result = SSL_read(m_ssl.get(), bufs[0].iov_base, toRead);
        switch (SSL_get_error(m_ssl.get(), result)) {
            case SSL_ERROR_NONE:
                b.produce(result);
                return result;
            case SSL_ERROR_ZERO_RETURN:
                // Received close_notify message
                ASSERT(result == 0);
                return 0;
            case SSL_ERROR_WANT_READ:
                wantRead();
                continue;
            case SSL_ERROR_WANT_WRITE:
            case SSL_ERROR_WANT_CONNECT:
            case SSL_ERROR_WANT_ACCEPT:
            case SSL_ERROR_WANT_X509_LOOKUP:
            case SSL_ERROR_SYSCALL:
                NOTREACHED();
            case SSL_ERROR_SSL:
                throw std::runtime_error("SSL Protocol Error");
            default:
                NOTREACHED();
        }
    }
}

size_t
SSLStream::write(const Buffer &b, size_t len)
{
    flushBuffer();
    if (len == 0)
        return 0;
    std::vector<iovec> bufs = b.readBufs(len);
    int toWrite = (int)std::min<size_t>(0x0fffffff, bufs[0].iov_len);
    while (true) {
        int result = SSL_write(m_ssl.get(), bufs[0].iov_base, toWrite);
        switch (SSL_get_error(m_ssl.get(), result)) {
            case SSL_ERROR_NONE:
                // Don't flushBuffer() here, because we would lose our return
                // value
                return result;
            case SSL_ERROR_ZERO_RETURN:
                // Received close_notify message
                // TODO: implicitly call SSL_shutdown here too?
                ASSERT(result != 0);
                return result;
            case SSL_ERROR_WANT_READ:
                wantRead();
                continue;
            case SSL_ERROR_WANT_WRITE:
            case SSL_ERROR_WANT_CONNECT:
            case SSL_ERROR_WANT_ACCEPT:
            case SSL_ERROR_WANT_X509_LOOKUP:
            case SSL_ERROR_SYSCALL:
                NOTREACHED();
            case SSL_ERROR_SSL:
                throw std::runtime_error("SSL Protocol Error");
            default:
                NOTREACHED();
        }
    }
}

void
SSLStream::flush()
{
    if ( (SSL_get_shutdown(m_ssl.get()) & SSL_SENT_SHUTDOWN) &&
        !(SSL_get_shutdown(m_ssl.get()) & SSL_RECEIVED_SHUTDOWN)) {
        while (true) {
            int result = SSL_shutdown(m_ssl.get());
            switch (SSL_get_error(m_ssl.get(), result)) {
                case SSL_ERROR_NONE:
                case SSL_ERROR_ZERO_RETURN:
                    break;
                case SSL_ERROR_WANT_READ:
                    wantRead();
                    continue;
                case SSL_ERROR_WANT_WRITE:
                case SSL_ERROR_WANT_CONNECT:
                case SSL_ERROR_WANT_ACCEPT:
                case SSL_ERROR_WANT_X509_LOOKUP:
                case SSL_ERROR_SYSCALL:
                    NOTREACHED();
                case SSL_ERROR_SSL:
                    throw std::runtime_error("SSL Protocol Error");
                default:
                    NOTREACHED();
            }
        }
    }
    flushBuffer();
}

void
SSLStream::flushBuffer()
{
    char *writeBuf;
    long toWrite = BIO_get_mem_data(m_writeBio, &writeBuf);
    while (toWrite) {
        size_t written = parent()->write(writeBuf, toWrite);
        writeBuf += written;
        toWrite -= written;
    }
    BIO_reset(m_writeBio);
}

void
SSLStream::wantRead()
{
    flushBuffer();
    BUF_MEM *bm;
    BIO_get_mem_ptr(m_readBio, &bm);
    ASSERT(bm->length == 0);
    m_readBuffer.consume(bm->max);
    if (m_readBuffer.readAvailable() == 0) {
        // Maximum SSL record size
        size_t result = parent()->read(m_readBuffer, 16384 + 5);
        if (result == 0) {
            BIO_set_mem_eof_return(m_readBio, 0);
            return;
        }
    }
    ASSERT(m_readBuffer.readAvailable());
    std::vector<iovec> bufs = m_readBuffer.readBufs();
    bm->data = (char *)bufs[0].iov_base;
    bm->length = bm->max =
        std::min<size_t>(0x7fffffff, bufs[0].iov_len);
}
