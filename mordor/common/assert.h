#ifndef __ASSERT_H__
#define __ASSERT_H__
// Copyright (c) 2009 - Decho Corp.

#include "exception.h"
#include "version.h"

struct Assertion : virtual ExceptionBase
{
    Assertion(const std::string &expr) : m_expr(expr) {}
    ~Assertion() throw() {}

    const char *what() const throw() { return m_expr.c_str(); }
private:
    std::string m_expr;
};

#define VERIFY(x)                                                               \
    if (!(x)) MORDOR_THROW_EXCEPTION(Assertion(# x));

#define NOTREACHED() MORDOR_THROW_EXCEPTION(Assertion("Not Reached"))

#ifdef DEBUG
#define ASSERT VERIFY
#else
#define ASSERT(x) {}
#endif

#endif
