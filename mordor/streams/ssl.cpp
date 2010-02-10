// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "ssl.h"

#include <sstream>

#include <openssl/err.h>
#include <openssl/x509v3.h>

#include "mordor/log.h"
#include "mordor/util.h"

#ifdef MSVC
#pragma comment(lib, "libeay32")
#pragma comment(lib, "ssleay32")
#endif

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:streams:ssl");

namespace {

static struct Initializer {
    Initializer()
    {
        SSL_library_init();
        SSL_load_error_strings();
    }
} g_init;

}

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
                MORDOR_THROW_EXCEPTION(std::invalid_argument(buf));
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
    MORDOR_ASSERT(!os.str().empty());
    return os.str();
}

OpenSSLException::OpenSSLException() :
    std::runtime_error(getOpenSSLErrorMessage())
{
}

std::string
CertificateVerificationException::constructMessage(long verifyResult)
{
    return X509_verify_cert_error_string(verifyResult);
}

// Adapted from https://www.codeblog.org/viewsrc/openssl-fips-1.1.1/demos/x509/mkcert.c
static void add_ext(X509 *cert, int nid, const char *value);

static void mkcert(boost::shared_ptr<X509> &cert,
                  boost::shared_ptr<EVP_PKEY> &pkey, int bits, int serial,
                  int days)
{
    RSA *rsa;
    X509_NAME *name=NULL;

    pkey.reset(EVP_PKEY_new(), &EVP_PKEY_free);
    if (!pkey)
        throw std::bad_alloc();
    cert.reset(X509_new(), &X509_free);
    if (!cert)
        throw std::bad_alloc();

    rsa = RSA_generate_key(bits,RSA_F4,NULL,NULL);
    MORDOR_VERIFY(EVP_PKEY_assign_RSA(pkey.get(),rsa));

    X509_set_version(cert.get(),2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert.get()),serial);
    X509_gmtime_adj(X509_get_notBefore(cert.get()),0);
    X509_gmtime_adj(X509_get_notAfter(cert.get()),(long)60*60*24*days);
    X509_set_pubkey(cert.get(),pkey.get());

    name=X509_get_subject_name(cert.get());

    /* This function creates and adds the entry, working out the
     * correct string type and performing checks on its length.
     * Normally we'd check the return value for errors...
     */
    X509_NAME_add_entry_by_txt(name,"C",
                            MBSTRING_ASC,
                            (const unsigned char *)"United States",
                            -1, -1, 0);
    X509_NAME_add_entry_by_txt(name,"CN",
                            MBSTRING_ASC,
                            (const unsigned char *)"Mordor Default Self-signed Certificate",
                            -1, -1, 0);

    /* Its self signed so set the issuer name to be the same as the
     * subject.
     */
    X509_set_issuer_name(cert.get(),name);

    /* Add various extensions: standard extensions */
    add_ext(cert.get(), NID_basic_constraints, "critical,CA:TRUE");
    add_ext(cert.get(), NID_key_usage, "critical,keyCertSign,cRLSign");

    add_ext(cert.get(), NID_subject_key_identifier, "hash");

    /* Some Netscape specific extensions */
    add_ext(cert.get(), NID_netscape_cert_type, "sslCA");

    MORDOR_VERIFY(X509_sign(cert.get(),pkey.get(),EVP_md5()));
}

/* Add extension using V3 code: we can set the config file as NULL
 * because we wont reference any other sections.
 */

void add_ext(X509 *cert, int nid, const char *value)
{
    X509_EXTENSION *ex = NULL;
    X509V3_CTX ctx;
    /* This sets the 'context' of the extensions. */
    /* No configuration database */
    X509V3_set_ctx_nodb(&ctx);
    /* Issuer and subject certs: both the target since it is self signed,
     * no request and no CRL
     */
    X509V3_set_ctx(&ctx, cert, cert, NULL, NULL, 0);
    MORDOR_VERIFY(X509V3_EXT_conf_nid(NULL, &ctx, nid, (char*) value));

    X509_add_ext(cert,ex,-1);
    X509_EXTENSION_free(ex);
}


SSLStream::SSLStream(Stream::ptr parent, bool client, bool own, SSL_CTX *ctx)
: MutatingFilterStream(parent, own),
  m_inRead(false),
  m_readCondition(m_mutex)
{
    MORDOR_ASSERT(parent);
    ERR_clear_error();
    if (ctx)
        m_ctx.reset(ctx, &nop<SSL_CTX *>);
    else
        m_ctx.reset(SSL_CTX_new(client ? SSLv23_client_method() :
            SSLv23_server_method()), &SSL_CTX_free);
    if (!m_ctx) {
        MORDOR_VERIFY(hasOpenSSLError());
        MORDOR_THROW_EXCEPTION(OpenSSLException(getOpenSSLErrorMessage()))
            << boost::errinfo_api_function("SSL_CTX_new");
    }
    // Auto-generate self-signed server cert
    if (!ctx && !client) {
        boost::shared_ptr<X509> cert;
        boost::shared_ptr<EVP_PKEY> pkey;
        mkcert(cert, pkey, 1024, 0, 365);
        SSL_CTX_use_certificate(m_ctx.get(), cert.get());
        SSL_CTX_use_PrivateKey(m_ctx.get(), pkey.get());
    }
    m_ssl.reset(SSL_new(m_ctx.get()), &SSL_free);
    if (!m_ssl) {
        MORDOR_VERIFY(hasOpenSSLError());
        MORDOR_THROW_EXCEPTION(OpenSSLException(getOpenSSLErrorMessage()))
            << boost::errinfo_api_function("SSL_CTX_new");
    }
    m_readBio = BIO_new_mem_buf((void *)"", 0);
    m_writeBio = BIO_new(BIO_s_mem());
    if (!m_readBio || !m_writeBio) {
        if (m_readBio) BIO_free(m_readBio);
        if (m_writeBio) BIO_free(m_writeBio);
        MORDOR_VERIFY(hasOpenSSLError());
        MORDOR_THROW_EXCEPTION(OpenSSLException(getOpenSSLErrorMessage()))
            << boost::errinfo_api_function("BIO_new");
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
    FiberMutex::ScopedLock lock(m_mutex);
    MORDOR_ASSERT(type == BOTH);
    if (!(SSL_get_shutdown(m_ssl.get()) & SSL_SENT_SHUTDOWN)) {
        ERR_clear_error();
        int result = SSL_shutdown(m_ssl.get());
        int error = SSL_get_error(m_ssl.get(), result);
        MORDOR_LOG_DEBUG(g_log) << this << " SSL_shutdown(" << m_ssl.get()
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
                MORDOR_NOTREACHED();
            case SSL_ERROR_SYSCALL:
                if (hasOpenSSLError()) {
                    std::string message = getOpenSSLErrorMessage();
                    MORDOR_LOG_ERROR(g_log) << this << " SSL_shutdown("
                        << m_ssl.get() << "): " << result << " (" << error
                        << ", " << message << ")";
                    MORDOR_THROW_EXCEPTION(OpenSSLException(message))
                        << boost::errinfo_api_function("SSL_shutdown");
                }
                if (result == 0)
                    break;
                MORDOR_NOTREACHED();
            case SSL_ERROR_SSL:
                {
                    MORDOR_VERIFY(hasOpenSSLError());
                    std::string message = getOpenSSLErrorMessage();
                    MORDOR_LOG_ERROR(g_log) << this << " SSL_shutdown("
                        << m_ssl.get() << "): " << result << " (" << error
                        << ", " << message << ")";
                    MORDOR_THROW_EXCEPTION(OpenSSLException(message))
                        << boost::errinfo_api_function("SSL_shutdown");
                }
            default:
                MORDOR_NOTREACHED();
        }
        flushBuffer();
    }
    parent()->close();
}

size_t
SSLStream::read(Buffer &b, size_t len)
{
    FiberMutex::ScopedLock lock(m_mutex);
    flushBuffer();
    std::vector<iovec> bufs = b.writeBufs(len);
    int toRead = (int)std::min<size_t>(0x0fffffff, bufs[0].iov_len);
    while (true) {
        int result = SSL_read(m_ssl.get(), bufs[0].iov_base, toRead);
        int error = SSL_get_error(m_ssl.get(), result);
        MORDOR_LOG_DEBUG(g_log) << this << " SSL_read(" << m_ssl.get() << ", "
            << toRead << "): " << result << " (" << error << ")";
        switch (error) {
            case SSL_ERROR_NONE:
                b.produce(result);
                return result;
            case SSL_ERROR_ZERO_RETURN:
                // Received close_notify message
                MORDOR_ASSERT(result == 0);
                return 0;
            case SSL_ERROR_WANT_READ:
                wantRead(lock);
                continue;
            case SSL_ERROR_WANT_WRITE:
            case SSL_ERROR_WANT_CONNECT:
            case SSL_ERROR_WANT_ACCEPT:
            case SSL_ERROR_WANT_X509_LOOKUP:
                MORDOR_NOTREACHED();
            case SSL_ERROR_SYSCALL:
                if (hasOpenSSLError()) {
                    std::string message = getOpenSSLErrorMessage();
                    MORDOR_LOG_ERROR(g_log) << this << " SSL_read("
                        << m_ssl.get() << ", " << toRead << "): " << result
                        << " (" << error << ", " << message << ")";
                    MORDOR_THROW_EXCEPTION(OpenSSLException(message))
                        << boost::errinfo_api_function("SSL_read");
                }
                if (result == 0) {
                    MORDOR_LOG_WARNING(g_log) << this << " SSL_read(" << m_ssl.get()
                        << ", " << toRead << "): " << result << " ("
                        << error << ")";
                    return 0;
                }
                MORDOR_NOTREACHED();
            case SSL_ERROR_SSL:
                {
                    MORDOR_VERIFY(hasOpenSSLError());
                    std::string message = getOpenSSLErrorMessage();
                    MORDOR_LOG_ERROR(g_log) << this << " SSL_read("
                        << m_ssl.get() << ", " << toRead << "): " << result
                        << " (" << error << ", " << message << ")";
                    MORDOR_THROW_EXCEPTION(OpenSSLException(message))
                        << boost::errinfo_api_function("SSL_read");
                }
            default:
                MORDOR_NOTREACHED();
        }
    }
}

size_t
SSLStream::write(const Buffer &b, size_t len)
{
    FiberMutex::ScopedLock lock(m_mutex);
    flushBuffer();
    if (len == 0)
        return 0;
    std::vector<iovec> bufs = b.readBufs(len);
    int toWrite = (int)std::min<size_t>(0x0fffffff, bufs[0].iov_len);
    while (true) {
        int result = SSL_write(m_ssl.get(), bufs[0].iov_base, toWrite);
        int error = SSL_get_error(m_ssl.get(), result);
        MORDOR_LOG_DEBUG(g_log) << this << " SSL_write(" << m_ssl.get() << ", "
            << toWrite << "): " << result << " (" << error << ")";
        switch (error) {
            case SSL_ERROR_NONE:
                // Don't flushBuffer() here, because we would lose our return
                // value
                return result;
            case SSL_ERROR_ZERO_RETURN:
                // Received close_notify message
                MORDOR_ASSERT(result != 0);
                return result;
            case SSL_ERROR_WANT_READ:
                wantRead(lock);
                continue;
            case SSL_ERROR_WANT_WRITE:
            case SSL_ERROR_WANT_CONNECT:
            case SSL_ERROR_WANT_ACCEPT:
            case SSL_ERROR_WANT_X509_LOOKUP:
                MORDOR_NOTREACHED();
            case SSL_ERROR_SYSCALL:
                if (hasOpenSSLError()) {
                        std::string message = getOpenSSLErrorMessage();
                        MORDOR_LOG_ERROR(g_log) << this << " SSL_write("
                            << m_ssl.get() << ", " << toWrite << "): "
                            << result << " (" << error << ", " << message
                            << ")";
                        MORDOR_THROW_EXCEPTION(OpenSSLException(message))
                            << boost::errinfo_api_function("SSL_write");
                    }
                if (result == 0) {
                    MORDOR_LOG_ERROR(g_log) << this << " SSL_write(" << m_ssl.get()
                        << ", " << toWrite << "): " << result << " ("
                        << error << ")";
                    MORDOR_THROW_EXCEPTION(UnexpectedEofException());
                }
                MORDOR_NOTREACHED();
            case SSL_ERROR_SSL:
                {
                    MORDOR_VERIFY(hasOpenSSLError());
                    std::string message = getOpenSSLErrorMessage();
                    MORDOR_LOG_ERROR(g_log) << this << " SSL_write("
                        << m_ssl.get() << ", " << toWrite << "): " << result
                        << " (" << error << ", " << message << ")";
                    MORDOR_THROW_EXCEPTION(OpenSSLException(message))
                        << boost::errinfo_api_function("SSL_write");
                }
            default:
                MORDOR_NOTREACHED();
        }
    }
}

void
SSLStream::flush(bool flushParent)
{
    FiberMutex::ScopedLock lock(m_mutex);
    if ( (SSL_get_shutdown(m_ssl.get()) & SSL_SENT_SHUTDOWN) &&
        !(SSL_get_shutdown(m_ssl.get()) & SSL_RECEIVED_SHUTDOWN)) {
        while (true) {
            int result = SSL_shutdown(m_ssl.get());
            int error = SSL_get_error(m_ssl.get(), result);
            MORDOR_LOG_DEBUG(g_log) << this << " SSL_shutdown(" << m_ssl.get()
                << "): " << result << " (" << error << ")";
            switch (error) {
                case SSL_ERROR_NONE:
                case SSL_ERROR_ZERO_RETURN:
                    break;
                case SSL_ERROR_WANT_READ:
                    wantRead(lock);
                    continue;
                case SSL_ERROR_WANT_WRITE:
                case SSL_ERROR_WANT_CONNECT:
                case SSL_ERROR_WANT_ACCEPT:
                case SSL_ERROR_WANT_X509_LOOKUP:
                    MORDOR_NOTREACHED();
                case SSL_ERROR_SYSCALL:
                    if (hasOpenSSLError()) {
                        std::string message = getOpenSSLErrorMessage();
                        MORDOR_LOG_ERROR(g_log) << this << " SSL_shutdown("
                            << m_ssl.get() << "): " << result << " (" << error
                            << ", " << message << ")";
                        MORDOR_THROW_EXCEPTION(OpenSSLException(message))
                            << boost::errinfo_api_function("SSL_shutdown");
                    }
                    if (result == 0) {
                        MORDOR_LOG_ERROR(g_log) << this << " SSL_shutdown(" << m_ssl.get()
                            << "): " << result << " (" << error << ")";
                        MORDOR_THROW_EXCEPTION(UnexpectedEofException());
                    }
                    MORDOR_NOTREACHED();
                case SSL_ERROR_SSL:
                    {
                        MORDOR_VERIFY(hasOpenSSLError());
                        std::string message = getOpenSSLErrorMessage();
                        MORDOR_LOG_ERROR(g_log) << this << " SSL_shutdown("
                            << m_ssl.get() << "): " << result << " (" << error
                            << ", " << message << ")";
                        MORDOR_THROW_EXCEPTION(OpenSSLException(message))
                            << boost::errinfo_api_function("SSL_shutdown");
                    }
                default:
                    MORDOR_NOTREACHED();
            }
        }
    }
    flushBuffer(flushParent);
}

void
SSLStream::accept()
{
    FiberMutex::ScopedLock lock(m_mutex);
    flushBuffer();
    while (true) {
        int result = SSL_accept(m_ssl.get());
        int error = SSL_get_error(m_ssl.get(), result);
        MORDOR_LOG_DEBUG(g_log) << this << " SSL_accept(" << m_ssl.get() << "): "
            << result << " (" << error << ")";
        switch (error) {
            case SSL_ERROR_NONE:
                flushBuffer();
                return;
            case SSL_ERROR_ZERO_RETURN:
                // Received close_notify message
                return;
            case SSL_ERROR_WANT_READ:
                wantRead(lock);
                continue;
            case SSL_ERROR_WANT_WRITE:
            case SSL_ERROR_WANT_CONNECT:
            case SSL_ERROR_WANT_ACCEPT:
            case SSL_ERROR_WANT_X509_LOOKUP:
                MORDOR_NOTREACHED();
            case SSL_ERROR_SYSCALL:
                if (hasOpenSSLError()) {
                    std::string message = getOpenSSLErrorMessage();
                    MORDOR_LOG_ERROR(g_log) << this << " SSL_accept("
                        << m_ssl.get() << "): " << result << " (" << error
                        << ", " << message << ")";
                    MORDOR_THROW_EXCEPTION(OpenSSLException(message))
                        << boost::errinfo_api_function("SSL_accept");
                }
                if (result == 0) {
                    MORDOR_LOG_ERROR(g_log) << this << " SSL_accept(" << m_ssl.get()
                        << "): " << result << " (" << error << ")";
                    MORDOR_THROW_EXCEPTION(UnexpectedEofException());
                }
                MORDOR_NOTREACHED();
            case SSL_ERROR_SSL:
                {
                    MORDOR_VERIFY(hasOpenSSLError());
                    std::string message = getOpenSSLErrorMessage();
                    MORDOR_LOG_ERROR(g_log) << this << " SSL_accept("
                        << m_ssl.get() << "): " << result << " (" << error
                        << ", " << message << ")";
                    MORDOR_THROW_EXCEPTION(OpenSSLException(message))
                        << boost::errinfo_api_function("SSL_accept");
                }
            default:
                MORDOR_NOTREACHED();
        }
    }
}

void
SSLStream::connect()
{
    FiberMutex::ScopedLock lock(m_mutex);
    flushBuffer();
    while (true) {
        int result = SSL_connect(m_ssl.get());
        int error = SSL_get_error(m_ssl.get(), result);
        MORDOR_LOG_DEBUG(g_log) << this << " SSL_connect(" << m_ssl.get() << "): "
            << result << " (" << error << ")";
        switch (error) {
            case SSL_ERROR_NONE:
                flushBuffer();
                return;
            case SSL_ERROR_ZERO_RETURN:
                // Received close_notify message
                return;
            case SSL_ERROR_WANT_READ:
                wantRead(lock);
                continue;
            case SSL_ERROR_WANT_WRITE:
            case SSL_ERROR_WANT_CONNECT:
            case SSL_ERROR_WANT_ACCEPT:
            case SSL_ERROR_WANT_X509_LOOKUP:
                MORDOR_NOTREACHED();
            case SSL_ERROR_SYSCALL:
                if (hasOpenSSLError()) {
                    std::string message = getOpenSSLErrorMessage();
                    MORDOR_LOG_ERROR(g_log) << this << " SSL_connect("
                        << m_ssl.get() << "): " << result << " (" << error
                        << ", " << message << ")";
                    MORDOR_THROW_EXCEPTION(OpenSSLException(message))
                        << boost::errinfo_api_function("SSL_connect");
                }
                if (result == 0) {
                    MORDOR_LOG_ERROR(g_log) << this << " SSL_connect(" << m_ssl.get()
                        << "): " << result << " (" << error << ")";
                    MORDOR_THROW_EXCEPTION(UnexpectedEofException());
                }
                MORDOR_NOTREACHED();
            case SSL_ERROR_SSL:
                {
                    MORDOR_VERIFY(hasOpenSSLError());
                    std::string message = getOpenSSLErrorMessage();
                    MORDOR_LOG_ERROR(g_log) << this << " SSL_connect("
                        << m_ssl.get() << "): " << result << " (" << error
                        << ", " << message << ")";
                    MORDOR_THROW_EXCEPTION(OpenSSLException(message))
                        << boost::errinfo_api_function("SSL_connect");
                }
            default:
                MORDOR_NOTREACHED();
        }
    }
}

void
SSLStream::verifyPeerCertificate()
{
    long verifyResult = SSL_get_verify_result(m_ssl.get());
    MORDOR_LOG_LEVEL(g_log, verifyResult ? Log::WARNING : Log::DEBUG) << this
        << " SSL_get_verify_result(" << m_ssl.get() << "): "
        << verifyResult;
    if (verifyResult != X509_V_OK)
        MORDOR_THROW_EXCEPTION(CertificateVerificationException(verifyResult));
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
            MORDOR_THROW_EXCEPTION(CertificateVerificationException(
                X509_V_ERR_APPLICATION_VERIFICATION,
                "No Certificate Presented"));
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
            MORDOR_THROW_EXCEPTION(CertificateVerificationException(
                X509_V_ERR_APPLICATION_VERIFICATION,
                "No Subject Name"));
        int len = X509_NAME_get_text_by_NID(name, NID_commonName, NULL, 0);
        if (len == -1)
            MORDOR_THROW_EXCEPTION(CertificateVerificationException(
                X509_V_ERR_APPLICATION_VERIFICATION,
                "No Common Name"));
        std::string commonName;
        commonName.resize(len);
        X509_NAME_get_text_by_NID(name, NID_commonName, &commonName[0], len + 1);
        if (commonName == wildcardHostname || commonName == hostname)
            return;
        MORDOR_THROW_EXCEPTION(CertificateVerificationException(
                X509_V_ERR_APPLICATION_VERIFICATION,
                "No Matching Common Name"));
    }
}

void
SSLStream::flushBuffer(bool flushParent)
{
    char *writeBuf;
    size_t toWrite = BIO_get_mem_data(m_writeBio, &writeBuf);
    while (toWrite) {
        MORDOR_LOG_DEBUG(g_log) << this << " parent()->write(" << toWrite << ")";
        size_t written = parent()->write(writeBuf, toWrite);
        MORDOR_LOG_DEBUG(g_log) << this << " parent()->write(" << toWrite << "): "
            << written;
        writeBuf += written;
        toWrite -= written;
    }
    int dummy = BIO_reset(m_writeBio);
    dummy = 0;
    if (flushParent)
        parent()->flush();
}

void
SSLStream::wantRead(FiberMutex::ScopedLock &lock)
{
    flushBuffer(true);
    BUF_MEM *bm;
    BIO_get_mem_ptr(m_readBio, &bm);
    MORDOR_ASSERT(bm->length == 0);
    m_readBuffer.consume(bm->max);
    bm->length = bm->max = 0;
    if (m_readBuffer.readAvailable() == 0) {
        if (m_inRead) {
            m_readCondition.wait();
            return;
        }
        m_inRead = true;
        lock.unlock();
        size_t result;
        Buffer temp;
        try {
            MORDOR_LOG_DEBUG(g_log) << this << " parent()->read(32768)";
            result = parent()->read(temp, 32768);
            MORDOR_LOG_DEBUG(g_log) << this << " parent()->read(32768): " << result;
        } catch (...) {
            lock.lock();
            m_inRead = false;
            m_readCondition.broadcast();
            throw;
        }
        lock.lock();
        m_inRead = false;
        m_readCondition.broadcast();
        if (result == 0) {
            BIO_set_mem_eof_return(m_readBio, 0);
            return;
        }
        m_readBuffer.copyIn(temp);
        BIO_get_mem_ptr(m_readBio, &bm);
    }
    MORDOR_ASSERT(m_readBuffer.readAvailable());
    std::vector<iovec> bufs = m_readBuffer.readBufs();
    bm->data = (char *)bufs[0].iov_base;
    bm->length = bm->max =
        (long)std::min<size_t>(0x7fffffff, bufs[0].iov_len);
    MORDOR_LOG_DEBUG(g_log) << this << " wantRead(): " << bm->length;
}

}
