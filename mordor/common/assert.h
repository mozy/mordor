#ifndef __ASSERT_H__
#define __ASSERT_H__
// Copyright (c) 2009 - Decho Corp.

#include <stdexcept>

#include "version.h"

class Assertion : public std::logic_error
{
public:
    Assertion(const std::string &expr, const char *file, int line)
        : std::logic_error(expr),
          m_file(file),
          m_line(line)
    {}

    const char *file() const { return m_file; }
    int line() const { return m_line; }

private:
    const char *m_file;
    int m_line;
};

#define VERIFY(x)                                                               \
    if (!(x)) throw Assertion(# x, __FILE__, __LINE__);

#define NOTREACHED()                                                            \
    throw Assertion("Not Reached", __FILE__, __LINE__);

#ifdef DEBUG
#define ASSERT VERIFY
#else
#define ASSERT(x) {}
#endif

#endif
