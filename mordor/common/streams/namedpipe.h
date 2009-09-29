#ifndef __NAMEDPIPE_H__
#define __NAMEDPIPE_H__
// Copyright (c) 2009 - Decho Corp.

#include "handle.h"

class NamedPipeStream : public HandleStream
{
public:
    enum Flags {
        READ = PIPE_ACCESS_INBOUND,
        WRITE = PIPE_ACCESS_OUTBOUND,
        READWRITE = PIPE_ACCESS_DUPLEX
    };
public:
    NamedPipeStream(const std::string &name, Flags flags = READWRITE);
    NamedPipeStream(IOManagerIOCP &ioManager, const std::string &name, Flags flags = READWRITE);
    NamedPipeStream(const std::wstring &name, Flags flags = READWRITE);
    NamedPipeStream(IOManagerIOCP &ioManager, const std::wstring &name, Flags flags = READWRITE);

    bool supportsRead() { return m_supportsRead; }
    bool supportsWrite() { return m_supportsWrite; }
    bool supportsSeek() { return false; }

    void close(CloseType type = BOTH);

    void accept();

private:
    bool m_supportsRead, m_supportsWrite;
};

#endif
