#ifndef __ATOMIC_H__
#define __ATOMIC_H__

#include "version.h"

#ifdef WINDOWS
template <class T> T atomicDecrement(T& t) { return InterlockedDecrement((LONG*)&t); }
template <class T> T atomicIncrement(T& t) { return InterlockedIncrement((LONG*)&t); }
#endif

#endif
