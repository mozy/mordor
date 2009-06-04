#ifndef __ATOMIC_H__
#define __ATOMIC_H__

#include "version.h"

#ifdef WINDOWS
template <class T> T atomicDecrement(T& t) { return InterlockedDecrement((LONG*)&t); }
template <class T> T atomicIncrement(T& t) { return InterlockedIncrement((LONG*)&t); }
#elif (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
template <class T> T atomicDecrement(T& t) { return __sync_sub_and_fetch(&t, 1); }
template <class T> T atomicIncrement(T& t) { return __sync_add_and_fetch(&t, 1); }
#elif (__GNUC__ == 4 && __GNUC_MINOR__ == 0)
template <class T> T atomicDecrement(T& t) { return 
__gnu_cxx::__exchange_and_add((_Atomic_word*)&t, -1) - 1; }
template <class T> T atomicIncrement(T& t) { return 
__gnu_cxx::__exchange_and_add((_Atomic_word*)&t, 1) + 1; }
#endif

#endif
