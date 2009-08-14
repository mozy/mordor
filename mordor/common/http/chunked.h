#ifndef __HTTP_CHUNKED_H__
#define __HTTP_CHUNKED_H__
// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/streams/filter.h"

namespace HTTP
{
    class InvalidChunkError : public StreamError
    {
    public:
        enum Type
        {
            HEADER,
            FOOTER
        };
        InvalidChunkError(const std::string &line, Type type);
        ~InvalidChunkError() throw() {}

        const std::string &line() const { return m_line; }
        Type type() const { return m_type; }

    private:
        std::string m_line;
        Type m_type;
    };

    class ChunkedStream : public MutatingFilterStream
    {
    public:
        ChunkedStream(Stream::ptr parent, bool own = true);

        void close(CloseType type = BOTH);
        size_t read(Buffer &b, size_t len);
        size_t write(const Buffer &b, size_t len);

    private:
        unsigned long long m_nextChunk;
    };
};

#endif
