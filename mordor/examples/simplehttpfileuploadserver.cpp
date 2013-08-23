#include "mordor/predef.h"

#include <iostream>

#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>

#include "mordor/config.h"
#include "mordor/http/server.h"
#include "mordor/iomanager.h"
#include "mordor/main.h"
#include "mordor/socket.h"
#include "mordor/streams/file.h"
#include "mordor/streams/socket.h"
#include "mordor/streams/transfer.h"
#include "mordor/streams/ssl.h"
#include "mordor/streams/null.h"

using namespace Mordor;


static volatile int counter = 0;

static void httpRequest(HTTP::ServerRequest::ptr request)
{	
	try {
		const std::string &method = request->request().requestLine.method;
		if (method == HTTP::POST) {
			request->response().entity.contentLength = 0;
			request->response().status.status = HTTP::OK;
			if (request->hasRequestBody()) {
				FileStream::ptr fileStream(new FileStream("/tmp/" + boost::lexical_cast<std::string>(__sync_fetch_and_add(&counter, 1)), FileStream::WRITE, FileStream::OPEN_OR_CREATE, static_cast<IOManager*>(Scheduler::getThis()), Scheduler::getThis()));
				transferStreamDirect(request->requestStream(), fileStream);
			}
		} else {
			respondError(request, HTTP::METHOD_NOT_ALLOWED);
		}

	} catch (...) {
        std::cerr << boost::current_exception_diagnostic_information() << std::endl;
    }
}

void serve(Socket::ptr listen, bool ssl)
{
	IOManager ioManager;
	SSL_CTX* ssl_ctx = NULL;

	if (ssl)
		ssl_ctx = SSLStream::createSSLCTX();

	while (true)
	{
		Socket::ptr socket = listen->accept(&ioManager);
		SocketStream::ptr socketStream(new SocketStream(socket));

		Stream::ptr stream;

		if (ssl)
		{
			SSLStream::ptr sslStream(new SSLStream(socketStream, false, true, ssl_ctx));
			sslStream->accept();
			stream = sslStream;
		}
		else
		{
			stream = socketStream;
		}

		HTTP::ServerConnection::ptr conn(new HTTP::ServerConnection(stream, &httpRequest));
		Scheduler::getThis()->schedule(boost::bind(&HTTP::ServerConnection::processRequests, conn));
	}
}

MORDOR_MAIN(int argc, char *argv[])
{
    try {
        Config::loadFromEnvironment();
        IOManager ioManager;

        Socket::ptr httpSocket = Socket::ptr(new Socket(ioManager, AF_INET, SOCK_STREAM));
        IPv4Address httpAddress(INADDR_ANY, 8080);
        httpSocket->bind(httpAddress);
        httpSocket->listen();

        boost::thread serveThread1(serve, httpSocket, false);
		boost::thread serveThread2(serve, httpSocket, false);
		boost::thread serveThread3(serve, httpSocket, false);
		boost::thread serveThread4(serve, httpSocket, false);
		serveThread1.join();
		serveThread2.join();
		serveThread3.join();
		serveThread4.join();

    } catch (...) {
        std::cerr << boost::current_exception_diagnostic_information() << std::endl;
    }
    return 0;
}
