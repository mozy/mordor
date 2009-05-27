#ifndef __HANDLE_H__
#define __HANDLE_H__
// Copyright (c) 2009 - Decho Corp.

#include <windows.h>

#include "iomanager_iocp.h"
#include "stream.h"

class HandleStream : public Stream
{
protected:
    HandleStream();
    void init(HANDLE hFile, bool own = true);
    void init(IOManagerIOCP *ioManager, HANDLE hFile, bool own = true);
public:
    HandleStream(HANDLE hFile, bool ownHandle = true);
    HandleStream(IOManagerIOCP *ioManager, HANDLE hFile, bool own = true);
    ~HandleStream();

    bool supportsRead() { return true; }
    bool supportsWrite() { return true; }
    bool supportsSeek() { return GetFileType(m_hFile) == FILE_TYPE_DISK; }
    bool supportsSize() { return supportsSeek(); }
    bool supportsTruncate() { return supportsSeek(); }

    void close(CloseType type = BOTH);
    size_t read(Buffer *b, size_t len);
    size_t write(const Buffer *b, size_t len);
    long long seek(long long offset, Anchor anchor);
    long long size();
    void truncate(long long size);

private:
    IOManagerIOCP *m_ioManager;
    AsyncEventIOCP m_readEvent;
    AsyncEventIOCP m_writeEvent;
    long long m_pos;
    HANDLE m_hFile;
    bool m_own;
};

#endif
