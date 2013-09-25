#ifndef __MORDOR_TEE_STREAM_H__
#define __MORDOR_TEE_STREAM_H__
// Copyright (c) 2013 - Cody Cutrer

#include <vector>

#include "stream.h"

namespace Mordor {

class TeeStream : public Stream
{
public:
    TeeStream(std::vector<Stream::ptr> outputs, int parallelism = -2, bool own = true);

    bool supportsWrite() { return true; }

    void close(CloseType type = BOTH);

    size_t write(const Buffer &buffer, size_t length);
    void flush(bool flushOutputs = true);

    bool ownsOutputs() const { return m_own; }

private:
    static void doWrites(Stream::ptr output, const Buffer &buffer, size_t length);

private:
    std::vector<Stream::ptr> m_outputs;
    int m_parallelism;
    bool m_own;
};

}

#endif

