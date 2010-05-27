
#include "mordor/pch.h"
#include "random.h"

#include "mordor/endian.h"

#ifndef WINDOWS
#include <openssl/rand.h>
#include "ssl.h"
#endif

namespace Mordor {

RandomStream::RandomStream()
{
#ifdef WINDOWS
    BOOL ret = ::CryptAcquireContext(&m_hCP, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
    if(!ret || !m_hCP) {
        if(::GetLastError() == NTE_BAD_KEYSET) {
            ret = ::CryptAcquireContext(&m_hCP, NULL, NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET | CRYPT_VERIFYCONTEXT);
            if(!ret || !m_hCP) {
                MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CryptAcquireContext");
            }
        } else {
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CryptAcquireContext");
        }
    }
#endif
}

RandomStream::~RandomStream()
{
#ifdef WINDOWS
    ::CryptReleaseContext(m_hCP, 0);
#endif
}

size_t RandomStream::read(void *buffer, size_t length)
{
#ifdef WINDOWS
    if (length >= 0xffffffffu)
        length = 0xffffffffu;
    if (!::CryptGenRandom(m_hCP, static_cast<DWORD>(length), static_cast<BYTE *>(buffer)))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CryptGenRandom");
#else
    if (!RAND_bytes(static_cast<unsigned char *>(buffer), length))
        MORDOR_THROW_EXCEPTION(OpenSSLException());
#endif
    return length;
}

}
