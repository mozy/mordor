#ifndef __MORDOR_HANDLE_STREAM_H__
#define __MORDOR_HANDLE_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include <windows.h>

#include "mordor/iomanager_iocp.h"
#include "stream.h"

namespace Mordor {

class HandleStream : public Stream
{
protected:
    HandleStream();
    void init(IOManagerIOCP *ioManager, Scheduler *scheduler, HANDLE hFile, bool own = true);
public:
    HandleStream(HANDLE hFile, bool own= true)
    { init(NULL, NULL, hFile, own); }
    HandleStream(IOManagerIOCP &ioManager, HANDLE hFile, bool own = true)
    { init(&ioManager, NULL, hFile, own); }
    HandleStream(Scheduler &scheduler, HANDLE hFile, bool own = true)
    { init(NULL, &scheduler, hFile, own); }
    HandleStream(IOManagerIOCP &ioManager, Scheduler &scheduler, HANDLE hFile, bool own = true)
    { init(&ioManager, &scheduler, hFile, own); }
    ~HandleStream();

    bool supportsRead() { return true; }
    bool supportsWrite() { return true; }
    bool supportsSeek() { return GetFileType(m_hFile) == FILE_TYPE_DISK; }
    bool supportsSize() { return supportsSeek(); }
    bool supportsTruncate() { return supportsSeek(); }

    void close(CloseType type = BOTH);
    size_t read(Buffer &b, size_t len);
    size_t write(const Buffer &b, size_t len);
    long long seek(long long offset, Anchor anchor);
    long long size();
    void truncate(long long size);

    HANDLE handle() { return m_hFile; }

protected:
    IOManagerIOCP *m_ioManager;
    Scheduler *m_scheduler;
    AsyncEventIOCP m_readEvent;
    AsyncEventIOCP m_writeEvent;
    long long m_pos;
    HANDLE m_hFile;
    bool m_own;
};

typedef HandleStream NativeStream;
typedef HANDLE NativeHandle;

}

#endif
