// Copyright (c) 2009 - Mozy, Inc.

#include "ssl.h"

#include <sstream>

#include <openssl/err.h>
#include <openssl/x509v3.h>

#include "mordor/assert.h"
#include "mordor/log.h"
#include "mordor/util.h"

#ifdef MSVC
#pragma comment(lib, "libeay32")
#pragma comment(lib, "ssleay32")
#endif

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:streams:ssl");

namespace {

static struct SSLInitializer {
    SSLInitializer()
    {
        SSL_library_init();
        SSL_load_error_strings();
    }
    ~SSLInitializer()
    {
        ERR_free_strings();
        CRYPTO_cleanup_all_ex_data();
        EVP_cleanup();
    }
} g_init;

}

static bool hasOpenSSLError()
{
    unsigned long err = ERR_peek_error();
    if (err == SSL_ERROR_NONE)
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
    while ( (err = ERR_get_error()) != SSL_ERROR_NONE) {
        if (!os.str().empty())
            os << "\n";
        os << ERR_error_string(err, buf);
    }
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
: MutatingFilterStream(parent, own)
{
    MORDOR_ASSERT(parent);
    clearSSLError();
    if (ctx)
        m_ctx.reset(ctx, &nop<SSL_CTX *>);
    else
        m_ctx.reset(SSL_CTX_new(client ? SSLv23_client_method() :
            SSLv23_server_method()), &SSL_CTX_free);
    if (!m_ctx) {
        MORDOR_ASSERT(hasOpenSSLError());
        MORDOR_THROW_EXCEPTION(OpenSSLException(getOpenSSLErrorMessage()))
            << boost::errinfo_api_function("SSL_CTX_new");
    }
    // Auto-generate self-signed server cert
    if (!ctx && !client) {
        boost::shared_ptr<X509> cert;
        boost::shared_ptr<EVP_PKEY> pkey;
        mkcert(cert, pkey, 1024, rand(), 365);
        SSL_CTX_use_certificate(m_ctx.get(), cert.get());
        SSL_CTX_use_PrivateKey(m_ctx.get(), pkey.get());
    }
    m_ssl.reset(SSL_new(m_ctx.get()), &SSL_free);
    if (!m_ssl) {
        MORDOR_ASSERT(hasOpenSSLError());
        MORDOR_THROW_EXCEPTION(OpenSSLException(getOpenSSLErrorMessage()))
            << boost::errinfo_api_function("SSL_CTX_new");
    }
    m_readBio = BIO_new(BIO_s_mem());
    m_writeBio = BIO_new(BIO_s_mem());
    if (!m_readBio || !m_writeBio) {
        if (m_readBio) BIO_free(m_readBio);
        if (m_writeBio) BIO_free(m_writeBio);
        MORDOR_ASSERT(hasOpenSSLError());
        MORDOR_THROW_EXCEPTION(OpenSSLException(getOpenSSLErrorMessage()))
            << boost::errinfo_api_function("BIO_new");
    }
    BIO_set_mem_eof_return(m_readBio, -1);

    SSL_set_bio(m_ssl.get(), m_readBio, m_writeBio);
}

void
SSLStream::clearSSLError()
{
    std::string msg;
    unsigned long err = SSL_ERROR_NONE;
    while ((err = ERR_get_error()) != SSL_ERROR_NONE) {
        switch (ERR_GET_REASON(err)) {
            case ERR_R_MALLOC_FAILURE:
                msg = "bad alloc";
                break;
            case ERR_R_PASSED_NULL_PARAMETER:
                msg = "invalid argument";
                break;
            default:
                {
                    char buf[120];
                    const char * errBuf = ERR_error_string(err, buf);
                    if (errBuf != NULL) {
                        msg = errBuf;
                    }
                }
        }
        MORDOR_LOG_ERROR(g_log) << this
                << " ssl: " << m_ssl.get()
                << " ignoring error: " << err
                << " error msg: " << msg;
    };

    // Clear it again for insurance.
    ERR_clear_error();
}

void
SSLStream::close(CloseType type)
{
    MORDOR_ASSERT(type == BOTH);
    if (!(sslCallWithLock(boost::bind(SSL_get_shutdown, m_ssl.get()), NULL) & SSL_SENT_SHUTDOWN)) {
        unsigned long error = SSL_ERROR_NONE;
        const int result = sslCallWithLock(boost::bind(SSL_shutdown, m_ssl.get()), &error);
        if (result <= 0) {
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
                    MORDOR_LOG_ERROR(g_log) << this << " SSL_shutdown("
                        << m_ssl.get() << "): " << result << " (" << error
                        << ")";
                    if (result == 0)
                        break;
                    MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("SSL_shutdown");
                case SSL_ERROR_SSL:
                    {
                        MORDOR_ASSERT(hasOpenSSLError());
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
        flush(false);
    }

    while (!(sslCallWithLock(boost::bind(SSL_get_shutdown, m_ssl.get()), NULL) & SSL_RECEIVED_SHUTDOWN)) {
        unsigned long error = SSL_ERROR_NONE;
        const int result = sslCallWithLock(boost::bind(SSL_shutdown, m_ssl.get()), &error);
        MORDOR_LOG_DEBUG(g_log) << this << " SSL_shutdown(" << m_ssl.get()
            << "): " << result << " (" << error << ")";
        if (result > 0) {
            break;
        }
        switch (error) {
            case SSL_ERROR_NONE:
            case SSL_ERROR_ZERO_RETURN:
                break;
            case SSL_ERROR_WANT_READ:
                flush();
                wantRead();
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
                MORDOR_LOG_WARNING(g_log) << this << " SSL_shutdown(" << m_ssl.get()
                    << "): " << result << " (" << error << ")";
                if (result == 0) {
                    break;
                }
                MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("SSL_shutdown");
            case SSL_ERROR_SSL:
                {
                    MORDOR_ASSERT(hasOpenSSLError());
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
        break;
    }
    parent()->close();
}

size_t
SSLStream::read(void *buffer, size_t length)
{
    const int toRead = (int)std::min<size_t>(0x0fffffff, length);
    while (true) {
        unsigned long error = SSL_ERROR_NONE;
        const int result = sslCallWithLock(boost::bind(SSL_read, m_ssl.get(), buffer, toRead), &error);
        if (result > 0) {
            return result;
        }
        MORDOR_LOG_DEBUG(g_log) << this << " SSL_read(" << m_ssl.get() << ", "
            << toRead << "): " << result << " (" << error << ")";
        switch (error) {
            case SSL_ERROR_NONE:
                return result;
            case SSL_ERROR_ZERO_RETURN:
                // Received close_notify message
                MORDOR_ASSERT(result == 0);
                return 0;
            case SSL_ERROR_WANT_READ:
                wantRead();
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
                MORDOR_LOG_WARNING(g_log) << this << " SSL_read("
                    << m_ssl.get() << ", " << toRead << "): " << result
                    << " (" << error << ")";
                if (result == 0) {
                    return 0;
                }
                MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("SSL_read");
            case SSL_ERROR_SSL:
                {
                    MORDOR_ASSERT(hasOpenSSLError());
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
SSLStream::write(const Buffer &buffer, size_t length)
{
    // SSL_write will create at least two SSL records for each call -
    // one for data, and one tiny one for the checksum or IV or something.
    // Dealing with lots of extra records can take some serious CPU time
    // server-side, so we want to provide it with as much data as possible,
    // even if that means reallocating.  That's why we use pass the flag to
    // coalesce small segments, instead of only doing the first available
    // segment
    return Stream::write(buffer, length, true);
}

size_t
SSLStream::write(const void *buffer, size_t length)
{
    flush(false);
    if (length == 0)
        return 0;

    const int toWrite = (int)std::min<size_t>(0x7fffffff, length);
    while (true) {
        unsigned long error = SSL_ERROR_NONE;
        const int result = sslCallWithLock(boost::bind(SSL_write, m_ssl.get(), buffer, toWrite), &error);
        if (result > 0) {
            return result;
        }

        MORDOR_LOG_DEBUG(g_log) << this << " SSL_write(" << m_ssl.get() << ", "
            << toWrite << "): " << result << " (" << error << ")";
        switch (error) {
            case SSL_ERROR_NONE:
                return result;
            case SSL_ERROR_ZERO_RETURN:
                // Received close_notify message
                MORDOR_ASSERT(result != 0);
                return result;
            case SSL_ERROR_WANT_READ:
                MORDOR_THROW_EXCEPTION(OpenSSLException("SSL_write generated SSL_ERROR_WANT_READ"));
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
                MORDOR_LOG_ERROR(g_log) << this << " SSL_write("
                    << m_ssl.get() << ", " << toWrite << "): " << result
                    << " (" << error << ")";
                if (result == 0) {
                    MORDOR_THROW_EXCEPTION(UnexpectedEofException());
                }
                MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("SSL_write");
            case SSL_ERROR_SSL:
                {
                    MORDOR_ASSERT(hasOpenSSLError());
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
    static const int WRITE_BUF_LENGTH = 4096;
    char writeBuf[WRITE_BUF_LENGTH];
    int toWrite = 0;
    do {
        toWrite = BIO_read(m_writeBio, (void *)writeBuf, WRITE_BUF_LENGTH);
        if (toWrite > 0) {
            m_writeBuffer.copyIn((const void *)writeBuf, toWrite);
        }
    } while (toWrite > 0);

    if (m_writeBuffer.readAvailable() == 0)
        return;

    while (m_writeBuffer.readAvailable()) {
        MORDOR_LOG_TRACE(g_log) << this << " parent()->write("
            << m_writeBuffer.readAvailable() << ")";
        size_t written = parent()->write(m_writeBuffer,
            m_writeBuffer.readAvailable());
        MORDOR_LOG_TRACE(g_log) << this << " parent()->write("
            << m_writeBuffer.readAvailable() << "): " << written;
        m_writeBuffer.consume(written);
    }

    if (flushParent)
        parent()->flush(flushParent);
}

void
SSLStream::accept()
{
    while (true) {
        unsigned long error = SSL_ERROR_NONE;
        const int result = sslCallWithLock(boost::bind(SSL_accept, m_ssl.get()), &error);
        if (result > 0) {
            flush(false);
            return;
        }
        MORDOR_LOG_DEBUG(g_log) << this << " SSL_accept(" << m_ssl.get()
            << "): " << result << " (" << error << ")";
        switch (error) {
            case SSL_ERROR_NONE:
                flush(false);
                return;
            case SSL_ERROR_ZERO_RETURN:
                // Received close_notify message
                return;
            case SSL_ERROR_WANT_READ:
                flush();
                wantRead();
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
                MORDOR_LOG_ERROR(g_log) << this << " SSL_accept("
                    << m_ssl.get() << "): " << result << " (" << error
                    << ")";
                if (result == 0) {
                    MORDOR_THROW_EXCEPTION(UnexpectedEofException());
                }
                MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("SSL_accept");
            case SSL_ERROR_SSL:
                {
                    MORDOR_ASSERT(hasOpenSSLError());
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
    while (true) {
        unsigned long error = SSL_ERROR_NONE;
        const int result = sslCallWithLock(boost::bind(SSL_connect, m_ssl.get()), &error);
        MORDOR_LOG_DEBUG(g_log) << this << " SSL_connect(" << m_ssl.get()
            << "): " << result << " (" << error << ")";
        if (result > 0) {
            flush(false);
            return;
        }
        switch (error) {
            case SSL_ERROR_NONE:
                flush(false);
                return;
            case SSL_ERROR_ZERO_RETURN:
                // Received close_notify message
                return;
            case SSL_ERROR_WANT_READ:
                flush();
                wantRead();
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
                MORDOR_LOG_ERROR(g_log) << this << " SSL_connect("
                    << m_ssl.get() << "): " << result << " (" << error
                    << ")";
                if (result == 0) {
                    MORDOR_THROW_EXCEPTION(UnexpectedEofException());
                }
                MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("SSL_connect");
            case SSL_ERROR_SSL:
                {
                    MORDOR_ASSERT(hasOpenSSLError());
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
SSLStream::serverNameIndication(const std::string &hostname)
{
    // Older versions of OpenSSL don't support this (I'm looking at you,
    // Leopard); just ignore it then
#ifdef SSL_set_tlsext_host_name
    boost::mutex::scoped_lock lock(m_mutex);
    if (!SSL_set_tlsext_host_name(m_ssl.get(), hostname.c_str())) {
        if (!hasOpenSSLError()) return;
        std::string message = getOpenSSLErrorMessage();
        MORDOR_LOG_ERROR(g_log) << this << " SSL_set_tlsext_host_name("
            << m_ssl.get() << ", " << hostname.c_str() << "): " << message;
        MORDOR_THROW_EXCEPTION(OpenSSLException(message))
            << boost::errinfo_api_function("SSL_set_tlsext_host_name");
    }
#endif
}

void
SSLStream::verifyPeerCertificate()
{
    const long verifyResult = sslCallWithLock(boost::bind(SSL_get_verify_result, m_ssl.get()), NULL);
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
        boost::mutex::scoped_lock lock(m_mutex);
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
        GENERAL_NAMES *gens = (GENERAL_NAMES *)X509_get_ext_d2i(cert.get(),
            NID_subject_alt_name, &critical, &altNameIndex);
        if (gens) {
            do {
                try {
                    bool success = false;
                    for(int i = 0; i < sk_GENERAL_NAME_num(gens); i++)
                    {
                        GENERAL_NAME *gen = sk_GENERAL_NAME_value(gens, i);
                        if(gen->type != GEN_DNS) continue;
                        std::string altName((const char *)gen->d.dNSName->data,
                            gen->d.dNSName->length);
                        if (altName == wildcardHostname ||
                            altName == hostname) {
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
                gens = (GENERAL_NAMES *)X509_get_ext_d2i(cert.get(),
                    NID_subject_alt_name, &critical, &altNameIndex);
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
        X509_NAME_get_text_by_NID(name, NID_commonName, &commonName[0],
            len + 1);
        if (commonName == wildcardHostname || commonName == hostname)
            return;
        MORDOR_THROW_EXCEPTION(CertificateVerificationException(
                X509_V_ERR_APPLICATION_VERIFICATION,
                "No Matching Common Name"));
    }
}

void
SSLStream::wantRead()
{
    if (m_readBuffer.readAvailable() == 0) {
        MORDOR_LOG_TRACE(g_log) << this << " parent()->read(32768)";
        const size_t result = parent()->read(m_readBuffer, 32768);
        MORDOR_LOG_TRACE(g_log) << this << " parent()->read(32768): " << result;
        if (result == 0) {
            BIO_set_mem_eof_return(m_readBio, 0);
            return;
        }
    }
    MORDOR_ASSERT(m_readBuffer.readAvailable());
    const iovec iov = m_readBuffer.readBuffer(~0, false);
    MORDOR_ASSERT(iov.iov_len > 0);
    const int written = BIO_write(m_readBio, (char *)iov.iov_base, iov.iov_len);
    MORDOR_ASSERT(written > 0);
    if (written > 0) {
        m_readBuffer.consume(written);
    }
    MORDOR_LOG_DEBUG(g_log) << this << " wantRead(): " << written;
}

int
SSLStream::sslCallWithLock(boost::function<int ()> dg, unsigned long *error)
{
    boost::mutex::scoped_lock lock(m_mutex);

    // If error is NULL, it means that sslCallWithLock is not supposed to call SSL_get_error
    // after dg got called. If SSL_get_error is not supposed to be called, there is no need
    // to clear current thread's error queue.
    if (error == NULL) {
        return dg();
    }

    clearSSLError();
    const int result = dg();
    if (result <= 0) {
        *error = SSL_get_error(m_ssl.get(), result);
    }
    return result;
}

}
