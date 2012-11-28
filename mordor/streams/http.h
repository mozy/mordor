#ifndef __MORDOR_HTTP_STREAM__
#define __MORDOR_HTTP_STREAM__
// Copyright (c) 2009 - Mozy, Inc.

#include "filter.h"
#include "mordor/exception.h"
#include "mordor/future.h"
#include "mordor/http/broker.h"

namespace Mordor {

struct EntityChangedException : virtual Exception {};

/// @note When using HTTPStream for PUTs, automatic retry is not supported
/// (since HTTPStream does not see the entire request body in a single call,
/// the retry must be handled at a higher level).
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
    ~HTTPStream();

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
    bool supportsWrite() { return true; }
    bool supportsSeek() { return true; }
    bool supportsSize();
    bool supportsTruncate() { return true; }
    bool supportsTell() { return true; }

    void close(CloseType type = BOTH);

    using FilterStream::read;
    size_t read(Buffer &buffer, size_t length);
    using FilterStream::write;
    size_t write(const Buffer &buffer, size_t length);
    long long seek(long long offset, Anchor anchor = BEGIN);
    long long size();
    /// @note Truncate is accomplished by doing a PUT with
    /// Content-Range: */<size>.
    void truncate(long long size);
    void flush(bool flushParent = true);

    /// Advise about subsequent read calls, so that more efficient HTTP
    /// requests can be used.
    ///
    /// Sets the minimum number of bytes per HTTP request.  The default is
    /// ~0ull, which means only one HTTP request will be needed.  Setting to 0
    /// would mean each call to read will generate a new HTTP request.
    void adviseRead(unsigned long long advice) { m_readAdvice = advice; }
    /// Advise about subsequent write calls, so that more efficient HTTP
    /// requests can be used.
    ///
    /// Sets the minimum number of bytes per HTTP request.  The default is
    /// ~0ull, which means only one HTTP request will be needed.  Setting to 0
    /// would mean each call to write will generate a new HTTP request.
    /// @warning If you write beyond the initial advice, you will invoke
    /// non-standardized HTTP behavior that most servers will not support.
    /// See http://code.google.com/p/gears/wiki/ContentRangePostProposal
    /// and http://www.hpl.hp.com/personal/ange/archives/archives-97/http-wg-archive/2530.html
    void adviseWrite(unsigned long long advice) { m_writeAdvice = advice; }
    /// Advise about the eventual size of the stream, so that a more efficient
    /// HTTP request can be used.
    ///
    /// Setting this will avoid using chunked encoding.
    void adviseSize(long long advice) { m_sizeAdvice = advice; }

private:
    void start(size_t length);
    void stat();
    void doWrite(boost::shared_ptr<HTTP::ClientRequest> request);
    void startWrite();
    void clearParent(bool error = false);

private:
    HTTP::Request m_requestHeaders;
    HTTP::Response m_response;
    HTTP::RequestBroker::ptr m_requestBroker;
    HTTP::ETag m_eTag;
    long long m_pos, m_size, m_sizeAdvice;
    unsigned long long m_readAdvice, m_writeAdvice,
        m_readRequested, m_writeRequested;
    boost::function<bool (size_t)> m_delayDg;
    size_t *mp_retries;
    bool m_writeInProgress, m_abortWrite;
    Future<> m_writeFuture, m_writeFuture2;
    boost::shared_ptr<HTTP::ClientRequest> m_writeRequest;
    boost::shared_ptr<HTTP::ClientRequest> m_readRequest;
    boost::exception_ptr m_writeException;
};

}

#endif
