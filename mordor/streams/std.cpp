// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "std.h"

#include "mordor/exception.h"

namespace Mordor {

StdStream::StdStream(IOManager *ioManager, Scheduler *scheduler, int stream)
{
#ifdef WINDOWS
    HANDLE hFile = GetStdHandle(stream);
    if (hFile == INVALID_HANDLE_VALUE)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("GetStdHandle");
    if (hFile == NULL)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(ERROR_FILE_NOT_FOUND, "GetStdHandle");
    init(ioManager, scheduler, hFile, false);
#else
    init(ioManager, scheduler, stream, false);
#endif
}

}
