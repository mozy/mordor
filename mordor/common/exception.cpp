// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "exception.h"

static void throwSocketException(error_t lastError)
{
    switch (lastError) {
        case WSA(ECONNABORTED):
            throw ConnectionAbortedException();
        case WSA(ECONNRESET):
            throw ConnectionResetException();
        case WSA(ETIMEDOUT):
            throw TimedOutException();
        default:
            break;
    }
}

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

error_t lastError()
{
    return GetLastError();
}

void throwExceptionFromLastError(unsigned int lastError)
{
    switch (lastError) {
        case ERROR_INVALID_HANDLE:
        case WSAENOTSOCK:
            throw BadHandleException();
        case ERROR_FILE_NOT_FOUND:
            throw FileNotFoundException();
        case ERROR_OPERATION_ABORTED:
            throw OperationAbortedException();
        case WSAESHUTDOWN:
            throw BrokenPipeException();
        default:
            throwSocketException(lastError);
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

error_t lastError()
{
    return errno;
}

void throwExceptionFromLastError(int error)
{
    switch (error) {
        case EBADF:
            throw BadHandleException();
        case ENOENT:
            throw FileNotFoundException();
        case ECANCELED:
            throw OperationAbortedException();
        case EPIPE:
            throw BrokenPipeException();
        default:
            throwSocketException(error);
            throw ErrnoError(error);
    }
}
#endif

void throwExceptionFromLastError()
{
    throwExceptionFromLastError(lastError());
}
