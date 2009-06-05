#ifndef __STD_H__
#define __STD_H__
// Copyright (c) 2009 - Decho Corp.

#include "common/version.h"

#ifdef WINDOWS
#include "handle.h"
typedef HandleStream NativeStream;
#else
#include "fd.h"
typedef FDStream NativeStream;
#endif

class StdinStream : public NativeStream
{
public:
    StdinStream();

    bool supportsWrite() { return false; }
};

class StdoutStream : public NativeStream
{
public:
    StdoutStream();

    bool supportsRead() { return false; }
};

class StderrStream : public NativeStream
{
public:
    StderrStream();

    bool supportsRead() { return false; }
};

#endif
