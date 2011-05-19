#ifndef __MORDOR_UTIL_H__
#define __MORDOR_UTIL_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "version.h"

#include <boost/shared_ptr.hpp>

#ifdef OSX
#include <CoreFoundation/CFBase.h>
#endif

#ifndef WINDOWS
// VC2008 doesn't have this, so we'll do without
#include <stdint.h>
#endif

namespace Mordor {

template <class T>
void nop(const T &) {}

/// Create a boost::shared_ptr to an object without the shared_ptr managing it.
///
/// The object will not be deleted when the shared_ptr goes out of scope.  The
/// lifetime of the object is not managed by the shared_ptr at all, so whatever
/// you pass this to must not continue using it after the passed in object is
/// destroyed.  Typically you could use this to create a shared_ptr to a stack
/// object.
template <class T>
boost::shared_ptr<T> unmanagedPtr(T &t)
{ return boost::shared_ptr<T>(&t, &nop<T*>); }

#ifdef OSX
template <class T>
struct ScopedCFRef
{
public:
    ScopedCFRef() : m_ref(NULL) {}
    ScopedCFRef(T ref) : m_ref(ref) {}
    ScopedCFRef(const ScopedCFRef &copy)
        : m_ref(copy.m_ref)
    {
        if (m_ref)
            CFRetain(m_ref);
    }
    ~ScopedCFRef()
    {
        if (m_ref)
            CFRelease(m_ref);
    }

    operator T & () { return m_ref; }
    operator const T & () const { return m_ref; }
    T * operator &() { return &m_ref; }
    T * const operator &() const { return &m_ref; }

    ScopedCFRef &operator =(T ref)
    {
        if (m_ref)
            CFRelease(m_ref);
        m_ref = ref;
        return *this;
    }
    ScopedCFRef &operator =(const ScopedCFRef &copy)
    {
        if (m_ref)
            CFRelease(m_ref);
        m_ref = copy.m_ref;
        if (m_ref)
            CFRetain(m_ref);
        return *this;
    }

private:
    T m_ref;
};
#endif

#ifdef WINDOWS
// create defined types for specific-sized integers
typedef __int8 int8_t;
typedef __int16 int16_t;
typedef __int32 int32_t;
typedef __int64 int64_t;
typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
#else
// copy existing type definitions from stdint.h into Mordor namespace
using ::int8_t;
using ::int16_t;
using ::int32_t;
using ::int64_t;
using ::uint8_t;
using ::uint16_t;
using ::uint32_t;
using ::uint64_t;
#endif

// compute with 96 bit intermediate result: (a*b)/c 
uint64_t muldiv64(uint64_t a, uint32_t b, uint64_t c);

}

#endif
