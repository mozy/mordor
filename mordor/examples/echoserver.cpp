// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/predef.h"

#include <boost/bind.hpp>

#include "mordor/config.h"
#include "mordor/http/multipart.h"
#include "mordor/http/server.h"
#include "mordor/iomanager.h"
#include "mordor/main.h"
#include "mordor/socket.h"
#ifdef WINDOWS
#include "mordor/streams/namedpipe.h"
#endif
#include "mordor/streams/socket.h"
#include "mordor/streams/transfer.h"

using namespace Mordor;

void streamConnection(Stream::ptr stream)
{
    try {
        transferStream(stream, stream);
    } catch (UnexpectedEofException &)
    {}
    stream->close();
}

void socketServer(Socket::ptr listen)
{
    listen->listen();

    while (true) {
        Socket::ptr socket = listen->accept();
        Stream::ptr stream(new SocketStream(socket));
        Scheduler::getThis()->schedule(boost::bind(&streamConnection, stream));
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
        Scheduler::getThis()->schedule(boost::bind(&socketServer, s));
    }

#ifndef WINDOWS
    UnixAddress echoaddress("/tmp/echo", SOCK_STREAM);
    Socket::ptr s = echoaddress.createSocket(ioManager);
    s->bind(echoaddress);
    Scheduler::getThis()->schedule(boost::bind(&socketServer, s));
#endif
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
                    if (request->request().entity.contentType.type == "multipart") {
                        Multipart::ptr requestMultipart = request->requestMultipart();
                        Multipart::ptr responseMultipart = request->responseMultipart();
                        for (BodyPart::ptr requestPart = requestMultipart->nextPart();
                            requestPart;
                            requestPart = requestMultipart->nextPart()) {
                            BodyPart::ptr responsePart = responseMultipart->nextPart();
                            responsePart->headers() = requestPart->headers();
                            transferStream(requestPart->stream(), responsePart->stream());
                            responsePart->stream()->close();
                        }
                        responseMultipart->finish();
                    } else {
                        respondStream(request, request->requestStream());
                        return;
                    }
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

void httpServer(Socket::ptr listen)
{
    listen->listen();

    while (true) {
        Socket::ptr socket = listen->accept();
        Stream::ptr stream(new SocketStream(socket));
        HTTP::ServerConnection::ptr conn(new HTTP::ServerConnection(stream, &httpRequest));
        Scheduler::getThis()->schedule(boost::bind(&HTTP::ServerConnection::processRequests, conn));
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
        Scheduler::getThis()->schedule(boost::bind(&httpServer, s));
    }
}

#ifdef WINDOWS
void namedPipeServer(IOManager &ioManager)
{
    while (true) {
        NamedPipeStream::ptr stream(new NamedPipeStream("\\\\.\\pipe\\echo", NamedPipeStream::READWRITE, &ioManager));
        stream->accept();
        Scheduler::getThis()->schedule(boost::bind(&streamConnection, stream));
    }
}
#endif

MORDOR_MAIN(int argc, char *argv[])
{
    try {
        Config::loadFromEnvironment();
        IOManager ioManager;
        startSocketServer(ioManager);
        startHttpServer(ioManager);
#ifdef WINDOWS
        ioManager.schedule(boost::bind(&namedPipeServer, boost::ref(ioManager)));
        ioManager.schedule(boost::bind(&namedPipeServer, boost::ref(ioManager)));
#endif
        ioManager.dispatch();
    } catch (...) {
        std::cerr << boost::current_exception_diagnostic_information() << std::endl;
    }
    return 0;
}
