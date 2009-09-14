#ifndef __STD_H__
#define __STD_H__
// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/iomanager.h"
#include "mordor/common/version.h"

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
    StdinStream(IOManager &ioManager);

    bool supportsWrite() { return false; }
};

class StdoutStream : public NativeStream
{
public:
    StdoutStream();
    StdoutStream(IOManager &ioManager);

    bool supportsRead() { return false; }
};

class StderrStream : public NativeStream
{
public:
    StderrStream();
    StderrStream(IOManager &ioManager);

    bool supportsRead() { return false; }
};

#endif
