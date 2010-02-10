#ifndef __MORDOR_RANDOM_STREAM_H__
#define __MORDOR_RANDOM_STREAM_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "stream.h"

// a stream to generate random data
// uses CryptGenRandom() on WINDOWS; OpenSSL RAND_bytes elsewhere (seeded automatically by /dev/urandom)

#ifdef WINDOWS
#include <wincrypt.h>
#endif

namespace Mordor {

class RandomStream : public Stream
{
public:
    RandomStream();
    ~RandomStream();

    bool supportsRead() { return true; }
    size_t read(Buffer &buffer, size_t length);
    size_t read(void *buffer, size_t length);

private:
#ifdef WINDOWS
    HCRYPTPROV m_hCP;
#endif
};

}

#endif // __MORDOR_RANDOM_STREAM_H__

