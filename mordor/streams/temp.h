#ifndef __MORDOR_TEMP_STREAM_H__
#define __MORDOR_TEMP_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "file.h"

namespace Mordor {

#ifdef WINDOWS
typedef FileStream TempStreamBase;
#else
typedef FDStream TempStreamBase;
#endif

class TempStream : public TempStreamBase
{
public:
    /// Create a stream representing a temporary file

    /// The file will be created with FileStream::DELETE_ON_CLOSE flag, to
    /// guarantee the file will be cleaned up when the TempStream destructs
    /// (or the process dies).
    /// @param prefix The prefix for the temporary file.  If it is an absolute
    /// path, the file will be created in that location.  If it is relative, it
    /// will be relative to the system temporary directory.  In either case,
    /// a suffix will be added to make it unique.
    /// @param ioManager The IOManager to use for any asynchronous I/O, if
    /// possible
    /// @param scheduler The Scheduler to switchTo for any blocking I/O
    TempStream(const std::string &prefix = "", IOManager *ioManager = NULL,
        Scheduler *scheduler = NULL);
};

}

#endif
