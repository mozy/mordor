#include "comlike.h"

#include "version.h"

#ifndef WINDOWS
#include <string.h>

bool IsEqualGUID(const GUID& lhs, const GUID& rhs)
{
    return memcmp(&lhs, &rhs, sizeof(GUID)) == 0;
}

// {29946E26-C9EC-4196-9083-4C292AABFD54}
const GUID IID_IUnknown = 
{ 0x29946e26, 0xc9ec, 0x4196, { 0x90, 0x83, 0x4c, 0x29, 0x2a, 0xab, 0xfd, 0x54 } };

result_t HRESULT_FROM_WIN32(int err)
{
    return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, err & 0xffff);
}

#endif

CUnknown::CUnknown()
{
    m_refCount = 1;
}

CUnknown::~CUnknown()
{
}

unsigned long
CUnknown::AddRef()
{
    return ++m_refCount;
}

unsigned long
CUnknown::Release()
{
    unsigned int result = --m_refCount;
    if (result == 0)
        delete this;
    return result;
}
