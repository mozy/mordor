#ifndef __MORDOR_HTTP_STREAM__
#define __MORDOR_HTTP_STREAM__
// Copyright (c) 2009 - Mozy, Inc.

#include "filter.h"
#include "mordor/exception.h"
#include "mordor/http/broker.h"

namespace Mordor {

struct EntityChangedException : virtual Exception {};

class HTTPStream : public FilterStream
{
public:
    HTTPStream(const URI &uri, HTTP::RequestBroker::ptr requestBroker,
        boost::function<bool (size_t)> delayDg = NULL);
    HTTPStream(const HTTP::Request &requestHeaders,
        HTTP::RequestBroker::ptr requestBroker,
        boost::function<bool (size_t)> delayDg = NULL);

    void sharedRetryCounter(size_t *retries) { mp_retries = retries; }

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
    HTTP::Request m_requestHeaders;
    HTTP::RequestBroker::ptr m_requestBroker;
    long long m_pos, m_size;
    boost::function<bool (size_t)> m_delayDg;
    size_t *mp_retries;
};

}

#endif
