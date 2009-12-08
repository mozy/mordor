
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
    
size_t RandomStream::read(Buffer &buffer, size_t length)
{
    Buffer::SegmentData data = buffer.writeBuf(length);
#ifdef WINDOWS
    if (!::CryptGenRandom(m_hCP, static_cast<DWORD>(length), static_cast<BYTE *>(data.start())))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CryptGenRandom");
#else
    if (!RAND_bytes(static_cast<unsigned char *>(data.start()), length))
        MORDOR_THROW_EXCEPTION(OpenSSLException());
#endif
    buffer.produce(length);
    return length;
}

}
