#ifndef __MORDOR_ATOMIC_H__
#define __MORDOR_ATOMIC_H__

#include <boost/utility/enable_if.hpp>

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
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(LONG), T>::type
atomicSwap(volatile T& t, T newvalue)
{
    return InterlockedExchange((volatile LONG*)&t, (LONG)newvalue);
}
inline
bool
atomicTestAndSet(volatile void *address, int bit = 0)
{
    return !!InterlockedBitTestAndSet((volatile LONG*)address + (bit >> 5), (LONG)(0x80000000 >> (bit & 31)));
}
inline
bool
atomicTestAndClear(volatile void *address, int bit = 0)
{
    return !!InterlockedBitTestAndReset((volatile LONG*)address + (bit >> 5), (LONG)(0x80000000 >> (bit & 31)));
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
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(LONGLONG), T>::type
atomicSwap(volatile T& t, T newvalue)
{
    return InterlockedExchange64((volatile LONGLONG*)&t, (LONGLONG)newvalue);
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
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(int32_t), T>::type
atomicSwap(volatile T &t, T newvalue)
{
    int32_t comparand = (int32_t)t;
    while (!OSAtomicCompareAndSwap32Barrier((int32_t)comparand, (int32_t)newvalue, (volatile int32_t *) &t))
        comparand = (int32_t)t;
    return comparand;
}
inline
bool
atomicTestAndSet(volatile void *addr, int bit = 0)
{
    return OSAtomicTestAndSetBarrier((uint32_t)bit, addr);
}
template <class T>
bool
atomicTestAndClear(volatile void *addr, int bit = 0)
{
    return OSAtomicTestAndClearBarrier((uint32_t)bit, addr);
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
template <class T>
typename boost::enable_if_c<sizeof(T) == sizeof(int64_t), T>::type
atomicSwap(volatile T &t, T newvalue)
{
    int64_t comparand = (int64_t)t;
    while (!OSAtomicCompareAndSwap64Barrier((int64_t)comparand, (int64_t)newvalue,
        comparand = (int64_t)t));
    return comparand;
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
template <class T>
typename boost::enable_if_c<sizeof(T) <= sizeof(void *), T>::type
atomicSwap(volatile T &t, T newvalue)
{ return __sync_lock_test_and_set(&t, newvalue); }
inline
bool
atomicTestAndSet(volatile void *address, int bit = 0)
{
    int mask = (1 << (sizeof(int) * 8 - 1)) >> (bit & (sizeof(int) * 8 - 1));
    volatile int &target = *(volatile int *)((intptr_t)address >> (sizeof(int) >> 3));
    int oldvalue, newvalue;
    do {
        oldvalue = target;
        newvalue = oldvalue | mask;
    } while (newvalue != atomicCompareAndSwap(target, newvalue, oldvalue));
    return !!(oldvalue & mask);
}
inline
bool
atomicTestAndClear(volatile void *address, int bit = 0)
{
    int mask = (1 << (sizeof(int) * 8 - 1)) >> (bit & (sizeof(int) * 8 - 1));
    volatile int &target = *(volatile int *)((intptr_t)address >> (sizeof(int) >> 3));
    int oldvalue, newvalue;
    do {
        oldvalue = target;
        newvalue = oldvalue & ~mask;
    } while (newvalue != atomicCompareAndSwap(target, newvalue, oldvalue));
    return !!(oldvalue & mask);
}
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

template <typename T>
class Atomic
{
public:
    Atomic(T val = 0) : m_val(val) { }

    operator T(void) const { return atomicAdd(m_val, T(0)); }
    T operator +=(T v) { return atomicAdd(m_val, v); }
    T operator -=(T v) { return atomicAdd(m_val, -v); }
    T operator ++(void) { return atomicIncrement(m_val); }
    T operator --(void) { return atomicDecrement(m_val); }

    // the postfix operators couild be a little more efficient if we
    // created atomicPost(Increment,Decrement) functions, but meh
    T operator ++(int) { return atomicIncrement(m_val) - 1; }
    T operator --(int) { return atomicDecrement(m_val) + 1; }

private:
    mutable T m_val;
};

}

#endif
