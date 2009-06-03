#ifndef __HTTP_CHUNKED_H__
#define __HTTP_CHUNKED_H__
// Copyright (c) 2009 - Decho Corp.

#include "common/streams/filter.h"

namespace HTTP
{
    class ChunkedStream : public MutatingFilterStream
    {
    public:
        ChunkedStream(Stream::ptr parent, bool own = true);

        void close(CloseType type = BOTH);
        size_t read(Buffer *b, size_t len);
        size_t write(const Buffer *b, size_t len);

    private:
        unsigned long long m_nextChunk;
    };
};

#endif
