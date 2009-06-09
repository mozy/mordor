// Copyright (c) 2009 - Decho Corp.

#include "exception.h"

#ifdef WINDOWS
#include <windows.h>

Win32Error::Win32Error(unsigned int lastError)
: std::runtime_error(""),
  m_lastError(lastError)
{
    char *desc;
    DWORD numChars = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        lastError, 0,
        (char*)&desc, 0, NULL);
    if (numChars > 0) {
        m_message = desc;
        LocalFree((HANDLE)desc);
    }
}

void throwExceptionFromLastError()
{
    throwExceptionFromLastError(GetLastError());
}

void throwExceptionFromLastError(unsigned int lastError)
{
    switch (lastError) {
        case ERROR_OPERATION_ABORTED:
            throw OperationAbortedException();
        default:
            throw Win32Error(lastError);
    }
}
#else
#include <errno.h>
#include <string.h>

ErrnoError::ErrnoError(int error)
: std::runtime_error(""),
  m_error(error)
{
    char *desc = strerror(error);
    if (desc) {
        m_message = desc;
    }
}

void throwExceptionFromLastError()
{
    throwExceptionFromLastError(errno);
}

void throwExceptionFromLastError(int error)
{
    switch (error) {
        case ECANCELED:
            throw OperationAbortedException();
        default:
            throw ErrnoError(error);
    }
}
#endif
