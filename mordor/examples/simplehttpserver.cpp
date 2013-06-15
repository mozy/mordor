#include "mordor/predef.h"

#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include "mordor/config.h"
#include "mordor/daemon.h"
#include "mordor/http/multipart.h"
#include "mordor/http/server.h"
#include "mordor/iomanager.h"
#include "mordor/main.h"
#include "mordor/socket.h"
#include "mordor/streams/socket.h"
#include "mordor/streams/transfer.h"

using namespace Mordor;


void httpRequest(HTTP::ServerRequest::ptr request)
{
    const std::string &method = request->request().requestLine.method;
    if (method == HTTP::GET || method == HTTP::HEAD || method == HTTP::PUT ||
        method == HTTP::POST) {
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
    } else {
        respondError(request, HTTP::METHOD_NOT_ALLOWED);
    }
}

void httpServer(Socket::ptr listen)
{
	IOManager ioManager;

    while (true)
    {
        Socket::ptr socket = listen->accept(&ioManager);
        Stream::ptr stream(new SocketStream(socket));
        HTTP::ServerConnection::ptr conn(new HTTP::ServerConnection(stream, &httpRequest));
        Scheduler::getThis()->schedule(boost::bind(&HTTP::ServerConnection::processRequests, conn));
    }
}

int main(int argc, char *argv[])
{
	try {
		IOManager ioManager;

		std::vector<Address::ptr> addresses = Address::lookup("localhost:80");
		Socket::ptr socket = addresses[0]->createSocket(ioManager, SOCK_STREAM);
		socket->bind(addresses[0]);
		socket->listen();

    	boost::thread serveThread1(httpServer, socket);
    	boost::thread serveThread2(httpServer, socket);
    	boost::thread serveThread3(httpServer, socket);
    	boost::thread serveThread4(httpServer, socket);

    	serveThread1.join();
    	serveThread2.join();
    	serveThread3.join();
    	serveThread4.join();

    } catch (...) {
        std::cerr << boost::current_exception_diagnostic_information() << std::endl;
        return 1;
    }
    return 0;
}
