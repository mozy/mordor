#ifndef __MORDOR_EFS_STREAM__
#define __MORDOR_EFS_STREAM__
// Copyright (c) 2009 - Mozy, Inc.

#include "stream.h"

namespace Mordor {

class EFSStream : public Stream
{
public:
    EFSStream(void *context, bool read = true, bool ownContext = true);
    EFSStream(const char *filename, bool read = true);
    EFSStream(const wchar_t *filename, bool read = true);
    EFSStream(const std::string &filename, bool read = true);
    EFSStream(const std::wstring &filename, bool read = true);
    ~EFSStream();

    bool supportsRead() { return m_read; }
    bool supportsWrite() { return !m_read; }
    bool supportsSeek() { return m_read; }

    void close(CloseType type = BOTH);
    size_t read(Buffer &b, size_t len);
    size_t write(const Buffer &b, size_t len);
    long long seek(long long offset, Anchor anchor = BEGIN);

private:
    void init();
    void readFiber();
    static DWORD WINAPI ExportCallback(PBYTE pbData, PVOID pvCallbackContext,
        ULONG ulLength);
    void writeFiber();
    static DWORD WINAPI ImportCallback(PBYTE pbData, PVOID pvCallbackContext,
        PULONG ulLength);

private:
    Fiber::ptr m_fiber;
    void *m_context;
    bool m_read, m_own;
    union {
        Buffer *m_readBuffer;
        const Buffer *m_writeBuffer;
    };
    size_t m_todo;
    long long m_pos;
    long long m_seekTarget;
};

}

#endif
