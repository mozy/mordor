#ifndef __MORDOR_CAT_STREAM__
#define __MORDOR_CAT_STREAM__
// Copyright (c) 2009 - Decho Corporation

#include <vector>

#include "stream.h"

namespace Mordor {

class CatStream : public Stream
{
public:
    CatStream(const std::vector<Stream::ptr> &streams);

    bool supportsRead() { return true; }
    bool supportsSeek() { return m_seekable; }
    bool supportsTell() { return true; }
    bool supportsSize() { return m_size != -1ll; }

    size_t read(Buffer &buffer, size_t length);

    long long seek(long long offset, Anchor anchor = BEGIN);
    long long size();

private:
    std::vector<Stream::ptr> m_streams;
    std::vector<Stream::ptr>::iterator m_it;
    bool m_seekable;
    long long m_size;
    long long m_pos;
};

};

#endif
