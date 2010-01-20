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
    NamedPipeStream(const std::string &name, Flags flags = READWRITE, IOManager *ioManager = NULL, Scheduler *scheduler = NULL);
    NamedPipeStream(const std::wstring &name, Flags flags = READWRITE, IOManager *ioManager = NULL, Scheduler *scheduler = NULL);

    bool supportsRead() { return m_supportsRead; }
    bool supportsWrite() { return m_supportsWrite; }
    bool supportsSeek() { return false; }

    void close(CloseType type = BOTH);

    void accept();
    void cancelAccept();

private:
    bool m_supportsRead, m_supportsWrite;
};

}

#endif
