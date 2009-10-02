// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "ssl.h"

#include <sstream>

#include <openssl/err.h>
#include <openssl/x509v3.h>

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

static bool hasOpenSSLError()
{
    int err = ERR_peek_error();
    if (err == 0)
        return false;
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
            return true;
    }
}

static std::string getOpenSSLErrorMessage()
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

std::string
CertificateVerificationException::constructMessage(long verifyResult)
{
    return X509_verify_cert_error_string(verifyResult);
}

static void delete_nothing(SSL_CTX *) {}

SSLStream::SSLStream(Stream::ptr parent, bool client, bool own, SSL_CTX *ctx)
: MutatingFilterStream(parent, own)
{
    ASSERT(parent);
    ERR_clear_error();
    if (ctx)
        m_ctx.reset(ctx, &delete_nothing);
    else
        m_ctx.reset(SSL_CTX_new(client ? SSLv23_client_method() :
            SSLv23_server_method()), &SSL_CTX_free);
    if (!m_ctx) {
        VERIFY(hasOpenSSLError());
        throw OpenSSLException(getOpenSSLErrorMessage(), "SSL_CTX_new");
    }
    SSL_CTX_set_mode(m_ctx.get(), SSL_MODE_ENABLE_PARTIAL_WRITE);
    m_ssl.reset(SSL_new(m_ctx.get()), &SSL_free);
    if (!m_ssl) {
        VERIFY(hasOpenSSLError());
        throw OpenSSLException(getOpenSSLErrorMessage(), "SSL_CTX_new");
    }
    m_readBio = BIO_new_mem_buf((void *)"", 0);
    m_writeBio = BIO_new(BIO_s_mem());
    if (!m_readBio || !m_writeBio) {
        if (m_readBio) BIO_free(m_readBio);
        if (m_writeBio) BIO_free(m_writeBio);
        VERIFY(hasOpenSSLError());
        throw OpenSSLException(getOpenSSLErrorMessage(), "BIO_new");
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
            LOG_VERBOSE(g_log) << this << " SSL_shutdown(" << m_ssl.get()
                << "): " << result << " (" << error << ")";
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
                    if (hasOpenSSLError()) {
                        std::string message = getOpenSSLErrorMessage();
                        LOG_ERROR(g_log) << this << " SSL_shutdown("
                            << m_ssl.get() << "): " << result << " (" << error
                            << ", " << message << ")";
                        throw OpenSSLException(message, "SSL_shutdown");
                    }
                    if (result == 0)
                        break;
                    NOTREACHED();
                case SSL_ERROR_SSL:
                    {
                        VERIFY(hasOpenSSLError());
                        std::string message = getOpenSSLErrorMessage();
                        LOG_ERROR(g_log) << this << " SSL_shutdown("
                            << m_ssl.get() << "): " << result << " (" << error
                            << ", " << message << ")";
                        throw OpenSSLException(message, "SSL_shutdown");
                    }
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
        LOG_VERBOSE(g_log) << this << " SSL_read(" << m_ssl.get() << ", "
            << toRead << "): " << result << " (" << error << ")";
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
                NOTREACHED();
            case SSL_ERROR_SYSCALL:
                if (hasOpenSSLError()) {
                    std::string message = getOpenSSLErrorMessage();
                    LOG_ERROR(g_log) << this << " SSL_read("
                        << m_ssl.get() << ", " << toRead << "): " << result
                        << " (" << error << ", " << message << ")";
                    throw OpenSSLException(message, "SSL_read");
                }
                if (result == 0) {
                    LOG_ERROR(g_log) << this << " SSL_read(" << m_ssl.get()
                        << ", " << toRead << "): " << result << " ("
                        << error << ")";
                    throw UnexpectedEofError();
                }
                NOTREACHED();
            case SSL_ERROR_SSL:
                {
                    VERIFY(hasOpenSSLError());
                    std::string message = getOpenSSLErrorMessage();
                    LOG_ERROR(g_log) << this << " SSL_read("
                        << m_ssl.get() << ", " << toRead << "): " << result
                        << " (" << error << ", " << message << ")";
                    throw OpenSSLException(message, "SSL_read");
                }
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
        LOG_VERBOSE(g_log) << this << " SSL_write(" << m_ssl.get() << ", "
            << toWrite << "): " << result << " (" << error << ")";
        switch (error) {
            case SSL_ERROR_NONE:
                // Don't flushBuffer() here, because we would lose our return
                // value
                return result;
            case SSL_ERROR_ZERO_RETURN:
                // Received close_notify message
                ASSERT(result != 0);
                return result;
            case SSL_ERROR_WANT_READ:
                wantRead();
                continue;
            case SSL_ERROR_WANT_WRITE:
            case SSL_ERROR_WANT_CONNECT:
            case SSL_ERROR_WANT_ACCEPT:
            case SSL_ERROR_WANT_X509_LOOKUP:
                NOTREACHED();
            case SSL_ERROR_SYSCALL:
                if (hasOpenSSLError()) {
                        std::string message = getOpenSSLErrorMessage();
                        LOG_ERROR(g_log) << this << " SSL_write("
                            << m_ssl.get() << ", " << toWrite << "): "
                            << result << " (" << error << ", " << message
                            << ")";
                        throw OpenSSLException(message, "SSL_write");
                    }
                if (result == 0) {
                    LOG_ERROR(g_log) << this << " SSL_write(" << m_ssl.get()
                        << ", " << toWrite << "): " << result << " ("
                        << error << ")";
                    throw UnexpectedEofError();
                }
                NOTREACHED();
            case SSL_ERROR_SSL:
                {
                    VERIFY(hasOpenSSLError());
                    std::string message = getOpenSSLErrorMessage();
                    LOG_ERROR(g_log) << this << " SSL_write("
                        << m_ssl.get() << ", " << toWrite << "): " << result
                        << " (" << error << ", " << message << ")";
                    throw OpenSSLException(message, "SSL_write");
                }
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
            LOG_VERBOSE(g_log) << this << " SSL_shutdown(" << m_ssl.get()
                << "): " << result << " (" << error << ")";
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
                    NOTREACHED();
                case SSL_ERROR_SYSCALL:
                    if (hasOpenSSLError()) {
                        std::string message = getOpenSSLErrorMessage();
                        LOG_ERROR(g_log) << this << " SSL_shutdown("
                            << m_ssl.get() << "): " << result << " (" << error
                            << ", " << message << ")";
                        throw OpenSSLException(message, "SSL_shutdown");
                    }
                    if (result == 0) {
                        LOG_ERROR(g_log) << this << " SSL_shutdown(" << m_ssl.get()
                            << "): " << result << " (" << error << ")";
                        throw UnexpectedEofError();
                    }
                    NOTREACHED();
                case SSL_ERROR_SSL:
                    {
                        VERIFY(hasOpenSSLError());
                        std::string message = getOpenSSLErrorMessage();
                        LOG_ERROR(g_log) << this << " SSL_shutdown("
                            << m_ssl.get() << "): " << result << " (" << error
                            << ", " << message << ")";
                        throw OpenSSLException(message, "SSL_shutdown");
                    }
                default:
                    NOTREACHED();
            }
        }
    }
    flushBuffer();
}

void
SSLStream::accept()
{
    flushBuffer();
    while (true) {
        int result = SSL_accept(m_ssl.get());
        int error = SSL_get_error(m_ssl.get(), result);
        LOG_VERBOSE(g_log) << this << " SSL_accept(" << m_ssl.get() << "): "
            << result << " (" << error << ")";
        switch (error) {
            case SSL_ERROR_NONE:
                return;
            case SSL_ERROR_ZERO_RETURN:
                // Received close_notify message
                return;
            case SSL_ERROR_WANT_READ:
                wantRead();
                continue;
            case SSL_ERROR_WANT_WRITE:
            case SSL_ERROR_WANT_CONNECT:
            case SSL_ERROR_WANT_ACCEPT:
            case SSL_ERROR_WANT_X509_LOOKUP:
                NOTREACHED();
            case SSL_ERROR_SYSCALL:
                if (hasOpenSSLError()) {
                    std::string message = getOpenSSLErrorMessage();
                    LOG_ERROR(g_log) << this << " SSL_accept("
                        << m_ssl.get() << "): " << result << " (" << error
                        << ", " << message << ")";
                    throw OpenSSLException(message, "SSL_accept");
                }
                if (result == 0) {
                    LOG_ERROR(g_log) << this << " SSL_accept(" << m_ssl.get()
                        << "): " << result << " (" << error << ")";
                    throw UnexpectedEofError();
                }
                NOTREACHED();
            case SSL_ERROR_SSL:
                {
                    VERIFY(hasOpenSSLError());
                    std::string message = getOpenSSLErrorMessage();
                    LOG_ERROR(g_log) << this << " SSL_accept("
                        << m_ssl.get() << "): " << result << " (" << error
                        << ", " << message << ")";
                    throw OpenSSLException(message, "SSL_accept");
                }
            default:
                NOTREACHED();
        }
    }
}

void
SSLStream::connect()
{
    flushBuffer();
    while (true) {
        int result = SSL_connect(m_ssl.get());
        int error = SSL_get_error(m_ssl.get(), result);
        LOG_VERBOSE(g_log) << this << " SSL_connect(" << m_ssl.get() << "): "
            << result << " (" << error << ")";
        switch (error) {
            case SSL_ERROR_NONE:
                return;
            case SSL_ERROR_ZERO_RETURN:
                // Received close_notify message
                return;
            case SSL_ERROR_WANT_READ:
                wantRead();
                continue;
            case SSL_ERROR_WANT_WRITE:
            case SSL_ERROR_WANT_CONNECT:
            case SSL_ERROR_WANT_ACCEPT:
            case SSL_ERROR_WANT_X509_LOOKUP:
                NOTREACHED();
            case SSL_ERROR_SYSCALL:
                if (hasOpenSSLError()) {
                    std::string message = getOpenSSLErrorMessage();
                    LOG_ERROR(g_log) << this << " SSL_connect("
                        << m_ssl.get() << "): " << result << " (" << error
                        << ", " << message << ")";
                    throw OpenSSLException(message, "SSL_connect");
                }
                if (result == 0) {
                    LOG_ERROR(g_log) << this << " SSL_connect(" << m_ssl.get()
                        << "): " << result << " (" << error << ")";
                    throw UnexpectedEofError();
                }
                NOTREACHED();
            case SSL_ERROR_SSL:
                {
                    VERIFY(hasOpenSSLError());
                    std::string message = getOpenSSLErrorMessage();
                    LOG_ERROR(g_log) << this << " SSL_connect("
                        << m_ssl.get() << "): " << result << " (" << error
                        << ", " << message << ")";
                    throw OpenSSLException(message, "SSL_connect");
                }
            default:
                NOTREACHED();
        }
    }
}

void
SSLStream::verifyPeerCertificate()
{
    long verifyResult = SSL_get_verify_result(m_ssl.get());
    LOG_LEVEL(g_log, verifyResult ? Log::WARNING : Log::VERBOSE) << this
        << " SSL_get_verify_result(" << m_ssl.get() << "): "
        << verifyResult;
    if (verifyResult != X509_V_OK)
        throw CertificateVerificationException(verifyResult);
}

void
SSLStream::verifyPeerCertificate(const std::string &hostname)
{
    if (!hostname.empty()) {
        std::string wildcardHostname = "*";
        size_t dot = hostname.find('.');
        if (dot != std::string::npos)
            wildcardHostname.append(hostname.substr(dot));
        boost::shared_ptr<X509> cert;
        cert.reset(SSL_get_peer_certificate(m_ssl.get()), &X509_free);
        if (!cert)
            throw CertificateVerificationException(
                X509_V_ERR_APPLICATION_VERIFICATION,
                "No Certificate Presented");
        int critical = -1, altNameIndex = -1;
        GENERAL_NAMES *gens = (GENERAL_NAMES *)X509_get_ext_d2i(cert.get(), NID_subject_alt_name, &critical, &altNameIndex);
        if (gens) {
            do {
                try {
                    bool success = false;
                    for(int i = 0; i < sk_GENERAL_NAME_num(gens); i++)
	                {
		                GENERAL_NAME *gen = sk_GENERAL_NAME_value(gens, i);
		                if(gen->type != GEN_DNS) continue;
                        std::string altName((const char *)gen->d.dNSName->data, gen->d.dNSName->length);
                        if (altName == wildcardHostname || altName == hostname) {
                            success = true;
                            break;
                        }
	                }
                    sk_GENERAL_NAME_pop_free(gens, GENERAL_NAME_free);
                    if (success)
                        return;
                } catch (...) {
                    sk_GENERAL_NAME_pop_free(gens, GENERAL_NAME_free);
                    throw;
                }
                gens = (GENERAL_NAMES *)X509_get_ext_d2i(cert.get(), NID_subject_alt_name, &critical, &altNameIndex);
            } while (gens);
        }
        X509_NAME *name = X509_get_subject_name(cert.get());        
        if (!name)
            throw CertificateVerificationException(
                X509_V_ERR_APPLICATION_VERIFICATION,
                "No Subject Name");
        int len = X509_NAME_get_text_by_NID(name, NID_commonName, NULL, 0);
        if (len == -1)
            throw CertificateVerificationException(
                X509_V_ERR_APPLICATION_VERIFICATION,
                "No Common Name");
        std::string commonName;
        commonName.resize(len);
        X509_NAME_get_text_by_NID(name, NID_commonName, &commonName[0], len + 1);
        if (commonName == wildcardHostname || commonName == hostname)
            return;
        throw CertificateVerificationException(
                X509_V_ERR_APPLICATION_VERIFICATION,
                "No Matching Common Name");
    }    
}

void
SSLStream::flushBuffer()
{
    char *writeBuf;
    size_t toWrite = BIO_get_mem_data(m_writeBio, &writeBuf);
    while (toWrite) {
        LOG_VERBOSE(g_log) << this << " parent()->write(" << toWrite << ")";
        size_t written = parent()->write(writeBuf, toWrite);
        LOG_VERBOSE(g_log) << this << " parent()->write(" << toWrite << "): "
            << written;
        writeBuf += written;
        toWrite -= written;
    }
    int dummy = BIO_reset(m_writeBio);
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
        LOG_VERBOSE(g_log) << this << " parent()->read(16389)";
        size_t result = parent()->read(m_readBuffer, 16384 + 5);
        LOG_VERBOSE(g_log) << this << " parent()->read(16389): " << result;
        if (result == 0) {
            BIO_set_mem_eof_return(m_readBio, 0);
            return;
        }
    }
    ASSERT(m_readBuffer.readAvailable());
    std::vector<iovec> bufs = m_readBuffer.readBufs();
    bm->data = (char *)bufs[0].iov_base;
    bm->length = bm->max =
        (long)std::min<size_t>(0x7fffffff, bufs[0].iov_len);
    LOG_VERBOSE(g_log) << this << " wantRead(): " << bm->length;
}
