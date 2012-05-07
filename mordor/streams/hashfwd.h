#ifndef __MORDOR_HASH_STREAM_FWD_H__
#define __MORDOR_HASH_STREAM_FWD_H__


namespace Mordor {
    class HashStream;
    enum HASH_TYPE
    {
        MD5    = 0,
#ifndef OPENSSL_NO_SHA0
        SHA0   = 1,
#endif
#ifndef OPENSSL_NO_SHA1
        SHA1   = 2,
#endif
#ifndef OPENSSL_NO_SHA256
        SHA224 = 3,
        SHA256 = 4,
#endif
#ifndef OPENSSL_NO_SHA512
        SHA384 = 5,
        SHA512 = 6,
#endif
    };

    template<HASH_TYPE H> class _HashStream;
#ifndef OPENSSL_NO_SHA0
    typedef _HashStream<SHA0>   SHA0Stream;
#endif
#ifndef OPENSSL_NO_SHA1
    typedef _HashStream<SHA1>   SHA1Stream;
#endif
#ifndef OPENSSL_NO_SHA256
    typedef _HashStream<SHA224> SHA224Stream;
    typedef _HashStream<SHA256> SHA256Stream;
#endif
    typedef _HashStream<MD5>    MD5Stream;

    class CRC32Stream;
}

#endif
