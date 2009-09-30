// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "exception.h"

static void throwSocketException(error_t lastError, const char *function)
{
    switch (lastError) {
        case WSA(ECONNABORTED):
            throw ConnectionAbortedException(function);
        case WSA(ECONNRESET):
            throw ConnectionResetException(function);
        case WSA(ECONNREFUSED):
            throw ConnectionRefusedException(function);
        case WSA(EHOSTDOWN):
            throw HostDownException(function);
        case WSA(EHOSTUNREACH):
            throw HostUnreachableException(function);
        case WSA(ENETDOWN):
            throw NetworkDownException(function);
        case WSA(ENETRESET):
            throw NetworkResetException(function);
        case WSA(ENETUNREACH):
            throw NetworkUnreachableException(function);
        case WSA(ETIMEDOUT):
            throw TimedOutException(function);
        default:
            break;
    }
}

#ifdef WINDOWS
#include <windows.h>

Win32Error::Win32Error(unsigned int lastError, const char *function)
: std::runtime_error(constructMessage(lastError)),
  m_lastError(lastError),
  m_function(function)
{}

std::string
Win32Error::constructMessage(unsigned int lastError)
{
    std::string result;
    char *desc;
    DWORD numChars = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        lastError, 0,
        (char*)&desc, 0, NULL);
    if (numChars > 0) {
        result = desc;
        LocalFree((HANDLE)desc);
    }
    return result;
}

error_t lastError()
{
    return GetLastError();
}

void throwExceptionFromLastError(unsigned int lastError, const char *function)
{
    switch (lastError) {
        case ERROR_INVALID_HANDLE:
        case WSAENOTSOCK:
            throw BadHandleException(function);
        case ERROR_FILE_NOT_FOUND:
            throw FileNotFoundException(function);
        case ERROR_OPERATION_ABORTED:
            throw OperationAbortedException(function);
        case WSAESHUTDOWN:
            throw BrokenPipeException(function);
        default:
            throwSocketException(lastError, function);
            throw Win32Error(lastError, function);
    }
}
#else
#include <errno.h>
#include <string.h>

ErrnoError::ErrnoError(int error, const char *function)
: std::runtime_error(constructMessage(error)),
  m_error(error),
  m_function(function)
{}

std::string
ErrnoError::constructMessage(int error)
{
    const char *message = strerror(error);
    if (message)
        return message;
    return "";
}

error_t lastError()
{
    return errno;
}

void throwExceptionFromLastError(int error, const char *function)
{
    switch (error) {
        case EBADF:
            throw BadHandleException(function);
        case ENOENT:
            throw FileNotFoundException(function);
        case ECANCELED:
            throw OperationAbortedException(function);
        case EPIPE:
            throw BrokenPipeException(function);
        default:
            throwSocketException(error, function);
            throw ErrnoError(error);
    }
}
#endif

void throwExceptionFromLastError(const char *function)
{
    throwExceptionFromLastError(lastError(), function);
}
