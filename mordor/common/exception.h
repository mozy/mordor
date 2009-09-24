#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__
// Copyright (c) 2009 - Decho Corp.

#include <stdexcept>
#include <string>

#include "version.h"

class NestedException : public std::exception
{
public:
    NestedException(std::exception &inner)
        : m_inner(&inner)
    {}

    std::exception &inner() { return *m_inner; }
    const char *what() const throw () { return m_inner->what(); }

private:
    std::exception *m_inner;
};

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

class BufferOverflowError : public StreamError
{
public:
    BufferOverflowError() {}
};

#ifdef WINDOWS
#include <winerror.h>
class Win32Error : public std::runtime_error
{
public:
    Win32Error(unsigned int lastError);
    const char *what() const { return m_message.c_str(); }

    unsigned int error() const { return m_lastError; }

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

    int error() const { return m_error; }

private:
    int m_error;
    std::string m_message;
};
typedef ErrnoError NativeError;
typedef int error_t;
#endif

#ifdef WINDOWS
#define CREATE_NATIVE_EXCEPTION(Name, win32error, errnoerror)                   \
    class Name ## Exception : public NativeError                                \
    {                                                                           \
    public:                                                                     \
        Name ## Exception() : NativeError(win32error) {}                        \
    };
#else
#define CREATE_NATIVE_EXCEPTION(Name, win32error, errnoerror)                   \
    class Name ## Exception : public NativeError                                \
    {                                                                           \
    public:                                                                     \
        Name ## Exception() : NativeError(errnoerror) {}                        \
    };
#endif

CREATE_NATIVE_EXCEPTION(FileNotFound, ERROR_FILE_NOT_FOUND, ENOENT);
CREATE_NATIVE_EXCEPTION(BadHandle, ERROR_INVALID_HANDLE, EBADF);
CREATE_NATIVE_EXCEPTION(OperationAborted, ERROR_OPERATION_ABORTED, ECANCELED);
CREATE_NATIVE_EXCEPTION(BrokenPipe, WSAESHUTDOWN, EPIPE);

#undef CREATE_NATIVE_EXCEPTION

class SocketException : public NativeError
{
public:
    SocketException(error_t lastError) : NativeError(lastError) {}
};

#ifdef WINDOWS
#define WSA(error) WSA ## error
#else
#define WSA(error) error
#endif

#define CREATE_SOCKET_EXCEPTION(Name, errnoerror)                               \
    class Name ## Exception : public SocketException                            \
    {                                                                           \
    public:                                                                     \
        Name ## Exception() : SocketException(WSA(errnoerror)) {}               \
    };

CREATE_SOCKET_EXCEPTION(ConnectionAborted, ECONNABORTED);
CREATE_SOCKET_EXCEPTION(ConnectionReset, ECONNRESET);
CREATE_SOCKET_EXCEPTION(TimedOut, ETIMEDOUT);

#undef CREATE_SOCKET_EXCEPTION

error_t lastError();
void throwExceptionFromLastError();
void throwExceptionFromLastError(error_t lastError);

#endif
