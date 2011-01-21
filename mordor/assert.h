#ifndef __MORDOR_ASSERT_H__
#define __MORDOR_ASSERT_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "exception.h"
#include "log.h"
#include "version.h"

namespace Mordor {

bool isDebuggerAttached();
void debugBreak();

struct Assertion : virtual Exception
{
    Assertion(const std::string &expr) : m_expr(expr) {}
    ~Assertion() throw() {}

    const char *what() const throw() { return m_expr.c_str(); }

    static bool throwOnAssertion;
private:
    std::string m_expr;
};

}

#endif
// No include guard - you can include multiple times
#ifdef MORDOR_ASSERT
#undef MORDOR_ASSERT
#endif
#ifdef MORDOR_VERIFY
#undef MORDOR_VERIFY
#endif
#ifdef MORDOR_NOTREACHED
#undef MORDOR_NOTREACHED
#endif

#ifdef NDEBUG

#define MORDOR_ASSERT(x) ((void)0)
#define MORDOR_VERIFY(x) ((void)(x))
#define MORDOR_NOTREACHED() ::std::terminate();

#else

#define MORDOR_ASSERT(x)                                                        \
    if (!(x)) {                                                                 \
        MORDOR_LOG_FATAL(::Mordor::Log::root()) << "ASSERTION: " # x            \
            << "\nbacktrace:\n" << ::Mordor::to_string(::Mordor::backtrace());  \
        if (::Mordor::Assertion::throwOnAssertion)                              \
            MORDOR_THROW_EXCEPTION(::Mordor::Assertion(# x));                   \
        if (::Mordor::isDebuggerAttached())                                     \
            ::Mordor::debugBreak();                                             \
        ::std::terminate();                                                     \
    }

#define MORDOR_VERIFY(x) MORDOR_ASSERT(x)

#define MORDOR_NOTREACHED()                                                     \
{                                                                               \
    MORDOR_LOG_FATAL(::Mordor::Log::root()) << "NOT REACHED"                    \
        << "\nbacktrace:\n" << ::Mordor::to_string(::Mordor::backtrace());      \
    if (::Mordor::Assertion::throwOnAssertion)                                  \
        MORDOR_THROW_EXCEPTION(::Mordor::Assertion("Not Reached"));             \
    if (::Mordor::isDebuggerAttached())                                         \
        ::Mordor::debugBreak();                                                 \
    ::std::terminate();                                                         \
}

#endif
