#ifndef __MORDOR_THREAD_LOCAL_STORAGE_H__
#define __MORDOR_THREAD_LOCAL_STORAGE_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "predef.h"

#include <boost/noncopyable.hpp>

#ifdef WINDOWS
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace Mordor {

template <class T>
class ThreadLocalStorageBase : boost::noncopyable
{
public:
    ThreadLocalStorageBase()
    {
#ifdef WINDOWS
        m_key = TlsAlloc();
        if (m_key == TLS_OUT_OF_INDEXES)
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("TlsAlloc");
#else
        int rc = pthread_key_create(&m_key, NULL);
        if (rc)
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(rc, "pthread_create_specific");
#endif
    }

    ~ThreadLocalStorageBase()
    {
#ifdef WINDOWS
        TlsFree(m_key);
#else
        pthread_key_delete(m_key);
#endif
    }

    typename boost::enable_if_c<sizeof(T) <= sizeof(void *)>::type set(const T &t)
    {
#ifdef WINDOWS
        if (!TlsSetValue(m_key, (LPVOID)t))
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("TlsSetValue");
#else
        int rc = pthread_setspecific(m_key, (const void *)t);
        if (rc)
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(rc, "pthread_setspecific");
#endif
    }

    T get() const
    {
#ifdef WINDOWS
#pragma warning(push)
#pragma warning(disable: 4800)
        DWORD lastError = GetLastError();
        LPVOID result = TlsGetValue(m_key);
        SetLastError(lastError);
        return (T)result;
#pragma warning(pop)
#else
        return (T)pthread_getspecific(m_key);
#endif
    }

    operator T() const { return get(); }

private:
#ifdef WINDOWS
    DWORD m_key;
#else
    pthread_key_t m_key;
#endif
};

template <class T>
class ThreadLocalStorage : public ThreadLocalStorageBase<T>
{
public:
    T operator =(T t) { set(t); return t; }
};

template <class T>
class ThreadLocalStorage<T *> : public ThreadLocalStorageBase<T *>
{
public:
    T * operator =(T *const t) { set(t); return t; }
    T & operator*() { return *ThreadLocalStorageBase<T *>::get(); }
    T * operator->() { return ThreadLocalStorageBase<T *>::get(); }
};

};

#endif
