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
    enum Flags {
        READ      = 0x01,
        WRITE     = 0x02,
        READWRITE = 0x03,
        APPEND    = 0x06,
    };
    typedef DWORD CreateFlags;
#else
    enum Flags {
        READ = O_RDONLY,
        WRITE = O_WRONLY,
        READWRITE = O_RDWR,
        APPEND = O_APPEND | O_WRONLY
    };

    enum CreateFlags {
        /// Create a file. Fail if it exists.
        CREATE_NEW = 1,
        /// Create a file. If it exists, recreate it from scratch.        
        CREATE_ALWAYS,
        /// Open a file. Fail if it does not exist.
        OPEN_EXISTING,
        /// Open a file. Create it if it does not exist.
        OPEN_ALWAYS,
        /// Open a file, and recreate it from scratch. Fail if it does not exist.
        TRUNCATE_EXISTING
    };
#endif

private:
    void init(IOManager *ioManager, Scheduler *scheduler,
        const std::string &filename, Flags flags, CreateFlags createFlags);
#ifdef WINDOWS
    void init(IOManager *ioManager, Scheduler *scheduler,
        const std::wstring &filename, Flags flags, CreateFlags createFlags);
#endif

public:
    FileStream(const std::string &filename, Flags flags = READWRITE,
        CreateFlags createFlags = OPEN_EXISTING)
    { init(NULL, NULL, filename, flags, createFlags); }
    FileStream(IOManager &ioManager, const std::string &filename,
        Flags flags = READWRITE, CreateFlags createFlags = OPEN_EXISTING)
    { init(&ioManager, NULL, filename, flags, createFlags); }
    FileStream(Scheduler &scheduler, const std::string &filename,
        Flags flags = READWRITE, CreateFlags createFlags = OPEN_EXISTING)
    { init(NULL, &scheduler, filename, flags, createFlags); }
    FileStream(IOManager &ioManager, Scheduler &scheduler,
        const std::string &filename, Flags flags = READWRITE,
        CreateFlags createFlags = OPEN_EXISTING)
    { init(&ioManager, &scheduler, filename, flags, createFlags); }
#ifdef WINDOWS
    FileStream(const std::wstring &filename, Flags flags = READWRITE,
        CreateFlags createFlags = OPEN_EXISTING)
    { init(NULL, NULL, filename, flags, createFlags); }
    FileStream(IOManager &ioManager, const std::wstring &filename,
        Flags flags = READWRITE, CreateFlags createFlags = OPEN_EXISTING)
    { init(&ioManager, NULL, filename, flags, createFlags); }
    FileStream(Scheduler &scheduler, const std::wstring &filename,
        Flags flags = READWRITE, CreateFlags createFlags = OPEN_EXISTING)
    { init(NULL, &scheduler, filename, flags, createFlags); }
    FileStream(IOManager &ioManager, Scheduler &scheduler,
        const std::wstring &filename, Flags flags = READWRITE,
        CreateFlags createFlags = OPEN_EXISTING)
    { init(&ioManager, &scheduler, filename, flags, createFlags); }
#endif

    bool supportsRead() { return m_supportsRead && NativeStream::supportsRead(); }
    bool supportsWrite() { return m_supportsWrite && NativeStream::supportsWrite(); }
    bool supportsSeek() { return m_supportsSeek && NativeStream::supportsSeek(); }

private:
    bool m_supportsRead, m_supportsWrite, m_supportsSeek;
};

}

#endif
