// Copyright (c) 2009 - Decho Corp.

#include <boost/bind.hpp>

#include "common/http/server.h"
#include "common/iomanager.h"
#include "common/socket.h"
#include "common/streams/socket.h"
#include "common/streams/transfer.h"

void socketConnection(Socket::ptr s)
{
    unsigned char buf[4096];
    while (true) {
        size_t rc = s->receive(buf, 4096);
        if (rc == 0) {
            s->shutdown();
            s->close();
            break;
        }
        s->send(buf, rc);
    }
}

void socketServer(Socket::ptr s)
{
    s->listen(10);

    while (true) {
        Socket::ptr newsocket = s->accept();
        Scheduler::getThis()->schedule(Fiber::ptr(new Fiber(boost::bind(&socketConnection, newsocket), 8192)));
    }
}

void startSocketServer(IOManager &ioManager)
{
    std::vector<Address::ptr> addresses = Address::lookup("localhost:8000", AF_UNSPEC, SOCK_STREAM);

    for (std::vector<Address::ptr>::const_iterator it(addresses.begin());
        it != addresses.end();
        ++it) {
        Socket::ptr s = (*it)->createSocket(ioManager);
        s->bind(*it);
        Scheduler::getThis()->schedule(Fiber::ptr(new Fiber(boost::bind(&socketServer, s))));
    }
}

void httpRequest(HTTP::ServerRequest::ptr request)
{
    switch (request->request().requestLine.method) {
        case HTTP::GET:
        case HTTP::HEAD:
        case HTTP::PUT:
        case HTTP::POST:
            request->response().entity.contentLength = request->request().entity.contentLength;
            request->response().entity.contentType = request->request().entity.contentType;
            request->response().general.transferEncoding = request->request().general.transferEncoding;
            request->response().status.status = HTTP::OK;
            request->response().entity.extension = request->request().entity.extension;
            if (request->hasRequestBody()) {
                if (request->request().requestLine.method != HTTP::HEAD) {
                    transferStream(request->requestStream(), request->responseStream());
                    request->responseStream()->close();
                } else {
                    request->finish();
                }
            } else {
                request->response().entity.contentLength = 0;
                request->finish();
            }
            break;
        default:
            respondError(request, HTTP::METHOD_NOT_ALLOWED);
            break;
    }
}

void httpServer(Socket::ptr s)
{
    s->listen(10);

    while (true) {
        Socket::ptr newsocket = s->accept();
        Stream::ptr stream(new SocketStream(newsocket));
        HTTP::ServerConnection::ptr conn(new HTTP::ServerConnection(stream, &httpRequest));
        Scheduler::getThis()->schedule(Fiber::ptr(new Fiber(boost::bind(&HTTP::ServerConnection::processRequests, conn))));
    }
}

void startHttpServer(IOManager &ioManager)
{
    std::vector<Address::ptr> addresses = Address::lookup("localhost:80", AF_UNSPEC, SOCK_STREAM);

    for (std::vector<Address::ptr>::const_iterator it(addresses.begin());
        it != addresses.end();
        ++it) {
        Socket::ptr s = (*it)->createSocket(ioManager);
        s->bind(*it);
        Scheduler::getThis()->schedule(Fiber::ptr(new Fiber(boost::bind(&httpServer, s))));
    }    
}

int main(int argc, const char *argv[])
{
    Fiber::ptr mainfiber(new Fiber());
    IOManager ioManager;
    startSocketServer(ioManager);
    startHttpServer(ioManager);
    ioManager.yieldTo();
    return 0;
}
