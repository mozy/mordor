#include "mordor/predef.h"

#include <iostream>

#include <boost/thread.hpp>

#include "mordor/config.h"
#include "mordor/http/server.h"
#include "mordor/iomanager.h"
#include "mordor/main.h"
#include "mordor/socket.h"
#include "mordor/streams/file.h"
#include "mordor/streams/socket.h"
#include "mordor/streams/transfer.h"
#include "mordor/streams/ssl.h"

using namespace Mordor;

static void httpRequest(HTTP::ServerRequest::ptr request)
{
	const std::string &method = request->request().requestLine.method;
	const URI &uri = request->request().requestLine.uri;

	if (method == HTTP::GET) {
		FileStream::ptr stream(new FileStream(uri.path.toString(), FileStream::READ, FileStream::OPEN, static_cast<IOManager*>(Scheduler::getThis()), Scheduler::getThis()));
		HTTP::respondStream(request, stream);
	} else {
		HTTP::respondError(request, HTTP::METHOD_NOT_ALLOWED);
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

        Socket::ptr httpsSocket = Socket::ptr(new Socket(ioManager, AF_INET, SOCK_STREAM));
        IPv4Address httpsAddress(INADDR_ANY, 8443);
        httpsSocket->bind(httpsAddress);
        httpsSocket->listen();

        boost::thread serveThread1(serve, httpSocket, false);
		boost::thread serveThread2(serve, httpSocket, false);
		boost::thread serveThread3(serve, httpsSocket, true);
		boost::thread serveThread4(serve, httpsSocket, true);
		serveThread1.join();
		serveThread2.join();
		serveThread3.join();
		serveThread4.join();

    } catch (...) {
        std::cerr << boost::current_exception_diagnostic_information() << std::endl;
    }
    return 0;
}
