#ifndef __MORDOR_THREAD_LOCAL_STORAGE_H__
#define __MORDOR_THREAD_LOCAL_STORAGE_H__
// Copyright (c) 2009 - Decho Corp.

#include "predef.h"

#ifdef WINDOWS
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace Mordor {

template <class T>
class ThreadLocalStorage
{
public:
    ThreadLocalStorage()
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

    ~ThreadLocalStorage()
    {
#ifdef WINDOWS
        TlsFree(m_key);
#else
        pthread_key_delete(m_key);
#endif
    }

    void reset(T *t)
    {
#ifdef WINDOWS
        if (!TlsSetValue(m_key, t))
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("TlsSetValue");
#else
        int rc = pthread_setspecific(m_key, t);
        if (rc)
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(rc, "pthread_setspecific");
#endif
    }

    T *get()
    {
#ifdef WINDOWS
        return (T *)TlsGetValue(m_key);
#else
        return (T *)pthread_getspecific(m_key);
#endif
    }

    T *operator->() { return get(); }

private:
#ifdef WINDOWS
    DWORD m_key;
#else
    pthread_key_t m_key;
#endif
};

};

#endif
