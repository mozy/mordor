#include "crypto.h"
#include "ssl.h" // for OpenSSLException
#include "mordor/assert.h"

namespace Mordor {

#define SSL_CHECK(x) if (!(x)) MORDOR_THROW_EXCEPTION(OpenSSLException()); else (void)0

CryptoStream::CryptoStream(Stream::ptr p, const EVP_CIPHER *cipher, const std::string &key,
                           const std::string &iv, Direction dir, Operation op, bool own) :
    MutatingFilterStream(p, own),
    m_dir(dir),
    m_op(op),
    m_eof(true)
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
        int operation = (m_op == ENCRYPT) ? 1 : 0;
        SSL_CHECK( EVP_CipherInit_ex(&m_ctx, cipher, NULL, NULL, NULL, operation) );
        SSL_CHECK( EVP_CIPHER_CTX_set_key_length(&m_ctx, static_cast<int>(key.size())) );
        if (static_cast<size_t>(EVP_CIPHER_CTX_iv_length(&m_ctx)) != iv.size())
            MORDOR_THROW_EXCEPTION(OpenSSLException("incorrect iv length"));
        SSL_CHECK( EVP_CipherInit_ex(&m_ctx, NULL, NULL,
            (const unsigned char *)key.c_str(), (const unsigned char *)iv.c_str(),
            operation) );
        m_eof = false;
        m_blocksize = EVP_CIPHER_CTX_block_size(&m_ctx);
    }
    catch(...)
    {
        EVP_CIPHER_CTX_cleanup(&m_ctx);
        throw;
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
        // round up to a full encryption block
        to_read += (m_blocksize - (to_read % m_blocksize)) % m_blocksize;
        while(to_read > 0) {
            size_t read = parent()->read(m_tmp, to_read);
            if (read == 0)
                break;
            to_read -= read;
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

// ciphers len bytes from src
size_t CryptoStream::cipher(const Buffer &src, Buffer &dst, size_t len)
{
    int outlen = static_cast<int>(len) + m_blocksize;
    SSL_CHECK(EVP_CipherUpdate(&m_ctx,
        (unsigned char *)dst.writeBuffer(len + m_blocksize, true).iov_base, &outlen,
        (unsigned char *)src.readBuffer(len, true).iov_base, static_cast<int>(len)));
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

    MORDOR_ASSERT( m_tmp.readAvailable() == 0 );
    cipher(buffer, m_tmp, len);
    write_buffer(m_tmp);
    MORDOR_ASSERT( m_tmp.readAvailable() == 0 );
    return len;
}

void CryptoStream::finalize()
{
    if (!m_eof && m_dir == WRITE) {
        MORDOR_ASSERT( m_tmp.readAvailable() == 0 );
        final(m_tmp);
        write_buffer(m_tmp);
        m_eof = true;
        MORDOR_ASSERT( m_tmp.readAvailable() == 0 );
    }
}

}
