// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "ssl.h"

#include <sstream>

#include <openssl/err.h>

#include "mordor/common/log.h"

#ifdef MSVC
#pragma comment(lib, "libeay32")
#pragma comment(lib, "ssleay32")
#endif

static Logger::ptr g_log = Log::lookup("mordor:common:streams:ssl");

struct SSLInitializer {
    SSLInitializer()
    {
        SSL_library_init();
        SSL_load_error_strings();
    }
};

static SSLInitializer g_init;

static void throwOpenSSLException()
{
    int err = ERR_peek_error();
    switch (ERR_GET_REASON(err)) {
        case ERR_R_MALLOC_FAILURE:
            throw std::bad_alloc();
        case ERR_R_PASSED_NULL_PARAMETER:
            {
                char buf[120];
                ERR_error_string(err, buf);
                throw std::invalid_argument(buf);
            }
        default:
            throw OpenSSLException();
    }
};

std::string
OpenSSLException::constructMessage()
{
    std::ostringstream os;
    unsigned long err;
    char buf[120];
    while ( (err = ERR_get_error()) ) {
        if (!os.str().empty())
            os << "\n";
        os << ERR_error_string(err, buf);
    }
    ASSERT(!os.str().empty());
    return os.str();
}

SSLStream::SSLStream(Stream::ptr parent, bool client, bool own)
: MutatingFilterStream(parent, own)
{
    ASSERT(parent);
    ERR_clear_error();
    m_ctx.reset(SSL_CTX_new(client ? SSLv23_client_method() :
        SSLv23_server_method()), &SSL_CTX_free);
    if (!m_ctx)
        throwOpenSSLException();
    SSL_CTX_set_mode(m_ctx.get(), SSL_MODE_ENABLE_PARTIAL_WRITE);
    m_ssl.reset(SSL_new(m_ctx.get()), &SSL_free);
    if (!m_ssl)
        throwOpenSSLException();
    m_readBio = BIO_new_mem_buf("", 0);
    m_writeBio = BIO_new(BIO_s_mem());
    if (!m_readBio || !m_writeBio) {
        if (m_readBio) BIO_free(m_readBio);
        if (m_writeBio) BIO_free(m_writeBio);
        throwOpenSSLException();
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
            ERR_clear_error();
            int result = SSL_shutdown(m_ssl.get());
            int error = SSL_get_error(m_ssl.get(), result);
            LOG_VERBOSE(g_log) << (void *)this << " SSL_shutdown("
                << m_ssl.get() << "): " << result << " (" << error << ")";
            switch (error) {
                case SSL_ERROR_NONE:
                case SSL_ERROR_ZERO_RETURN:
                    break;
                case SSL_ERROR_WANT_READ:
                case SSL_ERROR_WANT_WRITE:
                case SSL_ERROR_WANT_CONNECT:
                case SSL_ERROR_WANT_ACCEPT:
                case SSL_ERROR_WANT_X509_LOOKUP:
                    NOTREACHED();
                case SSL_ERROR_SYSCALL:
                    NOTREACHED();
                case SSL_ERROR_SSL:
                    throwOpenSSLException();
                default:
                    NOTREACHED();
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
        int error = SSL_get_error(m_ssl.get(), result);
        LOG_VERBOSE(g_log) << (void *)this << "SSL_read(" << m_ssl.get()
            << ", " << toRead << "): " << result << " (" << error << ")";
        switch (error) {
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
                throwOpenSSLException();
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
        int error = SSL_get_error(m_ssl.get(), result);
        LOG_VERBOSE(g_log) << (void *)this << "SSL_write(" << m_ssl.get()
            << ", " << toWrite << "): " << result << " (" << error << ")";
        switch (error) {
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
                throwOpenSSLException();
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
            int error = SSL_get_error(m_ssl.get(), result);
            LOG_VERBOSE(g_log) << (void *)this << "SSL_shutdown("
                << m_ssl.get() << "): " << result << " (" << error << ")";
            switch (error) {
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
                    throwOpenSSLException();
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
        LOG_VERBOSE(g_log) << (void *)this << " parent()->write(" << toWrite
            << ")";
        size_t written = parent()->write(writeBuf, toWrite);
        LOG_VERBOSE(g_log) << (void *)this << " parent()->write(" << toWrite
            << "): " << written;
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
        LOG_VERBOSE(g_log) << (void *)this << " parent()->read(16389)";
        size_t result = parent()->read(m_readBuffer, 16384 + 5);
        LOG_VERBOSE(g_log) << (void *)this << " parent()->read(16389): "
            << result;
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
    LOG_VERBOSE(g_log) << (void *)this << " wantRead(): " << bm->length;
}
