#ifndef __MORDOR_HTTP_SERVER_H__
#define __MORDOR_HTTP_SERVER_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <boost/thread/mutex.hpp>

#include "connection.h"

namespace Mordor {

class Fiber;
class Multipart;
class Scheduler;

namespace HTTP {

class ServerConnection;

class ServerRequest : public boost::enable_shared_from_this<ServerRequest>, boost::noncopyable
{
private:
    friend class ServerConnection;
public:
    typedef boost::shared_ptr<ServerRequest> ptr;
    typedef boost::shared_ptr<const ServerRequest> const_ptr;

    enum State {
        PENDING,
        WAITING,
        HEADERS,
        BODY,
        COMPLETE,
        ERROR
    };

private:
    ServerRequest(boost::shared_ptr<ServerConnection> conn);

public:
    ~ServerRequest();

    const Request &request() const { return m_request; }
    bool hasRequestBody() const;
    /// A stream representing the request body
    ///
    /// The stream will be fully "decoded" according to the request headers.
    /// I.e. transfer encodings will already be applied, and it will have a
    /// size if Content-Length is set.
    /// @pre hasRequestBody()
    boost::shared_ptr<Stream> requestStream();
    const EntityHeaders &requestTrailer() const;
    boost::shared_ptr<Multipart> requestMultipart();

    /// Response Headers
    ///
    /// Changes to the headers will not do anything if the response has already
    /// been committed().
    Response &response() { return m_response; }
    const Response &response() const { return m_response; }
    bool hasResponseBody() const;

    /// A stream representing the response body
    ///
    /// Only the actual response needs to be written.  Transfer encodings
    /// will be automatically applied, and you will be unable to write beyond
    /// the size of Content-Length was set.  The response headers will be
    /// automatically committed if they have not been committed already
    boost::shared_ptr<Stream> responseStream();
    boost::shared_ptr<Multipart> responseMultipart();
    EntityHeaders &responseTrailer();

    boost::shared_ptr<ServerConnection> connection() { return m_conn; }

    bool committed() const { return m_responseState >= HEADERS; }

    /// Start reading the next request
    ///
    /// Even if this request hasn't responded yet; enables pipelining.
    /// If this request isn't complete, it will set a flag to immediately
    /// start reading the next request as soon as this one is complete.
    void processNextRequest();

    /// Abort the request
    ///
    /// Aborts this request, and any subsequent requests or responses
    void cancel();
    void finish();

    /// Context
    ///
    /// Context of the ServerRequest
    const std::string & context() const { return m_context; }
    unsigned long long requestNumber() const { return m_requestNumber; }

private:
    void doRequest();
    void commit();
    void finishRequest();
    void requestDone();
    void responseMultipartDone();
    void responseDone();

private:
    boost::shared_ptr<ServerConnection> m_conn;
    unsigned long long m_requestNumber;
    Scheduler *m_scheduler;
    boost::shared_ptr<Fiber> m_fiber;
    Request m_request;
    Response m_response;
    EntityHeaders m_requestTrailer, m_responseTrailer;
    State m_requestState, m_responseState;
    bool m_willClose, m_pipeline;
    boost::shared_ptr<Stream> m_requestStream, m_responseStream;
    boost::shared_ptr<Multipart> m_requestMultipart, m_responseMultipart;
    std::string m_context;
};

/// Individual connection to an HTTP server
///
/// A ServerConnection operates over any reliable, full-duplex stream. An HTTP
/// server needs to establish the physical connection, create a
/// ServerConnection, and call processRequests.  The ServerConnection will then
/// manager the underlying stream, and read requests off of it.  As each
/// request is read, dg is called with a ServerRequest object, and is
/// responsible for responding to it.  If dg returns without fulfilling any of
/// its duties (reading the request body, responding, exception), it will be
/// dealt with appropriately:
///  * Responding 500 if an exception happened before the headers were
///    committed
///  * Closing the connection if an exception happened after the headers were
///    committed
///  * Closing the connection if the response body was not fully written
///  * Reading the rest of the request body if it was not fully read
/// The connection will automatically manage keep alive, and responding to
/// unparseable and invalid requests (i.e. no Host header for HTTP/1.1).
/// It fully supports server side pipelining, but it is an opt-in feature:
/// ServerRequest::processNextRequest() must be called before the server will
/// start reading a pipelined request.
class ServerConnection : public Connection,
    public boost::enable_shared_from_this<ServerConnection>, boost::noncopyable
{
public:
    typedef boost::shared_ptr<ServerConnection> ptr;
    typedef boost::weak_ptr<ServerConnection> weak_ptr;

private:
    friend class ServerRequest;
public:
    ServerConnection(boost::shared_ptr<Stream> stream,
        boost::function<void (ServerRequest::ptr)> dg);

    /// Does not block; simply schedules a new fiber to read the first request
    void processRequests();

    std::vector<ServerRequest::const_ptr> requests();

private:
    void scheduleNextRequest(ServerRequest *currentRequest);
    void requestComplete(ServerRequest *currentRequest);
    void responseComplete(ServerRequest *currentRequest);
    void scheduleAllWaitingResponses();

private:
    boost::function<void (ServerRequest::ptr)> m_dg;
    boost::mutex m_mutex;
    std::list<ServerRequest *> m_pendingRequests;
    std::set<ServerRequest *> m_waitingResponses;
    unsigned long long m_requestCount, m_priorRequestFailed,
        m_priorRequestClosed, m_priorResponseClosed;

    void invariant() const;
};

// Helper functions
/// Respond with a status code
///
/// This will clear Transfer-Encoding, Content-Length, Content-Type,
/// and optionally ETag headers.
/// @param message The message to be used as the body of the response
///                (Content-Type will be set to text/plain)
void respondError(ServerRequest::ptr request, Status status,
    const std::string &message = std::string(), bool closeConnection = false,
    bool clearContentType = true, bool clearETag = true);

/// Respond with a Stream
///
/// This function will process Range, If-Range, and TE headers to stream
/// response in the most efficient way, applying transfer encodings if
/// necessary or possible
void respondStream(ServerRequest::ptr request, Stream &response);
void respondStream(ServerRequest::ptr request,
    boost::shared_ptr<Stream> response);

/// Procesess If-Match, If-None-Match headers
///
/// @param request The request
/// @param eTag The ETag of the entity currently
/// @return If the request should continue; otherwise the error has already
///         been processed, and the request is complete
bool ifMatch(ServerRequest::ptr request, const ETag &eTag);

}}

#endif
