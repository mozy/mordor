#ifndef __CXA_EXCEPTION_H__
#define __CXA_EXCEPTION_H__

/// @file cxa_exception.h
/// this file declares some interfaces for "Exception Handling APIs"
/// see http://www.codesourcery.com/public/cxx-abi/abi-eh.html

#if defined(__GNUC__) || defined(__clang__)
#   define CXXABIV1_EXCEPTION
// MSVC compiler has /GT option for supporting fiber-safe thread-local storage
#endif

#ifdef CXXABIV1_EXCEPTION

#include <stdint.h>
#include <stddef.h>

namespace __cxxabiv1 {
    // __cxa_exception is defined in unwind.h which is in turn a part of GCC.
    // here we do not care about the internals of it. so make it a `void'.
    typedef void __cxa_exception;
    // Each thread in a C++ program has access to a __cxa_eh_globals object.
    struct __cxa_eh_globals {
        __cxa_exception *   caughtExceptions;
        unsigned int        uncaughtExceptions;
#ifdef __ARM_EABI_UNWINDER__
        __cxa_exception* propagatingExceptions;
#endif
    };
}
#endif

namespace Mordor {

class ExceptionStack
{
 public:
    ExceptionStack();
    /// save current exception stack to this fiber, and replace current
    /// exception stack with the one in toFiber.
    void swap(ExceptionStack &);
private:
#ifdef CXXABIV1_EXCEPTION
    __cxxabiv1::__cxa_eh_globals m_eh;
#endif
};

}

#endif
