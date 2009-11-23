#ifndef __MORDOR_HTTP_STREAM__
#define __MORDOR_HTTP_STREAM__
// Copyright (c) 2009 - Decho Corp.

#include "filter.h"
#include "mordor/exception.h"
#include "mordor/http/broker.h"

namespace Mordor {

struct EntityChangedException : virtual Exception {};

class HTTPStream : public FilterStream
{
public:
    HTTPStream(const URI &uri, HTTP::RequestBroker::ptr requestBroker);

    HTTP::ETag eTag;

    bool supportsRead() { return true; }
    bool supportsSeek() { return true; }
    bool supportsSize();
    bool supportsTell() { return true; }

    size_t read(Buffer &buffer, size_t length);
    long long seek(long long offset, Anchor anchor = BEGIN);
    long long size();

private:
    void ensureSize();

private:
    URI m_uri;
    HTTP::RequestBroker::ptr m_requestBroker;
    long long m_pos, m_size;
};

}

#endif
