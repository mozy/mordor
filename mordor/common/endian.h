#ifndef __ENDIAN_H__
#define __ENDIAN_H__
// Copyright (c) 2009 - Decho Corp.

#include "version.h"

#ifdef WINDOWS

#include <stdlib.h>

#define ntohll(x) _byteswap_uint64(x)
#define htonll(x) _byteswap_uint64(x)

#elif defined(OSX)

#include <sys/_endian.h>

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define ntohll(x) _OSSwapInt64(x)
#define htonll(x) _OSSwapInt64(x)
#else
#define ntohll(x) (x)
#define htonll(x) (x)
#endif

#elif defined(FREEBSD)

#include <sys/endian.h>

#if _BYTE_ORDER == _LITTLE_ENDIAN
#define ntohll(x) bswap64(x)
#define htonll(x) bswap64(x)
#else
#define ntohll(x) (x)
#define htonll(x) (x)
#endif

#else

#include <byteswap.h>

#if BYTE_ORDER == LITTLE_ENDIAN
#define ntohll(x) bswap_64(x)
#define htonll(x) bswap_64(x)
#else
#define ntohll(x) (x)
#define htonll(x) (x)
#endif

#endif


#endif
