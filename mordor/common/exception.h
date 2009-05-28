#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__
// Copyright (c) 2009 - Decho Corp.

#include <stdexcept>
#include <string>

#include "version.h"

class StreamError : public std::runtime_error
{
public:
    StreamError() : std::runtime_error("") {}
};

class UnexpectedEofError : public StreamError
{
public:
    UnexpectedEofError() {}
};

#ifdef WINDOWS
class Win32Error : public std::runtime_error
{
public:
    Win32Error(unsigned int lastError);
    const char *what() const { return m_message.c_str(); }

    unsigned int lastError() { return m_lastError; }

private:
    int m_lastError;
    std::string m_message;
};
typedef Win32Error NativeError;
#endif

void throwExceptionFromLastError();
void throwExceptionFromLastError(unsigned int lastError);

#endif
