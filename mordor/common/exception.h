#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__
// Copyright (c) 2009 - Decho Corp.

#include <stdexcept>
#include <string>

#include "version.h"

class StreamError : public std::runtime_error
{
public:
    StreamError() : std::runtime_error("") {}
};

class UnexpectedEofError : public StreamError
{
public:
    UnexpectedEofError() {}
};

#ifdef WINDOWS
#include <winerror.h>
class Win32Error : public std::runtime_error
{
public:
    Win32Error(unsigned int lastError);
    const char *what() const { return m_message.c_str(); }

    unsigned int lastError() { return m_lastError; }

private:
    int m_lastError;
    std::string m_message;
};
typedef Win32Error NativeError;
typedef unsigned int error_t;
#else
#include <errno.h>
class ErrnoError : public std::runtime_error
{
public:
    ErrnoError(int error);
    ~ErrnoError() throw() {}
    const char *what() const throw() { return m_message.c_str(); }

    int error() { return m_error; }

private:
    int m_error;
    std::string m_message;
};
typedef ErrnoError NativeError;
typedef int error_t;
#endif

class OperationAbortedException : public NativeError
{
public:
#ifdef WINDOWS
    OperationAbortedException() : Win32Error(ERROR_OPERATION_ABORTED) {}
#else
    OperationAbortedException() : ErrnoError(ECANCELED) {}
#endif
};

void throwExceptionFromLastError();
void throwExceptionFromLastError(error_t lastError);

#endif
