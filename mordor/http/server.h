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
    boost::shared_ptr<Stream> requestStream();
    const EntityHeaders &requestTrailer() const;
    boost::shared_ptr<Multipart> requestMultipart();

    Response &response() { return m_response; }
    const Response &response() const { return m_response; }
    bool hasResponseBody() const;
    boost::shared_ptr<Stream> responseStream();
    boost::shared_ptr<Multipart> responseMultipart();
    EntityHeaders &responseTrailer();

    bool committed() const { return m_responseState >= HEADERS; }

    void cancel();
    void finish();

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
    bool m_willClose;
    boost::shared_ptr<Stream> m_requestStream, m_responseStream;
    boost::shared_ptr<Multipart> m_requestMultipart, m_responseMultipart;
};

class ServerConnection : public Connection, public boost::enable_shared_from_this<ServerConnection>, boost::noncopyable
{
public:
    typedef boost::shared_ptr<ServerConnection> ptr;
    typedef boost::weak_ptr<ServerConnection> weak_ptr;

private:
    friend class ServerRequest;
public:
    ServerConnection(boost::shared_ptr<Stream> stream,
        boost::function<void (ServerRequest::ptr)> dg);

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
void respondError(ServerRequest::ptr request, Status status,
    const std::string &message = "", bool closeConnection = false);
void respondStream(ServerRequest::ptr request, boost::shared_ptr<Stream> response);

}}

#endif
