#include "mordor/predef.h"

#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include "mordor/config.h"
#include "mordor/daemon.h"
#include "mordor/iomanager.h"
#include "mordor/main.h"
#include "mordor/socket.h"
#include "mordor/streams/socket.h"
#include "mordor/streams/ssl.h"

using namespace Mordor;


void sslTerminatorHandleIO(Stream::ptr from, Stream::ptr to)
{
	try
	{
		int size = 4096;
		Buffer buffer;
		buffer.reserve(size);

		while (true)
		{
			size_t len = from->read(buffer, size);
			if (!len)
				return;
			to->write (buffer, len);
		}
	} catch (...) {
		std::cerr << boost::current_exception_diagnostic_information() << std::endl;
	}
}

void sslTerminatorHandler(Stream::ptr client, Address::ptr address)
{
	Scheduler* scheduler = Scheduler::getThis();

	IOManager* ioManager = static_cast<IOManager*>(scheduler);
	Socket::ptr socket = address->createSocket(*ioManager, SOCK_STREAM);
	socket->connect(address);

	SocketStream::ptr socketStream(new SocketStream(socket));

	scheduler->schedule(boost::bind(&sslTerminatorHandleIO, client, socketStream));
	scheduler->schedule(boost::bind(&sslTerminatorHandleIO, socketStream, client));
}

void serve(Socket::ptr listen, Address::ptr address)
{
	IOManager ioManager;

	SSL_CTX* ssl_ctx = SSLStream::createSSLCTX();

    while (true) {
    	try
    	{
			Socket::ptr socket = listen->accept(&ioManager);
			SocketStream::ptr socketStream(new SocketStream(socket));

			SSLStream::ptr sslStream(new SSLStream(socketStream, false, true, ssl_ctx));
			sslStream->accept();

			Scheduler::getThis()->schedule(boost::bind(&sslTerminatorHandler, sslStream, address));
    	} catch (...) {
    		//TODO::for now we ignore from any exception that occurred during connection handling
    		std::cerr << boost::current_exception_diagnostic_information() << std::endl;
    	}
    }
}

int main(int argc, char *argv[])
{
	try {
		IOManager ioManager;

		std::vector<Address::ptr> addresses = Address::lookup("localhost:8443");
		Socket::ptr socket = addresses[0]->createSocket(ioManager, SOCK_STREAM);
		socket->bind(addresses[0]);
		socket->listen();

		std::vector<Address::ptr> backendAddresses = Address::lookup("localhost:8080");

    	boost::thread serveThread1(serve, socket, backendAddresses[0]);
    	boost::thread serveThread2(serve, socket, backendAddresses[0]);
    	boost::thread serveThread3(serve, socket, backendAddresses[0]);
    	boost::thread serveThread4(serve, socket, backendAddresses[0]);

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
