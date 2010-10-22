#ifndef __MORDOR_ENDIAN_H__
#define __MORDOR_ENDIAN_H__
// Copyright (c) 2009 - Decho Corporation

#include <boost/utility/enable_if.hpp>

#include "version.h"

#define MORDOR_LITTLE_ENDIAN 1
#define MORDOR_BIG_ENDIAN 2

#ifdef WINDOWS
#include <stdlib.h>
#elif defined(OSX)
#include <stdint.h>
#include <sys/_endian.h>
#elif defined(FREEBSD)
#include <stdint.h>
#include <sys/endian.h>
#else
#include <byteswap.h>
#include <stdint.h>
#endif

namespace Mordor {

#ifdef WINDOWS

template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(unsigned __int64), T>::type
byteswap(T value)
{
    return (T)_byteswap_uint64((unsigned __int64)value);
}
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(unsigned long), T>::type
byteswap(T value)
{
    return (T)_byteswap_ulong((unsigned long)value);
}
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(unsigned short), T>::type
byteswap(T value)
{
    return (T)_byteswap_ushort((unsigned short)value);
}

#define MORDOR_BYTE_ORDER MORDOR_LITTLE_ENDIAN

#elif defined(OSX)

template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(uint64_t), T>::type
byteswap(T value)
{
    return (T)_OSSwapInt64((uint64_t)value);
}
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(uint32_t), T>::type
byteswap(T value)
{
    return (T)_OSSwapInt32((uint32_t)value);
}
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(uint16_t), T>::type
byteswap(T value)
{
    return (T)_OSSwapInt16((uint16_t)value);
}

#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
#error Do not know the endianess of this architecture
#endif
    
#ifdef __BIG_ENDIAN__
#define MORDOR_BYTE_ORDER MORDOR_BIG_ENDIAN
#else
#define MORDOR_BYTE_ORDER MORDOR_LITTLE_ENDIAN
#endif

#elif defined(FREEBSD)

template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(uint64_t), T>::type
byteswap(T value)
{
    return (T)bswap64((uint64_t)value);
}
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(uint32_t), T>::type
byteswap(T value)
{
    return (T)bswap32((uint32_t)value);
}
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(uint16_t), T>::type
byteswap(T value)
{
    return (T)bswap16((uint16_t)value);
}

#if _BYTE_ORDER == _BIG_ENDIAN
#define MORDOR_BYTE_ORDER MORDOR_BIG_ENDIAN
#else
#define MORDOR_BYTE_ORDER MORDOR_LITTLE_ENDIAN
#endif

#else

template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(uint64_t), T>::type
byteswap(T value)
{
    return (T)bswap_64((uint64_t)value);
}
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(uint32_t), T>::type
byteswap(T value)
{
    return (T)bswap_32((uint32_t)value);
}
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(uint16_t), T>::type
byteswap(T value)
{
    return (T)bswap_16((uint16_t)value);
}

#if BYTE_ORDER == BIG_ENDIAN
#define MORDOR_BYTE_ORDER MORDOR_BIG_ENDIAN
#else
#define MORDOR_BYTE_ORDER MORDOR_LITTLE_ENDIAN
#endif

#endif

#if MORDOR_BYTE_ORDER == MORDOR_BIG_ENDIAN
template <class T>
T byteswapOnLittleEndian(T t)
{
    return t;
}

template <class T>
T byteswapOnBigEndian(T t)
{
    return byteswap(t);
}
#else
/// byteswap only when running on a little endian platform
///
/// On big endian platforms, it's a no op.  This is the equivalent of
/// htonX/ntohX
template <class T>
T byteswapOnLittleEndian(T t)
{
    return byteswap(t);
}

/// byteswap only when running on a big endian platform
///
/// On little endian platforms, it's a no op.
template <class T>
T byteswapOnBigEndian(T t)
{
    return t;
}
#endif

}

#endif
