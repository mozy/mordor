#ifndef __MORDOR_HANDLE_STREAM_H__
#define __MORDOR_HANDLE_STREAM_H__
// Copyright (c) 2009 - Decho Corporation

#include <windows.h>

#include "mordor/iomanager.h"
#include "stream.h"

namespace Mordor {

class HandleStream : public Stream
{
public:
    typedef boost::shared_ptr<HandleStream> ptr;

protected:
    HandleStream();
    void init(HANDLE hFile, IOManager *ioManager = NULL,
        Scheduler *scheduler = NULL, bool own = true);
public:
    HandleStream(HANDLE hFile, IOManager *ioManager = NULL,
        Scheduler *scheduler = NULL, bool own = true) :
        m_maxOpSize(0xffffffff)
    { init(hFile, ioManager, scheduler, own); }
    ~HandleStream();

    bool supportsRead() { return true; }
    bool supportsWrite() { return true; }
    bool supportsCancel();
    bool supportsSeek() { return GetFileType(m_hFile) == FILE_TYPE_DISK; }
    bool supportsSize() { return supportsSeek(); }
    bool supportsTruncate() { return supportsSeek(); }

    void close(CloseType type = BOTH);
    size_t read(void *buffer, size_t length);
    void cancelRead();
    size_t write(const void *buffer, size_t length);
    void cancelWrite();
    long long seek(long long offset, Anchor anchor = BEGIN);
    long long size();
    void truncate(long long size);
    void flush(bool flushParent = true);

    HANDLE handle() { return m_hFile; }

protected:
    IOManager *m_ioManager;
    bool m_skipCompletionPortOnSuccess;
    Scheduler *m_scheduler;
    AsyncEvent m_readEvent;
    AsyncEvent m_writeEvent;
    long long m_pos;
    HANDLE m_hFile;
    bool m_own, m_cancelRead, m_cancelWrite;
    size_t m_maxOpSize;
};

typedef HandleStream NativeStream;
typedef HANDLE NativeHandle;

}

#endif
