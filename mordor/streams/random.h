#ifndef __MORDOR_RANDOM_STREAM_H__
#define __MORDOR_RANDOM_STREAM_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "stream.h"

#ifdef WINDOWS
#include <wincrypt.h>
#endif

namespace Mordor {

/// a stream to generate random data
/// uses CryptGenRandom() on WINDOWS; OpenSSL RAND_bytes elsewhere (seeded automatically by /dev/urandom)
/// @note RandomStream is not guaranteed to be thread safety.
///   On Windows platform, the thread safety depends on the cryptographic service provider implementation
///   On Linux/MacOS platform, it depends on openssl thread safety. By default it isn't, use OpensslLockManager
///   to ensure the thread safety.
class RandomStream : public Stream
{
public:
    typedef boost::shared_ptr<RandomStream> ptr;

    RandomStream();
    ~RandomStream();

    bool supportsRead() { return true; }
    size_t read(void *buffer, size_t length);
    using Stream::read;

    bool supportsSeek() { return true; }
    long long seek(long long offset, Anchor anchor = BEGIN) { return 0; }

private:
#ifdef WINDOWS
    HCRYPTPROV m_hCP;
#endif
};

}

#endif // __MORDOR_RANDOM_STREAM_H__

