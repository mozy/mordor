#ifndef __COMLIKE_H__
#define __COMLIKE_H__
// Copyright (c) 2009 Decho Corp.

#include "version.h"

#ifdef WINDOWS
typedef HRESULT result_t
#else
typedef unsigned int result_t;

class IUnknown
{
public:
    virtual unsigned int AddRef() = 0;
    virtual unsigned int Release() = 0;
    virtual result_t QueryInterface(REFIID riid, void **ppObj) = 0;
};
#endif

class CUnknown : public IUnknown
{
public:
    CUnknown();

    unsigned int AddRef();
    unsigned int Release();

private:
    unsigned int m_refCount;
};

enum VariantType
{
    Empty,
    String,
    Int64,
    UInt64
}

struct Variant
{
    VariantType type;
    union {
        char *string;
        long long int64;
        unsigned long long uint64;
    };
};

template <class T>
class CCOMPtr<T>
{
public:
    CCOMPtr(T* t)
    {
        m_t = t;
    }

    CCOMPtr(const CCOMPtr<T> &copy)
    {
        m_t = copy.m_t;
        if (m_t) {
            m_t->AddRef();
        }
    }

    ~CCOMPtr()
    {
        clear();
    }

    void clear()
    {
        if (m_t) {
            m_t->Release();
            m_t = NULL;
        }
    }

    T* operator*()
    {
        return m_t;
    }

    T* operator->()
    {
        return m_t;
    }

    T** operator&()
    {
        clear();
        return &m_t;
    }

    CCOMPtr<T> operator=(T* t)
    {
        clear();
        m_t = t;
    }

    bool operator==(T* rhs)
    {
        return m_t == rhs;
    }

    bool operator!=(T* rhs)
    {
        return m_t != rhs;
    }

private:
    T* m_t;
}

#endif
