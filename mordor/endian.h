#ifndef __MORDOR_ENDIAN_H__
#define __MORDOR_ENDIAN_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "version.h"

namespace Mordor {

#ifndef ntohll

#ifdef WINDOWS

#include <stdlib.h>

inline unsigned __int64 ntohll(unsigned __int64 val)
{ return _byteswap_uint64(val); }
inline unsigned __int64 htonll(unsigned __int64 val)
{ return _byteswap_uint64(val); }

#elif defined(OSX)

#include <stdint.h>
#include <sys/_endian.h>

#if __BYTE_ORDER == __LITTLE_ENDIAN

inline uint64_t ntohll(uint64_t val)
{ return _OSSwapInt64(val); }
inline uint64_t htonll(uint64_t val)
{ return _OSSwapInt64(val); }
#else
inline uint64_t ntohll(uint64_t val)
{ return val; }
inline uint64_t htonll(uint64_t val)
{ return val; }
#endif

#elif defined(FREEBSD)

#include <stdint.h>
#include <sys/endian.h>

#if _BYTE_ORDER == _LITTLE_ENDIAN
inline uint64_t ntohll(uint64_t val)
{ return bswap64(val); }
inline uint64_t htonll(uint64_t val)
{ return bswap64(val); }
#else
inline uint64_t ntohll(uint64_t val)
{ return val; }
inline uint64_t htonll(uint64_t val)
{ return val; }
#endif

#else

#include <byteswap.h>
#include <stdint.h>

#if BYTE_ORDER == LITTLE_ENDIAN
inline uint64_t ntohll(uint64_t val)
{ return bswap_64(val); }
inline uint64_t htonll(uint64_t val)
{ return bswap_64(val); }
#else
inline uint64_t ntohll(uint64_t val)
{ return val; }
inline uint64_t htonll(uint64_t val)
{ return val; }
#endif

#endif

#endif

}

#endif
