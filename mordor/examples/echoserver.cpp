// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/predef.h"

#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include "mordor/config.h"
#include "mordor/daemon.h"
#include "mordor/iomanager.h"
#include "mordor/main.h"
#include "mordor/socket.h"
#include "mordor/streams/socket.h"
#include "mordor/streams/transfer.h"
#include "mordor/streams/ssl.h"


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
	IOManager ioManager;

    while (true) {
        Socket::ptr socket = listen->accept(&ioManager);
        Stream::ptr stream(new SocketStream(socket));
        Scheduler::getThis()->schedule(boost::bind(&streamConnection, stream));
    }
}

void startSocketServer(IOManager &ioManager)
{
    std::vector<Address::ptr> addresses = Address::lookup("localhost:8000");

    for (std::vector<Address::ptr>::const_iterator it(addresses.begin());
        it != addresses.end();
        ++it) {
        Socket::ptr s = (*it)->createSocket(ioManager, SOCK_STREAM);
        s->bind(*it);
        Scheduler::getThis()->schedule(boost::bind(&socketServer, s));
    }

    UnixAddress echoaddress("/tmp/echo");
    Socket::ptr s = echoaddress.createSocket(ioManager, SOCK_STREAM);
    s->bind(echoaddress);
    Scheduler::getThis()->schedule(boost::bind(&socketServer, s));
}

int main(int argc, char *argv[])
{
	try {
		IOManager ioManager;

		std::vector<Address::ptr> addresses = Address::lookup("localhost:8080");
		Socket::ptr socket = addresses[0]->createSocket(ioManager, SOCK_STREAM);
		socket->bind(addresses[0]);
		socket->listen();

    	boost::thread serveThread1(socketServer, socket);
    	boost::thread serveThread2(socketServer, socket);
    	boost::thread serveThread3(socketServer, socket);
    	boost::thread serveThread4(socketServer, socket);

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
