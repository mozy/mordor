#include "crypto.h"
#include "ssl.h" // for OpenSSLException
#include "mordor/assert.h"
#include "mordor/streams/random.h"

namespace Mordor {

#define SSL_CHECK(x) if (!(x)) MORDOR_THROW_EXCEPTION(OpenSSLException()); else (void)0

const std::string CryptoStream::RANDOM_IV;

CryptoStream::CryptoStream(Stream::ptr p, const EVP_CIPHER *cipher, const std::string &key,
                           const std::string &iv, Direction dir, Operation op, bool own) :
    MutatingFilterStream(p, own),
    m_iv(iv),
    m_dir(dir),
    m_op(op),
    m_eof(false),
    m_iv_to_extract(0)
{
    if (m_dir == INFER) {
        MORDOR_ASSERT( parent()->supportsRead() ^ parent()->supportsWrite() );
        m_dir = parent()->supportsWrite() ? WRITE : READ;
    }
    if (m_op == AUTO) {
        m_op = (m_dir == WRITE) ? ENCRYPT : DECRYPT;
    }
    EVP_CIPHER_CTX_init(&m_ctx);
    try
    {
        // do preliminary initialization (everything except the IV)
        SSL_CHECK( EVP_CipherInit_ex(&m_ctx, cipher, NULL, NULL, NULL, (m_op == ENCRYPT) ? 1 : 0) );
        SSL_CHECK( EVP_CIPHER_CTX_set_key_length(&m_ctx, static_cast<int>(key.size())) );
        SSL_CHECK( EVP_CipherInit_ex(&m_ctx, NULL, NULL, (const unsigned char *)key.c_str(), NULL, -1) );
        m_blocksize = EVP_CIPHER_CTX_block_size(&m_ctx);

        // generate an IV, if necessary
        size_t iv_len = static_cast<size_t>(EVP_CIPHER_CTX_iv_length(&m_ctx));
        if (&iv == &RANDOM_IV) {
            if (m_op == ENCRYPT) {
                RandomStream random;
                random.read(m_buf, iv_len);
                MORDOR_ASSERT(m_buf.readAvailable() == iv_len);
                m_iv.assign((const char *)m_buf.readBuffer(iv_len, true).iov_base, iv_len);
                init_iv();
                // leave the IV in m_buf;
                // read() will return it ahead of the ciphertext;
                // write() will write it to the parent stream on its first call
            } else {
                // tell read() and write() how much data should be extracted for the iv
                m_iv_to_extract = iv_len;
            }
        } else {
            init_iv();
        }
    }
    catch(...)
    {
        EVP_CIPHER_CTX_cleanup(&m_ctx);
        throw;
    }
}

void CryptoStream::init_iv()
{
    // note: I used to verify that, if m_iv.empty(), EVP_CIPHER_CTX_iv_length returns 0
    // however, some older versions of OpenSSL return a nonzero IV length for ECB mode

    if (!m_iv.empty()) {
        // make sure the size is correct
        if (static_cast<size_t>(EVP_CIPHER_CTX_iv_length(&m_ctx)) != m_iv.size())
            MORDOR_THROW_EXCEPTION(OpenSSLException("incorrect iv length"));

        // feed openssl the IV
        SSL_CHECK( EVP_CipherInit_ex(&m_ctx, NULL, NULL, NULL, (const unsigned char *)m_iv.c_str(), -1) );

        // clear data we don't need anymore
        m_iv.clear();
    }
}

CryptoStream::~CryptoStream()
{
    EVP_CIPHER_CTX_cleanup(&m_ctx);
}

void CryptoStream::close(CloseType type)
{
    if (!m_eof && (type == Stream::WRITE || type == BOTH)) {
        finalize();
        m_eof = true;
    }

    if (ownsParent())
        parent()->close(type);
}

size_t CryptoStream::read(Buffer &out, size_t len)
{
    MORDOR_ASSERT( m_dir == READ );

    size_t copied = 0;

    for(;;)
    {
        // copy out [de]crypted data
        size_t to_copy = (std::min)(m_buf.readAvailable(), len - copied);
        if (to_copy > 0) {
            out.copyIn(m_buf, to_copy);
            m_buf.consume(to_copy);
            copied += to_copy;
        }
        if (m_eof || copied == len)
            return copied;
        MORDOR_ASSERT( m_buf.readAvailable() == 0 );

        // m_tmp has no content between calls; it's a member variable
        // solely to reduce allocations/deallocations
        MORDOR_ASSERT( m_tmp.readAvailable() == 0 );

        size_t to_read = len - copied;
        // make sure to read enough that we can make progress
        to_read = (std::max)(to_read, 2 * m_blocksize + m_iv_to_extract);
        while(to_read > 0) {
            size_t read = parent()->read(m_tmp, to_read);
            if (read == 0)
                break;
            to_read -= read;
        }

        // initialize the IV, if we haven't done that yet
        if (m_iv_to_extract > 0) {
            if (m_tmp.readAvailable() < m_iv_to_extract)
                MORDOR_THROW_EXCEPTION(OpenSSLException("missing iv"));
            m_iv.assign( (char *)m_tmp.readBuffer(m_iv_to_extract, true).iov_base,
                m_iv_to_extract );
            m_tmp.consume(m_iv_to_extract);
            m_iv_to_extract = 0;
            init_iv();
        }

        // encrypt/decrypt some data
        cipher(m_tmp, m_buf, m_tmp.readAvailable());
        m_tmp.consume(m_tmp.readAvailable());

        // check for EOF
        if (m_buf.readAvailable() == 0) {
            final(m_buf);
            m_eof = true;
        }
    }
}

// ciphers len bytes from src, skipping over skip bytes at the front of the buffer
size_t CryptoStream::cipher(const Buffer &src, Buffer &dst, size_t len, size_t skip)
{
    MORDOR_ASSERT(skip <= len);
    len -= skip;
    if (len == 0)
        return 0;
    int outlen = static_cast<int>(len) + m_blocksize;
    SSL_CHECK(EVP_CipherUpdate(&m_ctx,
        (unsigned char *)dst.writeBuffer(len + m_blocksize, true).iov_base, &outlen,
        (unsigned char *)src.readBuffer(len + skip, true).iov_base + skip, static_cast<int>(len)));
    dst.produce(outlen);
    return outlen;
}

// finalizes the cipher and writes the last few bytes to dst
size_t CryptoStream::final(Buffer &dst)
{
    int outlen = m_blocksize;
    SSL_CHECK(EVP_CipherFinal(&m_ctx,
        (unsigned char *)dst.writeBuffer(m_blocksize, true).iov_base, &outlen));
    dst.produce(outlen);
    return outlen;
}

// writes and consumes entire buffer
void CryptoStream::write_buffer(Buffer &buffer)
{
    size_t to_write = buffer.readAvailable();
    while(to_write > 0) {
        size_t written = parent()->write(buffer, to_write);
        buffer.consume(written);
        to_write -= written;
    }
}

size_t CryptoStream::write(const Buffer &buffer, size_t len)
{
    MORDOR_ASSERT( m_dir == WRITE );

    size_t iv_skip = 0;
    if (m_iv_to_extract > 0) {
        // seed the IV, if we haven't done so yet
        MORDOR_ASSERT(m_op == DECRYPT);
        iv_skip = (std::min)(m_iv_to_extract, len);
        m_buf.copyIn(buffer, iv_skip);
        m_iv_to_extract -= iv_skip;
        if (m_iv_to_extract > 0)
            return len; // don't have the whole IV yet
        // now we have an IV, so we can initialize the cipher
        size_t iv_len = static_cast<size_t>(EVP_CIPHER_CTX_iv_length(&m_ctx));
        MORDOR_ASSERT(m_buf.readAvailable() == iv_len);
        m_iv.assign((char *)m_buf.readBuffer(iv_len, true).iov_base, iv_len);
        m_buf.clear();
        init_iv();
        if (iv_skip == len)
            return len; // have the IV but no payload yet
    } else if (m_buf.readAvailable() > 0) {
        // write the IV, if we haven't done so yet
        MORDOR_ASSERT(m_op == ENCRYPT);
        write_buffer(m_buf);
        MORDOR_ASSERT( m_buf.readAvailable() == 0 );
    }

    // now cipher and write the payload
    MORDOR_ASSERT( m_tmp.readAvailable() == 0 );
    cipher(buffer, m_tmp, len, iv_skip);
    write_buffer(m_tmp);
    MORDOR_ASSERT( m_tmp.readAvailable() == 0 );
    return len;
}

void CryptoStream::finalize()
{
    if (!m_eof && m_dir == WRITE) {
        // if we're encrypting, and we haven't written an IV
        // (i.e., because the user never called write(),
        //  because the file is empty) then do that now
        if (m_buf.readAvailable() > 0) {
            MORDOR_ASSERT(m_op == ENCRYPT);
            write_buffer(m_buf);
            MORDOR_ASSERT( m_buf.readAvailable() == 0 );
        }

        // finalize the cipher (if we actually finished initializing it;
        // if the caller never wrote the ciphertext with the leading IV,
        // then we never even started)
        if (m_iv_to_extract == 0) {
            MORDOR_ASSERT( m_tmp.readAvailable() == 0 );
            final(m_tmp);
            write_buffer(m_tmp);
            MORDOR_ASSERT( m_tmp.readAvailable() == 0 );
        }

        m_eof = true;
    }
}

}
