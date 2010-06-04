#ifndef __MORDOR_UTIL_H__
#define __MORDOR_UTIL_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "version.h"

#ifdef OSX
#include <CoreFoundation/CFBase.h>
#endif

namespace Mordor {

template <class T>
void nop(const T &) {}

#ifdef OSX
template <class T>
struct ScopedCFRef
{
public:
    ScopedCFRef() {}
    ScopedCFRef(T ref) : m_ref(ref) {}
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

private:
    T m_ref;
};
#endif

}

#endif
