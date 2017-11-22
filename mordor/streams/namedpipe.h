#ifndef __MORDOR_NAMEDPIPE_STREAM_H__
#define __MORDOR_NAMEDPIPE_STREAM_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "handle.h"

namespace Mordor {

class NamedPipeStream : public HandleStream
{
public:
    typedef boost::shared_ptr<NamedPipeStream> ptr;

    enum Flags {
        READ = PIPE_ACCESS_INBOUND,
        WRITE = PIPE_ACCESS_OUTBOUND,
        READWRITE = PIPE_ACCESS_DUPLEX
    };

public:

    // Currently this class only supports the Server side of a named pipe connection
    // It creates a new pipe based on the passed name argument.
    // By default a byte-mode pipe is created (PIPE_TYPE_BYTE), but the pipeModeFlags
    // argument can be used to create a message-mode pipe.
    NamedPipeStream(const std::string &name, Flags flags = READWRITE, IOManager *ioManager = NULL, Scheduler *scheduler = NULL, DWORD pipeModeFlags = (DWORD)-1);
    NamedPipeStream(const std::wstring &name, Flags flags = READWRITE, IOManager *ioManager = NULL, Scheduler *scheduler = NULL, DWORD pipeModeFlags = (DWORD)-1);

    bool supportsRead() { return m_supportsRead; }
    bool supportsWrite() { return m_supportsWrite; }
    bool supportsSeek() { return false; }

    void close(CloseType type = BOTH);

    // Close a connected client if any, but leave the named pipe open.
    // Should be called after processing a client request and before
    // calling accept to wait for the next connection.
    void disconnectClient();

    // Accept will put the fiber to sleep until a client connection arrives
    // Throws OperationAbortedException if another fiber calls cancelAccept()
    void accept();

    void cancelAccept();

private:
    void init(const std::wstring &name, Flags flags, DWORD pipeModeFlags, IOManager *ioManager, Scheduler *scheduler);

    bool m_supportsRead, m_supportsWrite;
};

}

#endif
