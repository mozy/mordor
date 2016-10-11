#include "mordor/streams/stdcrypto.h"
#include "mordor/assert.h"
#include "mordor/streams/cat.h"
#include "mordor/streams/crypto.h"
#include "mordor/streams/memory.h"
#include "mordor/streams/random.h"
#include "mordor/streams/ssl.h"
#include "mordor/string.h"

namespace Mordor {

static const std::string SALT_MAGIC("Salted__");

/// derive @p key and @p iv from @p pass and @p salt, salt.size() == 0 means no salt involved
static void deriveKeyIV(const EVP_CIPHER *cipher, const std::string &pass, const std::string &salt, std::string &key, std::string &iv)
{
    const unsigned char * psalt = NULL;
    if (!salt.empty())
        psalt = (const unsigned char *)salt.c_str();
    key.resize(EVP_MAX_KEY_LENGTH);
    iv.resize(EVP_MAX_IV_LENGTH);
    size_t key_size = EVP_BytesToKey(cipher, EVP_md5(), psalt, (const unsigned char *)pass.c_str(),
                             pass.size(), 1, (unsigned char *)&key[0], (unsigned char *) &iv[0]);
    if (key_size == 0)
        MORDOR_THROW_EXCEPTION(OpenSSLException("key derivation fail"));
    else
        key.resize(key_size);
}

StdCryptoStream::StdCryptoStream(Stream::ptr p, const EVP_CIPHER *cipher, const std::string &pass,
    bool nosalt, Direction dir, Operation op, bool own)
    : MutatingFilterStream(p, own)
    , m_cipher(cipher)
    , m_pass(pass)
    , m_nosalt(nosalt)
    , m_dir(dir)
    , m_op(op)
    , m_extractSalt(false)
{
    if (m_dir == INFER) {
        MORDOR_ASSERT( parent()->supportsRead() ^ parent()->supportsWrite() );
        m_dir = parent()->supportsWrite() ? WRITE : READ;
    }
    if (m_op == AUTO) {
        m_op = (m_dir == WRITE) ? ENCRYPT : DECRYPT;
    }
    if (nosalt) {
        parent(buildCryptoStream(p, ""));
    } else { // need salt
        if (m_op == ENCRYPT) { // generate salt from random stream
            RandomStream random;
            random.read(m_buffer, PKCS5_SALT_LEN);
            MORDOR_ASSERT(m_buffer.readAvailable() == PKCS5_SALT_LEN);
            std::string salt = m_buffer.toString();
            m_buffer.clear();
            if (m_dir == WRITE) {
                // save the magic in m_buffer, and wait for actual write() called
                // so the magic and salt can be written to parent stream and rest
                // data will flow to CryptoStream with given key & iv encryption
                m_buffer.copyIn(SALT_MAGIC);
                m_buffer.copyIn(salt);
                // save the cryptoStream for later write() call
                m_cryptoStream = buildCryptoStream(p, salt);
            } else {
                std::vector<Stream::ptr> streams;
                streams.push_back(Stream::ptr(new MemoryStream(Buffer(SALT_MAGIC + salt))));
                streams.push_back(buildCryptoStream(p, salt));
                parent(Stream::ptr(new CatStream(streams)));
            }
        } else {
            // salt need to extracted from either parent() when read() or buffer when write()
            m_extractSalt = true;
        }
    }
}

Stream::ptr
StdCryptoStream::buildCryptoStream(Stream::ptr p, const std::string &salt)
{
    std::string key, iv;
    deriveKeyIV(m_cipher, m_pass, salt, key, iv);
    return Stream::ptr(new CryptoStream(p, m_cipher, key, iv, (CryptoStream::Direction)m_dir, (CryptoStream::Operation)m_op, ownsParent()));
}

size_t
StdCryptoStream::read(Buffer &buffer, size_t len)
{
    MORDOR_ASSERT(m_dir == READ);
    if (m_extractSalt) {
        // decrypt-on-read, extract salt from parent
        MORDOR_ASSERT(m_op == DECRYPT);
        size_t todo = SALT_MAGIC.size() + PKCS5_SALT_LEN - m_buffer.readAvailable();
        while (todo > 0) {
            size_t size = parent()->read(m_buffer, todo);
            if (size == 0)
                MORDOR_THROW_EXCEPTION(OpenSSLException("bad magic number"));
            todo -= size;
        }
        MORDOR_ASSERT(m_buffer.readAvailable() == SALT_MAGIC.size() + PKCS5_SALT_LEN);
        std::string saltString = m_buffer.toString();
        m_buffer.clear();
        if (saltString.substr(0, SALT_MAGIC.size()) != SALT_MAGIC)
            MORDOR_THROW_EXCEPTION(OpenSSLException("bad magic number"));
        std::string salt = saltString.substr(SALT_MAGIC.size(), saltString.size());
        Stream::ptr cryptoStream = buildCryptoStream(parent(), salt);
        parent(cryptoStream);
        m_extractSalt = false;
        return parent()->read(buffer, len);
    } else {
        return parent()->read(buffer, len);
    }
    return 0;
}

size_t
StdCryptoStream::write(const Buffer &buffer, size_t len)
{
    MORDOR_ASSERT(m_dir == WRITE);
    if (m_extractSalt) {
        // decrypt-on-write: extract the salt from buffer
        MORDOR_ASSERT(m_op == DECRYPT);
        size_t todo = SALT_MAGIC.size() + PKCS5_SALT_LEN - m_buffer.readAvailable();
        MORDOR_ASSERT(todo > 0);
        if (len < todo) {
            m_buffer.copyIn(buffer, len);
            return len;
        } else { // enough data to extract the magic and salt
            m_buffer.copyIn(buffer, todo);
            MORDOR_ASSERT(m_buffer.readAvailable() == SALT_MAGIC.size() + PKCS5_SALT_LEN);
            std::string saltString = m_buffer.toString();
            m_buffer.clear();
            if (saltString.substr(0, SALT_MAGIC.size()) != SALT_MAGIC) {
                MORDOR_THROW_EXCEPTION(OpenSSLException("bad magic number"));
            }
            std::string salt = saltString.substr(SALT_MAGIC.size(), saltString.size());
            Stream::ptr cryptoStream = buildCryptoStream(parent(), salt);
            parent(cryptoStream);
            m_extractSalt = false;
            // make a copy of the rest data in buffer and write to parent stream
            Buffer tmpBuffer(buffer);
            tmpBuffer.consume(todo);
            return parent()->write(tmpBuffer, len - todo) + todo;
        }
    } else if (m_buffer.readAvailable() > 0) {
        // encrypt-on-write: m_buffer holds the generated salt info
        MORDOR_ASSERT(m_op == ENCRYPT);
        while (m_buffer.readAvailable() > 0) {
            size_t size = parent()->write(m_buffer, m_buffer.readAvailable());
            m_buffer.consume(size);
        }
        MORDOR_ASSERT(m_buffer.readAvailable() == 0);
        MORDOR_ASSERT(m_cryptoStream);
        parent(m_cryptoStream);
        m_cryptoStream.reset();
        return parent()->write(buffer, len);
    }
    return parent()->write(buffer, len);
}

void
StdCryptoStream::flush(bool flushParent)
{
    MORDOR_ASSERT(m_dir == WRITE);
    if (m_buffer.readAvailable() > 0) {
        // trigger an empty write to ensure the salt is flushed to parent stream
        write(Buffer(), 0);
    }
    parent()->flush(flushParent);
}

void
StdCryptoStream::close(CloseType type)
{
    if (m_extractSalt)
        MORDOR_THROW_EXCEPTION(UnexpectedEofException());
    // m_buffer content can be ignored in READ mode
    if (m_dir == WRITE && m_buffer.readAvailable() > 0)
        flush(false);
    if (ownsParent())
        parent()->close(type);
}

}
