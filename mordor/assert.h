#ifndef __MORDOR_ASSERT_H__
#define __MORDOR_ASSERT_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "exception.h"
#include "log.h"
#include "version.h"

namespace Mordor {

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
        MORDOR_LOG_FATAL(::Mordor::Log::root()) << "ASSERTION: " # x;           \
        if (::Mordor::Assertion::throwOnAssertion)                              \
            MORDOR_THROW_EXCEPTION(::Mordor::Assertion(# x));                   \
        ::std::terminate();                                                     \
    }

#define MORDOR_NOTREACHED()                                                     \
{                                                                               \
    MORDOR_LOG_FATAL(::Mordor::Log::root()) << "NOT REACHED";                   \
    if (::Mordor::Assertion::throwOnAssertion)                                  \
        MORDOR_THROW_EXCEPTION(::Mordor::Assertion("Not Reached"));             \
    ::std::terminate();                                                         \
}

#ifdef DEBUG
#define MORDOR_ASSERT MORDOR_VERIFY
#else
#define MORDOR_ASSERT(x) {}
#endif

}

#endif
