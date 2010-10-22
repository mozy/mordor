#ifndef __MORDOR_ASSERT_H__
#define __MORDOR_ASSERT_H__
// Copyright (c) 2009 - Decho Corporation

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

#define MORDOR_VERIFY(x)                                                        \
    if (!(x)) {                                                                 \
        MORDOR_LOG_FATAL(::Mordor::Log::root()) << "ASSERTION: " # x            \
            << "\nbacktrace:\n" << ::Mordor::to_string(::Mordor::backtrace());  \
        if (::Mordor::Assertion::throwOnAssertion)                              \
            MORDOR_THROW_EXCEPTION(::Mordor::Assertion(# x));                   \
        if (::Mordor::isDebuggerAttached())                                     \
            ::Mordor::debugBreak();                                             \
        ::std::terminate();                                                     \
    }

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

#ifdef DEBUG
#define MORDOR_ASSERT MORDOR_VERIFY
#else
#define MORDOR_ASSERT(x) {}
#endif

}

#endif
