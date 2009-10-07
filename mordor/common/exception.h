#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__
// Copyright (c) 2009 - Decho Corp.

#include <stdexcept>
#include <string>
#include <vector>

#include <boost/exception.hpp>

#include "version.h"

typedef boost::error_info<struct tag_backtrace, std::vector<void *> > errinfo_backtrace;
#ifdef WINDOWS
typedef boost::error_info<struct tag_lasterror, DWORD> errinfo_lasterror;
std::string to_string( errinfo_lasterror const & e );
typedef errinfo_lasterror errinfo_nativeerror;
typedef errinfo_lasterror errinfo_gaierror;
#else
typedef boost::error_info<struct tag_gaierror, int> errinfo_gaierror;
std::string to_string( errinfo_gaierror const & e );
typedef boost::errinfo_errno errinfo_nativeerror;
#endif

std::string to_string( errinfo_backtrace const &bt );

std::vector<void *> backtrace(int framesToSkip = 0);
void removeTopFrames(boost::exception &ex, int framesToSkip = 0);

#define MORDOR_THROW_EXCEPTION(x)                                               \
    throw boost::enable_current_exception(boost::enable_error_info(x))          \
        << boost::throw_function(BOOST_CURRENT_FUNCTION)                        \
        << boost::throw_file(__FILE__)                                          \
        << boost::throw_line((int)__LINE__)                                     \
        << errinfo_backtrace(backtrace())

void rethrow_exception(boost::exception_ptr const & ep);

struct ExceptionBase : virtual boost::exception, virtual std::exception {};

struct StreamException : virtual ExceptionBase {};
struct UnexpectedEofException : virtual StreamException {};
struct WriteBeyondEofException : virtual StreamException {};
struct BufferOverflowException : virtual StreamException {};

struct NativeException : virtual ExceptionBase {};

#ifdef WINDOWS
#include <winerror.h>
typedef DWORD error_t;
#else
#include <errno.h>
#include <netdb.h>

#if defined(FREEBSD) && !defined(EAI_NODATA)
#define EAI_ADDRFAMILY 1
#define EAI_NODATA 7
#endif

typedef int error_t;
#endif

struct FileNotFoundException : virtual NativeException {};
struct BadHandleException : virtual NativeException {};
struct OperationAbortedException : virtual NativeException {};
struct BrokenPipeException : virtual NativeException {};

struct SocketException : virtual NativeException {};

struct ConnectionAbortedException : virtual SocketException {};
struct ConnectionResetException : virtual SocketException {};
struct ConnectionRefusedException : virtual SocketException {};
struct HostDownException : virtual SocketException {};
struct HostUnreachableException : virtual SocketException {};
struct NetworkDownException : virtual SocketException {};
struct NetworkResetException : virtual SocketException {};
struct NetworkUnreachableException : virtual SocketException {};
struct TimedOutException : virtual SocketException {};
struct NameLookupException : virtual SocketException {};
struct TemporaryNameServerFailureException : virtual NameLookupException {};
struct PermanentNameServerFailureException : virtual NameLookupException {};
struct NoNameServerDataException : virtual NameLookupException {};
struct HostNotFoundException : virtual NameLookupException {};

error_t lastError();

void throwExceptionFromLastError(error_t lastError);

#define THROW_EXCEPTION_FROM_LAST_ERROR()                                       \
    try {                                                                       \
        throwExceptionFromLastError(lastError());                               \
    } catch (boost::exception &ex) {                                            \
        ex << boost::throw_function(BOOST_CURRENT_FUNCTION)                     \
            << boost::throw_file(__FILE__)                                      \
            << boost::throw_line((int)__LINE__)                                 \
            << errinfo_backtrace(backtrace());                                  \
        throw;                                                                  \
    }

#define THROW_EXCEPTION_FROM_LAST_ERROR_API(api)                                \
    try {                                                                       \
        throwExceptionFromLastError(lastError());                               \
    } catch (boost::exception &ex) {                                            \
        ex << boost::throw_function(BOOST_CURRENT_FUNCTION)                     \
            << boost::throw_file(__FILE__)                                      \
            << boost::throw_line((int)__LINE__)                                 \
            << boost::errinfo_api_function(api)                                 \
            << errinfo_backtrace(backtrace());                                  \
        throw;                                                                  \
    }

#define THROW_EXCEPTION_FROM_ERROR(error)                                       \
    try {                                                                       \
        throwExceptionFromLastError(error);                                     \
    } catch (boost::exception &ex) {                                            \
        ex << boost::throw_function(BOOST_CURRENT_FUNCTION)                     \
            << boost::throw_file(__FILE__)                                      \
            << boost::throw_line((int)__LINE__)                                 \
            << errinfo_backtrace(backtrace());                                  \
        throw;                                                                  \
    }

#define THROW_EXCEPTION_FROM_ERROR_API(error, api)                              \
    try {                                                                       \
        throwExceptionFromLastError(error);                                     \
    } catch (boost::exception &ex) {                                            \
        ex << boost::throw_function(BOOST_CURRENT_FUNCTION)                     \
            << boost::throw_file(__FILE__)                                      \
            << boost::throw_line((int)__LINE__)                                 \
            << boost::errinfo_api_function(api)                                 \
            << errinfo_backtrace(backtrace());                                  \
        throw;                                                                  \
    }

#endif
