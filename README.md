# Mordor

## What is it?

Mordor is a high performance I/O library.  It is cross-platform, compiling
on Windows, Linux, and Mac (32-bit and 64-bit on all platforms).  It includes
several main areas:

* Cooperatively scheduled fiber engine, including synchronization primitives
* Streams library, for dealing with streams of data and manipulating them.
* HTTP library, building on top of Fibers and Streams, to provide a simple to
  use, yet extremely powerful HTTP client and server API.
* Supporting infrastructure, including logging, configuration, statistics
  gathering, and exceptions.
* A unit test framework that is lightweight, easy to use, but has several useful
  features.

One of the main goals of Mordor is to provide very easy to use abstractions and
encapsulation of difficult and complex concepts, yet still provide near absolute
power in weilding them if necessary.

## Where should it be used?

Any software (server-side or client-side) that need to process a lot of data.
It is C++, so is probably overkill for something that could be easily handled
with a Python or Ruby script, but can be used for simpler tasks because it does
provide some nice abstractions that you won't see elsewhere.  Server
applications handling lots of connections will benefit most from the Fiber
engine, by transforming an event-based paradigm into a familiar thread-based
paradigm, while keeping (and in some cases improving) the performance of an
event-based paradigm.

## How does it change the game?

Mordor allows you to focus on performing a logical task, instead of deciding how
to make that task conform to a specific threading/event model.  Just because
local disk I/O will block, and should be performed in a thread pool, and network
I/O should be performed using an event based callback design, doesn't mean you
can't do them both _in the same function_.  Mordor allows you to do just that.
For example, here's a complete program for simple http file server:


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
		    HTTP::ServerConnection::ptr conn(new HTTP::ServerConnection(stream, &httpRequest));
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

The IOManager is the object used for non-blocking network I/O, and so is passed to the Socket when it is created. The FileStream need a scheduler object to schedule the non-blocking file I/O, so in that case it use the scheduler of network I/O, so the scheduling of incoming connections/processing and read from file system play in the same scheduler without any callbacks!
Something to point out here is that when the work is scheduled on the IOManager, each bit of work implicitly creates a Fiber - a lightweight, cooperatively scheduled, user-mode thread.


## Dependencies

* boost 1.49
* OpenSSL 1.0.1c
* Zlib
* Ragel (compile-time only)

## Building
    ~/mordor/buildtools/build.sh

for debug:

    ~/mordor/buildtools/build.sh debug
## License

Mordor is licensed under the New BSD License, and Copyright (c) 2009, Decho Corp.
See LICENSE for details.

## Authors

Uri Shamay (shamayuri@gmail.com)