#ifndef __FILE_H__
#define __FILE_H__
// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/version.h"

#ifdef WINDOWS
#include "handle.h"
typedef HandleStream NativeStream;
typedef HANDLE NativeHandle;
#else
#include "fd.h"
#include <fcntl.h>
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
        READWRITE = 0x03
    };
    typedef DWORD CreateFlags;
#else
    enum Flags {
        READ = O_RDONLY,
        WRITE = O_WRONLY,
        READWRITE = O_RDWR
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

    bool supportsRead() { return m_supportsRead && NativeStream::supportsRead(); }
    bool supportsWrite() { return m_supportsWrite && NativeStream::supportsWrite(); }

private:
    bool m_supportsRead, m_supportsWrite;
};

#endif
