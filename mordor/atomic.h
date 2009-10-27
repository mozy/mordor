#ifndef __MORDOR_ATOMIC_H__
#define __MORDOR_ATOMIC_H__

#include "version.h"

namespace Mordor {

#ifdef WINDOWS
template <class T>
typename boost::enable_if_c<sizeof(T) == 4, T>::type
atomicDecrement(volatile T& t)
{
    return InterlockedDecrement((LONG*)&t);
}
template <class T>
typename boost::enable_if_c<sizeof(T) == 4, T>::type
atomicIncrement(volatile T& t) 
{
    return InterlockedIncrement((LONG*)&t);
}
template <class T>
typename boost::enable_if_c<sizeof(T) == 4, T>::type
atomicAdd(volatile T& t, T v)
{
    return InterlockedExchangeAdd((LONG*)&t, (LONG)v) + v;
}
template <class T>
typename boost::enable_if_c<sizeof(T) == 4, T>::type
atomicCompareAndSwap(volatile T& t, T newvalue, T comparand)
{
    return InterlockedCompareExchange((LONG*)&t, (LONG)newvalue, (LONG)comparand);
}
#ifdef X86_64
template <class T>
typename boost::enable_if_c<sizeof(T) == 8, T>::type
atomicDecrement(volatile T& t)
{
    return InterlockedDecrement64((LONGLONG*)&t);
}
template <class T>
typename boost::enable_if_c<sizeof(T) == 8, T>::type
atomicIncrement(volatile T& t) 
{
    return InterlockedIncrement64((LONGLONG*)&t);
}
template <class T>
typename boost::enable_if_c<sizeof(T) == 8, T>::type
atomicAdd(volatile T& t, T v)
{
    return InterlockedExchangeAdd64((LONGLONG*)&t, (LONGLONG)v) + v;
}
template <class T>
typename boost::enable_if_c<sizeof(T) == 8, T>::type
atomicCompareAndSwap(volatile T& t, T newvalue, T comparand)
{
    return InterlockedCompareExchange64((LONGLONG*)&t, (LONGLONG)newvalue, (LONGLONG)comparand);
}
#endif
#elif (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
template <class T> T atomicDecrement(volatile T& t) { return __sync_sub_and_fetch(&t, 1); }
template <class T> T atomicIncrement(volatile T& t) { return __sync_add_and_fetch(&t, 1); }
template <class T> T atomicAdd(volatile T& t, T v) { return __sync_add_and_fetch(&t, v); }
template <class T> T atomicCompareAndSwap(volatile T &t, T newvalue, T comparand)
{ return __sync_val_compare_and_swap(&t, newvalue, comparand); }
#endif

}

#endif
