#ifndef __MORDOR_HTTP_CLIENT_H__
#define __MORDOR_HTTP_CLIENT_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <list>
#include <set>

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>

#include "connection.h"
#include "mordor/fiber.h"
#include "mordor/streams/stream.h"
#include "multipart.h"

namespace Mordor {

class Scheduler;
class TimeoutStream;
class Timer;
class TimerManager;

namespace HTTP {

class ClientConnection;
class ClientRequest : public boost::enable_shared_from_this<ClientRequest>, boost::noncopyable
{
private:
    friend class ClientConnection;

public:
    typedef boost::shared_ptr<ClientRequest> ptr;
    typedef boost::weak_ptr<ClientRequest> weak_ptr;

    /// The ClientRequest has a state for sending the request and receiving
    /// the response.  It progresses through these states (possibly skipping
    /// some of them).
    enum State {
        /// In line, but not actively waiting
        PENDING,
        /// Waiting for a prior request/response to complete
        WAITING,
        /// Reading/writing headers
        HEADERS,
        /// Reading/writing message body
        BODY,
        /// Complete
        COMPLETE,
        /// Error
        ERROR,
        /// Cancelled
        CANCELED
    };

private:
    ClientRequest(boost::shared_ptr<ClientConnection> conn, const Request &request);

public:
    ~ClientRequest();

    boost::shared_ptr<ClientConnection> connection() { return m_conn; }
    State requestState() const { return m_requestState; }
    State responseState() const { return m_responseState; }

    const Request &request();
    bool hasRequestBody() const;
    Stream::ptr requestStream();
    Multipart::ptr requestMultipart();
    EntityHeaders &requestTrailer();

    const Response &response();
    bool hasResponseBody();
    Stream::ptr responseStream();
    Multipart::ptr responseMultipart();
    const EntityHeaders &responseTrailer() const;

    Stream::ptr stream();

    void cancel(bool abort = false) { cancel(abort, false); }
    void finish();
    void ensureResponse();

private:
    void doRequest();
    void requestMultipartDone();
    void requestDone();
    void requestFailed();
    void responseDone();
    void cancel(bool abort, bool error);

private:
    boost::shared_ptr<ClientConnection> m_conn;
    unsigned long long m_requestNumber;
    Scheduler *m_scheduler;
    Fiber::ptr m_fiber;
    Request m_request;
    Response m_response;
    EntityHeaders m_requestTrailer, m_responseTrailer;
    State m_requestState, m_responseState;
    boost::exception_ptr m_priorResponseException;
    bool m_badTrailer, m_incompleteTrailer, m_hasResponseBody;
    Stream::ptr m_requestStream;
    boost::weak_ptr<Stream> m_responseStream;
    Multipart::ptr m_requestMultipart;
    boost::weak_ptr<Multipart> m_responseMultipart;
};

// Logically the entire response is unexpected
struct InvalidResponseException : virtual HTTP::Exception
{
public:
    InvalidResponseException(const std::string &message, ClientRequest::ptr request)
        : m_message(message),
          m_request(request)
    {}
    InvalidResponseException(ClientRequest::ptr request)
        : m_request(request)
    {}
    ~InvalidResponseException() throw() {}

    const char *what() const throw() { return m_message.c_str(); }
    ClientRequest::ptr request() { return m_request; }

private:
    std::string m_message;
    ClientRequest::ptr m_request;
};

class ClientConnection : public Connection, public boost::enable_shared_from_this<ClientConnection>, boost::noncopyable
{
private:
    friend class ClientRequest;

public:
    typedef boost::shared_ptr<ClientConnection> ptr;

public:
    ClientConnection(Stream::ptr stream, TimerManager *timerManager = NULL);
    ~ClientConnection();

    ClientRequest::ptr request(const Request &requestHeaders);

    bool newRequestsAllowed();
    size_t outstandingRequests();

    bool supportsTimeouts() const;

    void readTimeout(unsigned long long us);
    void writeTimeout(unsigned long long us);
    void idleTimeout(unsigned long long us, boost::function<void ()> dg);

private:
    void scheduleNextRequest(ClientRequest *currentRequest);
    void scheduleNextResponse(ClientRequest *currentRequest);
    void scheduleAllWaitingRequests();
    void scheduleAllWaitingResponses();

private:
    boost::mutex m_mutex;
    boost::shared_ptr<TimeoutStream> m_timeoutStream;
    unsigned long long m_readTimeout, m_idleTimeout;
    boost::shared_ptr<Timer> m_idleTimer;
    TimerManager *m_timerManager;
    boost::function<void ()> m_idleDg;
    std::list<ClientRequest *> m_pendingRequests;
    std::list<ClientRequest *>::iterator m_currentRequest;
    std::set<ClientRequest *> m_waitingResponses;
    bool m_allowNewRequests;
    bool m_priorRequestFailed;
    unsigned long long m_requestCount, m_priorResponseFailed, m_priorResponseClosed;
    size_t m_connectionNumber;

    void invariant() const;
};

}}

#endif
