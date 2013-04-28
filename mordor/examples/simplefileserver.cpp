// Copyright (c) 2010 - Mozy, Inc.

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

using namespace Mordor;

static void httpRequest(HTTP::ServerRequest::ptr request)
{
	try {
		const std::string &method = request->request().requestLine.method;
		const URI &uri = request->request().requestLine.uri;

		if (method == HTTP::GET) {
			FileStream::ptr stream(new FileStream(uri.path.toString(), FileStream::READ, FileStream::OPEN, static_cast<IOManager*>(Scheduler::getThis()), Scheduler::getThis()));
			HTTP::respondStream(request, stream);
		} else {
			HTTP::respondError(request, HTTP::METHOD_NOT_ALLOWED);
		}
	} catch (...) {
        std::cerr << boost::current_exception_diagnostic_information() << std::endl;
        HTTP::respondError(request, HTTP::NOT_FOUND);
    }
}

void serve(Socket::ptr listen)
{
	IOManager ioManager;

	while (true)
	{
		Socket::ptr socket = listen->accept(&ioManager);
		Stream::ptr stream(new SocketStream(socket));
		HTTP::ServerConnection::ptr conn(new HTTP::ServerConnection(stream,
			&httpRequest));
		conn->processRequests();
	}
}

MORDOR_MAIN(int argc, char *argv[])
{
    try {
        Config::loadFromEnvironment();
        IOManager ioManager;
        Socket::ptr socket = Socket::ptr(new Socket(ioManager, AF_INET, SOCK_STREAM));
        IPv4Address address(INADDR_ANY, 8080);

        socket->bind(address);
        socket->listen();

        boost::thread serveThread1(serve, socket);
		boost::thread serveThread2(serve, socket);
		serveThread1.join();
		serveThread2.join();

    } catch (...) {
        std::cerr << boost::current_exception_diagnostic_information() << std::endl;
    }
    return 0;
}
