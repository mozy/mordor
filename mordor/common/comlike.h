#ifndef __COMLIKE_H__
#define __COMLIKE_H__
// Copyright (c) 2009 Decho Corp.

#include "version.h"

typedef unsigned long long timestamp_t;

#ifdef WINDOWS
#include <guiddef.h>
#include <unknwn.h>
#include <winerror.h>
typedef HRESULT result_t;

#else
typedef unsigned int result_t;

#include <stddef.h>

#define STDMETHODCALLTYPE

#define S_OK 0
#define S_FALSE 1
#define E_UNEXPRECTED   0x8000FFFF
#define E_NOTIMPL       0x80000001
#define E_OUTOFMEMORY   0x80000002
#define E_INVALIDARG    0x80000003
#define E_NOINTERFACE   0x80000004

#define ERROR_FILE_NOT_FOUND 2

#define SEVERITY_SUCCESS    0
#define SEVERITY_ERROR      1

#define FACILITY_WIN32                   7

#define MAKE_HRESULT(sev,fac,code) \
    ((result_t) (((sev)<<31) | ((fac)<<16) | ((code))) )
result_t HRESULT_FROM_WIN32(int err);

struct GUID
{
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[ 8 ];
};

bool IsEqualGUID(const GUID& lhs, const GUID& rhs);

extern const GUID IID_IUnknown;
class IUnknown
{
public:
    virtual unsigned long AddRef() = 0;
    virtual unsigned long Release() = 0;
    virtual result_t QueryInterface(const GUID& iid, void **ppObj) = 0;
};
#endif

class CUnknown : virtual public IUnknown
{
public:
    CUnknown();

    unsigned long STDMETHODCALLTYPE AddRef();
    unsigned long STDMETHODCALLTYPE Release();

private:
    unsigned long m_refCount;
};

enum VariantType
{
    Empty,
    String,
    Int64,
    UInt64
};

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
class CCOMPtr
{
public:
    CCOMPtr()
    {
        m_t = NULL;
    }

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

    T** addressOf()
    {
        clear();
        return &m_t;
    }

    operator T*()
    {
        return m_t;
    }

    CCOMPtr<T>& operator=(T* t)
    {
        clear();
        m_t = t;
        return *this;
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
};

#endif
