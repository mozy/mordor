#ifndef __MORDOR_NAMEDPIPE_STREAM_H__
#define __MORDOR_NAMEDPIPE_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "handle.h"

namespace Mordor {

class NamedPipeStream : public HandleStream
{
public:
    enum Flags {
        READ = PIPE_ACCESS_INBOUND,
        WRITE = PIPE_ACCESS_OUTBOUND,
        READWRITE = PIPE_ACCESS_DUPLEX
    };

private:
    void init(IOManagerIOCP *ioManager, Scheduler *scheduler,
        const std::string &name, Flags flags);
    void init(IOManagerIOCP *ioManager, Scheduler *scheduler,
        const std::wstring &name, Flags flags);

public:
    NamedPipeStream(const std::string &name, Flags flags = READWRITE)
    { init(NULL, NULL, name, flags); }
    NamedPipeStream(IOManagerIOCP &ioManager, const std::string &name, Flags flags = READWRITE)
    { init(&ioManager, NULL, name, flags); }
    NamedPipeStream(Scheduler &scheduler, const std::string &name, Flags flags = READWRITE)
    { init(NULL, &scheduler, name, flags); }
    NamedPipeStream(IOManagerIOCP &ioManager, Scheduler &scheduler, const std::string &name, Flags flags = READWRITE)
    { init(&ioManager, &scheduler, name, flags); }
    NamedPipeStream(const std::wstring &name, Flags flags = READWRITE)
    { init(NULL, NULL, name, flags); }
    NamedPipeStream(IOManagerIOCP &ioManager, const std::wstring &name, Flags flags = READWRITE)
    { init(&ioManager, NULL, name, flags); }
    NamedPipeStream(Scheduler &scheduler, const std::wstring &name, Flags flags = READWRITE)
    { init(NULL, &scheduler, name, flags); }
    NamedPipeStream(IOManagerIOCP &ioManager, Scheduler &scheduler, const std::wstring &name, Flags flags = READWRITE)
    { init(&ioManager, &scheduler, name, flags); }

    bool supportsRead() { return m_supportsRead; }
    bool supportsWrite() { return m_supportsWrite; }
    bool supportsSeek() { return false; }

    void close(CloseType type = BOTH);

    void accept();

private:
    bool m_supportsRead, m_supportsWrite;
};

}

#endif
