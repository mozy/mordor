#ifndef __MORDOR_ASSERT_H__
#define __MORDOR_ASSERT_H__
// Copyright (c) 2009 - Decho Corp.

#include "exception.h"
#include "version.h"

namespace Mordor {

struct Assertion : virtual Exception
{
    Assertion(const std::string &expr) : m_expr(expr) {}
    ~Assertion() throw() {}

    const char *what() const throw() { return m_expr.c_str(); }
private:
    std::string m_expr;
};

#define MORDOR_VERIFY(x)                                                               \
    if (!(x)) MORDOR_THROW_EXCEPTION(::Mordor::Assertion(# x));

#define MORDOR_NOTREACHED() MORDOR_THROW_EXCEPTION(::Mordor::Assertion("Not Reached"))

#ifdef DEBUG
#define MORDOR_ASSERT MORDOR_VERIFY
#else
#define MORDOR_ASSERT(x) {}
#endif

}

#endif
