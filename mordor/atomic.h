#ifndef __MORDOR_ATOMIC_H__
#define __MORDOR_ATOMIC_H__

#include "version.h"

namespace Mordor {

#ifdef WINDOWS
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(LONG), T>::type
atomicDecrement(volatile T& t)
{
    return InterlockedDecrement((volatile LONG*)&t);
}
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(LONG), T>::type
atomicIncrement(volatile T& t) 
{
    return InterlockedIncrement((volatile LONG*)&t);
}
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(LONG), T>::type
atomicAdd(volatile T& t, T v)
{
    return InterlockedExchangeAdd((volatile LONG*)&t, (LONG)v) + v;
}
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(LONG), T>::type
atomicCompareAndSwap(volatile T& t, T newvalue, T comparand)
{
    return InterlockedCompareExchange((volatile LONG*)&t, (LONG)newvalue, (LONG)comparand);
}
#ifdef X86_64
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(LONGLONG), T>::type
atomicDecrement(volatile T& t)
{
    return InterlockedDecrement64((volatile LONGLONG*)&t);
}
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(LONGLONG), T>::type
atomicIncrement(volatile T& t) 
{
    return InterlockedIncrement64((volatile LONGLONG*)&t);
}
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(LONGLONG), T>::type
atomicAdd(volatile T& t, T v)
{
    return InterlockedExchangeAdd64((volatile LONGLONG*)&t, (LONGLONG)v) + v;
}
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(LONGLONG), T>::type
atomicCompareAndSwap(volatile T& t, T newvalue, T comparand)
{
    return InterlockedCompareExchange64((volatile LONGLONG*)&t, (LONGLONG)newvalue, (LONGLONG)comparand);
}
#endif
#elif defined(OSX)

#include <libkern/OSAtomic.h>

template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(int32_t), T>::type
atomicDecrement(volatile T &t)
{
    return OSAtomicDecrement32Barrier((volatile int32_t *)&t);
}
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(int32_t), T>::type
atomicIncrement(volatile T &t)
{
    return OSAtomicIncrement32Barrier((volatile int32_t *)&t);
}
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(int32_t), T>::type
atomicAdd(volatile T &t, T v)
{
    return OSAtomicAdd32Barrier((int32_t)v, (volatile int32_t *)&t);
}
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(int32_t), T>::type
atomicCompareAndSwap(volatile T &t, T newvalue, T comparand)
{
    return OSAtomicCompareAndSwap32Barrier((int32_t)comparand, (int32_t)newvalue, (volatile int32_t *)&t) ? comparand : t;
}
#ifdef X86_64
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(int64_t), T>::type
atomicDecrement(volatile T &t)
{
    return OSAtomicDecrement64Barrier((volatile int64_t *)&t);
}
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(int64_t), T>::type
atomicIncrement(volatile T &t)
{
    return OSAtomicIncrement64Barrier((volatile int64_t *)&t);
}
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(int64_t), T>::type
atomicAdd(volatile T &t, T v)
{
    return OSAtomicAdd64Barrier((int64_t)v, (volatile int64_t *)&t);
}
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(int64_t), T>::type
atomicCompareAndSwap(volatile T &t, T newvalue, T comparand)
{
    return OSAtomicCompareAndSwap64Barrier((int64_t)comparand, (int64_t)newvalue, (volatile int64_t *)&t) ? comparand : t;
}
#endif
#elif (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
template <class T> 
typename boost::enable_if_c<sizeof(T) <= sizeof(void *), T>::type
atomicDecrement(volatile T& t) { return __sync_sub_and_fetch(&t, 1); }
template <class T>
typename boost::enable_if_c<sizeof(T) <= sizeof(void *), T>::type
atomicIncrement(volatile T& t) { return __sync_add_and_fetch(&t, 1); }
template <class T>
typename boost::enable_if_c<sizeof(T) <= sizeof(void *), T>::type
atomicAdd(volatile T& t, T v) { return __sync_add_and_fetch(&t, v); }
template <class T>
typename boost::enable_if_c<sizeof(T) <= sizeof(void *), T>::type
atomicCompareAndSwap(volatile T &t, T newvalue, T comparand)
{ return __sync_val_compare_and_swap(&t, comparand, newvalue); }
#elif (__GNUC__ == 4 && __GNUC_MINOR__ == 0) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
template <class T>
typename boost::enable_if_c<sizeof(T) <= sizeof(_Atomic_word), T>::type
atomicDecrement(volatile T& t) { return __gnu_cxx::__exchange_and_add((_Atomic_word*)_&t, -1) - 1; }
template <class T>
typename boost::enable_if_c<sizeof(T) <= sizeof(_Atomic_word), T>::type
atomicIncrement(volatile T& t) { return __gnu_cxx::__exchange_and_add((_Atomic_word*)_&t, 1) + 1; }
template <class T>
typename boost::enable_if_c<sizeof(T) <= sizeof(_Atomic_word), T>::type
atomicAdd(volatile T& t, T v) { return __gnu_cxx::__exchange_and_add((_Atomic_word*)_&t, v) + v; }
#endif

}

#endif
