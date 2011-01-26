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
    typedef boost::shared_ptr<HTTPStream> ptr;

public:
    HTTPStream(const URI &uri, HTTP::RequestBroker::ptr requestBroker,
        boost::function<bool (size_t)> delayDg = NULL);
    HTTPStream(const HTTP::Request &requestHeaders,
        HTTP::RequestBroker::ptr requestBroker,
        boost::function<bool (size_t)> delayDg = NULL);

    void sharedRetryCounter(size_t *retries) { mp_retries = retries; }

    void eTag(const HTTP::ETag &eTag) { m_eTag = eTag; }
    HTTP::ETag eTag();

    /// Force the transfer to begin (i.e. so a call to size() won't try to
    /// do an extra HEAD)
    ///
    /// @note If the current read advice is zero, this will be forced to do an
    /// HTTP request for the entire entity
    void start();
    /// This will abandon any current transfer in progress.  If it returns
    /// true, the transfer will have already begun at the current position
    bool checkModified();
    const HTTP::Response &response();

    bool supportsRead() { return true; }
    bool supportsSeek() { return true; }
    bool supportsSize();
    bool supportsTell() { return true; }

    size_t read(Buffer &buffer, size_t length);
    /// Advise about subsequent read requests, so that a more efficient HTTP
    /// request can be used.
    ///
    /// Setting the advice sets the minimum number of bytes per HTTP request.
    /// The default is ~0ull, which means only one HTTP request will be needed.
    /// Setting to 0 would mean each call to read will generate a new HTTP
    /// request.
    void adviseRead(unsigned long long advice) { m_readAdvice = advice; }
    long long seek(long long offset, Anchor anchor = BEGIN);
    long long size();

private:
    void start(size_t length);
    void stat();

private:
    HTTP::Request m_requestHeaders;
    HTTP::Response m_response;
    HTTP::RequestBroker::ptr m_requestBroker;
    HTTP::ETag m_eTag;
    long long m_pos, m_size;
    unsigned long long m_readAdvice, m_readRequested;
    boost::function<bool (size_t)> m_delayDg;
    size_t *mp_retries;
};

}

#endif
