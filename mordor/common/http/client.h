#ifndef __HTTP_CLIENT_H__
#define __HTTP_CLIENT_H__
// Copyright (c) 2009 - Decho Corp.

#include <list>
#include <set>

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>

#include "connection.h"
#include "mordor/common/fiber.h"
#include "mordor/common/streams/stream.h"
#include "multipart.h"

class Scheduler;

namespace HTTP
{
    class ClientConnection;
    class ClientRequest : public boost::enable_shared_from_this<ClientRequest>, boost::noncopyable
    {
    private:
        friend class ClientConnection;
    public:
        typedef boost::shared_ptr<ClientRequest> ptr;

    private:
        ClientRequest(boost::shared_ptr<ClientConnection> conn, const Request &request);

    public:
        const Request &request();
        Stream::ptr requestStream();
        Multipart::ptr requestMultipart();
        EntityHeaders &requestTrailer();

        const Response &response();
        bool hasResponseBody();
        Stream::ptr responseStream();
        Multipart::ptr responseMultipart();
        const EntityHeaders &responseTrailer() const;

        Stream::ptr stream();

        void cancel(bool abort = false);
        void finish();
        void ensureResponse();

    private:
        void doRequest();
        void requestMultipartDone();
        void requestDone();
        void responseDone();

    private:
        boost::shared_ptr<ClientConnection> m_conn;
        Scheduler *m_scheduler;
        Fiber::ptr m_fiber;
        Request m_request;
        Response m_response;
        EntityHeaders m_requestTrailer, m_responseTrailer;
        bool m_requestDone, m_requestInFlight, m_responseHeadersDone, m_responseDone, m_responseInFlight, m_cancelled, m_aborted;
        bool m_badTrailer, m_incompleteTrailer;
        Stream::ptr m_requestStream, m_responseStream;
        Multipart::ptr m_requestMultipart, m_responseMultipart;
    };

    class ClientConnection : public Connection, public boost::enable_shared_from_this<ClientConnection>, boost::noncopyable
    {
    public:
        typedef boost::shared_ptr<ClientConnection> ptr;
    private:
        friend class ClientRequest;
    public:
        ClientConnection(Stream::ptr stream);

        ClientRequest::ptr request(const Request &requestHeaders);
    private:
        void scheduleNextRequest(ClientRequest::ptr currentRequest);
        void scheduleNextResponse(ClientRequest::ptr currentRequest);
        void scheduleAllWaitingRequests();
        void scheduleAllWaitingResponses();

    private:
        boost::mutex m_mutex;
        std::list<ClientRequest::ptr> m_pendingRequests;
        std::list<ClientRequest::ptr>::iterator m_currentRequest;
        std::set<ClientRequest::ptr> m_waitingResponses;
        bool m_allowNewRequests;
        bool m_priorRequestFailed, m_priorResponseFailed, m_priorResponseClosed;

        void invariant() const;
    };
};

#endif
