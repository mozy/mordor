#ifndef __MORDOR_FILE_STREAM_H__
#define __MORDOR_FILE_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/version.h"

#ifdef WINDOWS
#include "handle.h"
#else
#include <fcntl.h>

#include "fd.h"
#endif

namespace Mordor {

#ifdef WINDOWS
typedef HandleStream NativeStream;
typedef HANDLE NativeHandle;
#else
typedef FDStream NativeStream;
typedef int NativeHandle;
#endif

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
        APPEND = O_APPEND
    };

    enum CreateFlags {
        CREATE_NEW = 1,
        CREATE_ALWAYS,
        OPEN_EXISTING,
        OPEN_ALWAYS,
        TRUNCATE_EXISTING
    };
#endif

    FileStream(std::string filename, Flags flags = READWRITE, CreateFlags createFlags = OPEN_EXISTING);
#ifdef WINDOWS
    FileStream(std::wstring filename, Flags flags = READWRITE, CreateFlags createFlags = OPEN_EXISTING);
#endif

    bool supportsRead() { return m_supportsRead && NativeStream::supportsRead(); }
    bool supportsWrite() { return m_supportsWrite && NativeStream::supportsWrite(); }
    bool supportsSeek() { return m_supportsSeek && NativeStream::supportsSeek(); }

private:
    bool m_supportsRead, m_supportsWrite, m_supportsSeek;
};

}

#endif
