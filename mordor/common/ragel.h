#ifndef __RAGEL_H__
#define __RAGEL_H__
// Copyright (c) 2009 - Decho Corp.

#include <stdexcept>
#include <string>

#include "mordor/common/streams/stream.h"

class RagelParser
{
public:
    virtual ~RagelParser() {}

    // Complete parsing
    size_t run(const void *buf, size_t len);
    size_t run(const char *str);
    size_t run(const std::string &str);
    size_t run(const Buffer &b);
    unsigned long long run(Stream &stream);
    unsigned long long run(Stream::ptr stream) { return run(*stream.get()); }

    // Partial parsing
    virtual void init();
    size_t run(const void *buf, size_t len, bool isEof);

    virtual bool complete() const { return final(); }
    virtual bool final() const = 0;
    virtual bool error() const = 0;

protected:
    virtual void exec() = 0;

protected:
    // Ragel state
    int cs;
    const char *p, *pe, *eof, *mark;
    std::string m_fullString;
};

class RagelParserWithStack : public RagelParser
{
protected:
    void prepush();
    void postpop();

protected:
    // Ragel state
    std::vector<int> stack;
    size_t top;
};

#endif
