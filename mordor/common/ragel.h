#ifndef __RAGEL_H__
#define __RAGEL_H__
// Copyright (c) 2009 - Decho Corp.

#include <stdexcept>
#include <string>

#include "common/streams/stream.h"

class RagelParser
{
public:
    // Complete parsing
    void run(const std::string& str);
    void run(Stream &stream);
    void run(Stream::ptr stream) { run(*stream.get()); }

    // Partial parsing
    virtual void init();
    size_t run(const char *buf, size_t len, bool isEof);

    virtual bool complete() const = 0;
    virtual bool error() const = 0;

protected:
    virtual void exec() = 0;

protected:
    // Ragel state
    int cs;
    const char *p, *pe, *eof, *mark;
    std::string m_fullString;
};

#endif
