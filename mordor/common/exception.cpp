// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "exception.h"

#ifdef WINDOWS
#include <dbghelp.h>

#include "runtime_linking.h"

#pragma comment(lib, "dbghelp")
#else
#include <errno.h>
#include <execinfo.h>
#include <string.h>
#endif

#include <boost/thread/mutex.hpp>

namespace Mordor {

#ifdef WINDOWS
static BOOL g_useSymbols;

namespace {

static struct Initializer {
    Initializer()
    {
        SymSetOptions(SYMOPT_DEFERRED_LOADS |
            SYMOPT_FAIL_CRITICAL_ERRORS |
            SYMOPT_LOAD_LINES |
            SYMOPT_NO_PROMPTS);
        g_useSymbols = SymInitialize(GetCurrentProcess(), NULL, TRUE);
    }

    ~Initializer()
    {
        if (g_useSymbols)
            SymCleanup(GetCurrentProcess());
    }
} g_init;

}
#endif

std::string to_string( errinfo_backtrace const &bt )
{
#ifdef WINDOWS
    static boost::mutex s_mutex;
    boost::mutex::scoped_lock lock(s_mutex);
#endif
    std::ostringstream os;
    const std::vector<void *> &backtrace = bt.value();
#ifdef POSIX
    boost::shared_ptr<char *> symbols(backtrace_symbols(&backtrace[0],
        backtrace.size()), &free);
#endif
    for (size_t i = 0; i < backtrace.size(); ++i) {
        if (i != 0)
            os << std::endl;
#ifdef WINDOWS
        os << backtrace[i];
        if (g_useSymbols) {
            char buf[sizeof(SYMBOL_INFO) + MAX_SYM_NAME - 1];
            SYMBOL_INFO *symbol = (SYMBOL_INFO*)buf;
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = MAX_SYM_NAME;
            DWORD64 displacement64 = 0;
            if (SymFromAddr(GetCurrentProcess(), (DWORD64)backtrace[i],
                &displacement64, symbol)) {
                os << ": " << symbol->Name << "+" << displacement64;
            }
            IMAGEHLP_LINE64 line;
            line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            DWORD displacement = 0;
            if (SymGetLineFromAddr64(GetCurrentProcess(),
                (DWORD64)backtrace[i], &displacement, &line)) {
                os << ": " << line.FileName << "(" << line.LineNumber << ")+"
                    << displacement;
            }
        }
#else
        if (symbols)
            os << symbols.get()[i];
        else
            os << backtrace[i];
#endif
    }
    return os.str();
}

#ifdef WINDOWS
std::string to_string( errinfo_lasterror const &e)
{
    std::string result;
    char *desc;
    DWORD numChars = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        e.value(), 0,
        (char*)&desc, 0, NULL);
    if (numChars > 0) {
        result = desc;
        LocalFree((HANDLE)desc);
    }
    std::ostringstream os;
    os << e.value() << ", \"" << result << "\"";
    return os.str();
}
#else
std::string to_string( errinfo_gaierror const &e)
{
    std::ostringstream os;
    os << e.value() << ", \"" << gai_strerror(e.value()) << "\"";
    return os.str();
}
#endif

std::vector<void *> backtrace(int framesToSkip)
{
    std::vector<void *> result;
#ifdef WINDOWS
    result.resize(64);
    WORD count = pRtlCaptureStackBackTrace(1 + framesToSkip, 62 - framesToSkip, &result[0], NULL);
    result.resize(count);
#else
    result.resize(64);
    int count = ::backtrace(&result[0], 64);
    result.resize(count);
    framesToSkip = std::min(count, framesToSkip + 1);
    result.erase(result.begin(), result.begin() + framesToSkip);
#endif
    return result;
}

void removeTopFrames(boost::exception &ex, int framesToSkip)
{
    const std::vector<void *> *oldbt = boost::get_error_info<errinfo_backtrace>(ex);
    if (oldbt && !oldbt->empty()) {
        std::vector<void *> newbt(*oldbt);
        std::vector<void *> bt = backtrace();
        size_t count = 0;
        ++framesToSkip;
        for (; count + framesToSkip < newbt.size() &&
            count + framesToSkip < bt.size(); ++count) {
            if (bt[bt.size() - count - 1] != newbt[newbt.size() - count - 1])
                break;
        }
        count -= framesToSkip;
        newbt.resize(std::min<int>(0, newbt.size() - count));
        newbt.erase(newbt.end() - count, newbt.end());
        ex << errinfo_backtrace(newbt);
    }
}

void rethrow_exception(boost::exception_ptr const & ep)
{
    // Take the backtrace from here, to avoid additional frames from the
    // exception handler
    std::vector<void *> bt = backtrace(1);
    try {
        boost::rethrow_exception(ep);
    } catch (boost::exception &e) {
        const std::vector<void *> *oldbt =
            boost::get_error_info<errinfo_backtrace>(e);
        if (oldbt)
            bt.insert(bt.begin(), oldbt->begin(), oldbt->end());
        e << errinfo_backtrace(bt);
        throw;
    }
}

#ifdef WINDOWS
#define WSA(error) WSA ## error
#else
#define WSA(error) error
#endif

#ifdef WINDOWS
#define NATIVE_ERROR(win32error, errnoerror) win32error
#else
#define NATIVE_ERROR(win32error, errnoerror) errnoerror
#endif

static void throwSocketException(error_t error)
{
    switch (error) {
        case WSA(ECONNABORTED):
            throw boost::enable_current_exception(ConnectionAbortedException())
                << errinfo_nativeerror(error);
        case WSA(ECONNRESET):
            throw boost::enable_current_exception(ConnectionResetException())
                << errinfo_nativeerror(error);
        case WSA(ECONNREFUSED):
            throw boost::enable_current_exception(ConnectionRefusedException())
                << errinfo_nativeerror(error);
        case WSA(EHOSTDOWN):
            throw boost::enable_current_exception(HostDownException())
                << errinfo_nativeerror(error);
        case WSA(EHOSTUNREACH):
            throw boost::enable_current_exception(HostUnreachableException())
                << errinfo_nativeerror(error);
        case WSA(ENETDOWN):
            throw boost::enable_current_exception(NetworkDownException())
                << errinfo_nativeerror(error);
        case WSA(ENETRESET):
            throw boost::enable_current_exception(NetworkResetException())
                << errinfo_nativeerror(error);
        case WSA(ENETUNREACH):
            throw boost::enable_current_exception(NetworkUnreachableException())
                << errinfo_nativeerror(error);
        case WSA(ETIMEDOUT):
            throw boost::enable_current_exception(TimedOutException())
                << errinfo_nativeerror(error);
        case EAI_AGAIN:
            throw boost::enable_current_exception(TemporaryNameServerFailureException())
                << errinfo_gaierror(error);
        case EAI_FAIL:
            throw boost::enable_current_exception(PermanentNameServerFailureException())
                << errinfo_gaierror(error);
#if defined(WSANO_DATA) || defined(WAI_NODATA)
        case NATIVE_ERROR(WSANO_DATA, EAI_NODATA):
            throw boost::enable_current_exception(NoNameServerDataException())
                << errinfo_gaierror(error);
#endif
        case EAI_NONAME:
            throw boost::enable_current_exception(HostNotFoundException())
                << errinfo_gaierror(error);
#ifdef EAI_ADDRFAMILY
        case EAI_ADDRFAMILY:
#endif
        case EAI_BADFLAGS:
        case EAI_FAMILY:
        case EAI_MEMORY:
        case EAI_SERVICE:
        case EAI_SOCKTYPE:
#ifdef EAI_SYSTEM
        case EAI_SYSTEM:
#endif
            throw boost::enable_current_exception(NameLookupException())
                << errinfo_gaierror(error);
        default:
            break;
    }
}

#ifdef WINDOWS
error_t lastError()
{
    return GetLastError();
}

void throwExceptionFromLastError(error_t error)
{
    switch (error) {
        case ERROR_INVALID_HANDLE:
        case WSAENOTSOCK:
            throw boost::enable_current_exception(BadHandleException())
                << errinfo_nativeerror(error);
        case ERROR_FILE_NOT_FOUND:
            throw boost::enable_current_exception(FileNotFoundException())
                << errinfo_nativeerror(error);
        case ERROR_OPERATION_ABORTED:
            throw boost::enable_current_exception(OperationAbortedException())
                << errinfo_nativeerror(error);
        case WSAESHUTDOWN:
            throw boost::enable_current_exception(BrokenPipeException())
                << errinfo_nativeerror(error);
        default:
            throwSocketException(error);
            throw boost::enable_current_exception(NativeException())
                << errinfo_nativeerror(error);
    }
}
#else

error_t lastError()
{
    return errno;
}

void throwExceptionFromLastError(error_t error)
{
    switch (error) {
        case EBADF:
            throw boost::enable_current_exception(BadHandleException())
                << errinfo_nativeerror(error);
        case ENOENT:
            throw boost::enable_current_exception(FileNotFoundException())
                << errinfo_nativeerror(error);
        case ECANCELED:
            throw boost::enable_current_exception(OperationAbortedException())
                << errinfo_nativeerror(error);
        case EPIPE:
            throw boost::enable_current_exception(BrokenPipeException())
                << errinfo_nativeerror(error);
        default:
            throwSocketException(error);
            throw boost::enable_current_exception(NativeException())
                << errinfo_nativeerror(error);
    }
}
#endif

}
