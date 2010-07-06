#ifndef __MORDOR_HTTP_SERVER_H__
#define __MORDOR_HTTP_SERVER_H__
// Copyright (c) 2009 - Mozy, Inc.

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

    bool committed() const { return m_committed; }

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
    Scheduler *m_scheduler;
    boost::shared_ptr<Fiber> m_fiber;
    Request m_request;
    Response m_response;
    EntityHeaders m_requestTrailer, m_responseTrailer;
    bool m_requestDone, m_committed, m_responseDone, m_responseInFlight, m_aborted, m_willClose;
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
        boost::function<void (ServerRequest::ptr)> dg, size_t maxPipelineDepth = 5);

    void processRequests();

    std::vector<ServerRequest::const_ptr> requests();

private:
    void scheduleSingleRequest();
    void scheduleNextRequest(ServerRequest *currentRequest);
    void scheduleNextResponse(ServerRequest *currentRequest);
    void scheduleAllWaitingResponses();

private:
    boost::function<void (ServerRequest::ptr)> m_dg;
    boost::mutex m_mutex;
    std::list<ServerRequest *> m_pendingRequests;
    std::set<ServerRequest *> m_waitingResponses;
    bool m_priorRequestFailed, m_priorRequestClosed, m_priorResponseClosed;
    size_t m_maxPipelineDepth;

    void invariant() const;
};

// Helper functions
void respondError(ServerRequest::ptr request, Status status,
    const std::string &message = "", bool closeConnection = false);
void respondStream(ServerRequest::ptr request, boost::shared_ptr<Stream> response);

}}

#endif
