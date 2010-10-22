#ifndef __MORDOR_RAGEL_H__
#define __MORDOR_RAGEL_H__
// Copyright (c) 2009 - Decho Corporation

#include <stdexcept>
#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>

namespace Mordor {

struct Buffer;
class Stream;

class RagelParser
{
public:
    virtual ~RagelParser() {}

    // Complete parsing
    size_t run(const void *buffer, size_t length);
    size_t run(const char *string);
    size_t run(const std::string &string);
    size_t run(const Buffer &buffer);
    unsigned long long run(Stream &stream);
    unsigned long long run(boost::shared_ptr<Stream> stream) { return run(*stream); }

    // Partial parsing
    virtual void init();
    size_t run(const void *buffer, size_t length, bool isEof);

    virtual bool complete() const { return final(); }
    virtual bool final() const = 0;
    virtual bool error() const = 0;

protected:
    virtual void exec() = 0;

    virtual const char *earliestPointer() const;
    virtual void adjustPointers(ptrdiff_t offset);

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

}

#endif
