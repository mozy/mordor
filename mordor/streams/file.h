#ifndef __MORDOR_FILE_STREAM_H__
#define __MORDOR_FILE_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "mordor/version.h"

#ifdef WINDOWS
#include "handle.h"
#else
#include <fcntl.h>

#include "fd.h"
#endif

namespace Mordor {

class FileStream : public NativeStream
{
public:
#ifdef WINDOWS
    enum AccessFlags {
        READ      = 0x01,
        WRITE     = 0x02,
        READWRITE = 0x03,
        APPEND    = 0x06,
    };
    enum CreateFlags {
        OPEN = OPEN_EXISTING,
        CREATE = CREATE_NEW,
        OPEN_OR_CREATE = OPEN_ALWAYS,
        OVERWRITE = TRUNCATE_EXISTING,
        OVERWRITE_OR_CREATE = CREATE_ALWAYS,

        DELETE_ON_CLOSE = 0x80000000
    };
#else
    enum AccessFlags {
        READ = O_RDONLY,
        WRITE = O_WRONLY,
        READWRITE = O_RDWR,
        APPEND = O_APPEND | O_WRONLY
    };
    enum CreateFlags {
        /// Open a file. Fail if it does not exist.
        OPEN = 1,
        /// Create a file. Fail if it exists.
        CREATE,
        /// Open a file. Create it if it does not exist.
        OPEN_OR_CREATE,
        /// Open a file, and recreate it from scratch. Fail if it does not exist.
        OVERWRITE,
        /// Create a file. If it exists, recreate it from scratch.
        OVERWRITE_OR_CREATE,

        /// Delete the file when it is closed.  Can be combined with any of the
        /// other options
        DELETE_ON_CLOSE = 0x80000000
    };
#endif

protected:
    FileStream();
    void init(const std::string &filename,
        AccessFlags accessFlags = READWRITE, CreateFlags createFlags = OPEN,
        IOManager *ioManager = NULL, Scheduler *scheduler = NULL);
#ifdef WINDOWS
    void init(const std::wstring &filename,
        AccessFlags accessFlags = READWRITE, CreateFlags createFlags = OPEN,
        IOManager *ioManager = NULL, Scheduler *scheduler = NULL);
#endif

public:
    FileStream(const std::string &filename,
        AccessFlags accessFlags = READWRITE, CreateFlags createFlags = OPEN,
        IOManager *ioManager = NULL, Scheduler *scheduler = NULL)
    { init(filename, accessFlags, createFlags, ioManager, scheduler); }
#ifdef WINDOWS
    FileStream(const std::wstring &filename,
        AccessFlags accessFlags = READWRITE, CreateFlags createFlags = OPEN,
        IOManager *ioManager = NULL, Scheduler *scheduler = NULL)
    { init(filename, accessFlags, createFlags, ioManager, scheduler); }
#endif

    bool supportsRead() { return m_supportsRead && NativeStream::supportsRead(); }
    bool supportsWrite() { return m_supportsWrite && NativeStream::supportsWrite(); }
    bool supportsSeek() { return m_supportsSeek && NativeStream::supportsSeek(); }

private:
    bool m_supportsRead, m_supportsWrite, m_supportsSeek;
};

}

#endif
