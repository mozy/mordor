#ifndef __MORDOR_STD_STREAM_H__
#define __MORDOR_STD_STREAM_H__
// Copyright (c) 2009 - Decho Corporation

#include "mordor/version.h"

#ifdef WINDOWS
#include "handle.h"
#else
#include "fd.h"
#endif

namespace Mordor {

class StdStream : public NativeStream
{
protected:
    StdStream(IOManager *ioManager, Scheduler *scheduler, int stream);
};

class StdinStream : public StdStream
{
public:
    StdinStream()
        : StdStream(NULL, NULL, MORDOR_NATIVE(STD_INPUT_HANDLE, STDIN_FILENO))
    {}
    StdinStream(IOManager &ioManager)
        : StdStream(&ioManager, NULL, MORDOR_NATIVE(STD_INPUT_HANDLE, STDIN_FILENO))
    {}
    StdinStream(Scheduler &scheduler)
        : StdStream(NULL, &scheduler, MORDOR_NATIVE(STD_INPUT_HANDLE, STDIN_FILENO))
    {}
    StdinStream(IOManager &ioManager, Scheduler &scheduler)
        : StdStream(&ioManager, &scheduler, MORDOR_NATIVE(STD_INPUT_HANDLE, STDIN_FILENO))
    {}

    bool supportsWrite() { return false; }
};

class StdoutStream : public StdStream
{
public:
    StdoutStream()
        : StdStream(NULL, NULL, MORDOR_NATIVE(STD_OUTPUT_HANDLE, STDOUT_FILENO))
    {}
    StdoutStream(IOManager &ioManager)
        : StdStream(&ioManager, NULL, MORDOR_NATIVE(STD_OUTPUT_HANDLE, STDOUT_FILENO))
    {}
    StdoutStream(Scheduler &scheduler)
        : StdStream(NULL, &scheduler, MORDOR_NATIVE(STD_OUTPUT_HANDLE, STDOUT_FILENO))
    {}
    StdoutStream(IOManager &ioManager, Scheduler &scheduler)
        : StdStream(&ioManager, &scheduler, MORDOR_NATIVE(STD_OUTPUT_HANDLE, STDOUT_FILENO))
    {}

    bool supportsRead() { return false; }
};

class StderrStream : public StdStream
{
public:
    StderrStream()
        : StdStream(NULL, NULL, MORDOR_NATIVE(STD_ERROR_HANDLE, STDERR_FILENO))
    {}
    StderrStream(IOManager &ioManager)
        : StdStream(&ioManager, NULL, MORDOR_NATIVE(STD_ERROR_HANDLE, STDERR_FILENO))
    {}
    StderrStream(Scheduler &scheduler)
        : StdStream(NULL, &scheduler, MORDOR_NATIVE(STD_ERROR_HANDLE, STDERR_FILENO))
    {}
    StderrStream(IOManager &ioManager, Scheduler &scheduler)
        : StdStream(&ioManager, &scheduler, MORDOR_NATIVE(STD_ERROR_HANDLE, STDERR_FILENO))
    {}

    bool supportsRead() { return false; }
};

}

#endif
