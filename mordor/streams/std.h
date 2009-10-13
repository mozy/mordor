#ifndef __MORDOR_STD_STREAM_H__
#define __MORDOR_STD_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "mordor/iomanager.h"
#include "mordor/version.h"

#ifdef WINDOWS
#include "handle.h"
#else
#include "fd.h"
#endif

namespace Mordor {

#ifdef WINDOWS
typedef HandleStream NativeStream;
#else
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

}

#endif
