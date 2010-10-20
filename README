Mordor

What is it?

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

Where should it be used?

Any software (server-side or client-side) that need to process a lot of data.
It is C++, so is probably overkill for something that could be easily handled
with a Python or Ruby script, but can be used for simpler tasks because it does
provide some nice abstractions that you won't see elsewhere.  Server
applications handling lots of connections will benefit most from the Fiber
engine, by transforming an event-based paradigm into a familiar thread-based
paradigm, while keeping (and in some cases improving) the performance of an
event-based paradigm.

How does it change the game?

Mordor allows you to focus on performing a logical task, instead of deciding how
to make that task conform to a specific threading/event model.  Just because
local disk I/O will block, and should be performed in a thread pool, and network
I/O should be performed using an event based callback design, doesn't mean you
can't do them both _in the same function_.  Mordor allows you to do just that.
For example, here's a complete program to read a file from disk, and send it to
a socket on the network:

#include <iostream>

#include <mordor/socket.h>
#include <mordor/streams/file.h>
#include <mordor/streams/socket.h>
#include <mordor/streams/transfer.h>

using namespace Mordor;

int main(int argc, char **argv)
{
    if (argc != 3) {
        std::cerr << "usage: " << argv[0] << " <file> <destination>" << std::endl;
        return 1;
    }
    try {
        std::vector<Address::ptr> addresses = Address::lookup(argv[2], AF_UNSPEC, SOCK_STREAM);
        Socket::ptr socket = addresses[0]->createSocket();
        socket->connect(addresses[0]);
        Stream::ptr fileStream(new FileStream(argv[1], FileStream::OPEN, FileStream::READ));
        Stream::ptr socketStream(new SocketStream(socket));
        transferStream(fileStream, socketStream);
    } catch (...) {
        std::cerr << boost::current_exception_diagnostic_information() << std::endl;
        return 2;
    }
    return 0;
}

This program is quite simple.  It checks for usage, translates the string
argument into a network address, creates a socket that is compatible with that
address, connects to it, opens a file (as a stream), wraps the socket in a
stream, and then sends the file over the socket.  If an error occurs, complete
error information is printed on stdout, including the type of error, the OS
level error code and description (if applicable), and a complete stacktrace of
the error, including debug symbol information, if available.  Looking at it, we
can see that there is only a single thread.  Which is all fine and dandy if
this is all we're doing.  But what if instead we were sending 1000 files to
1000 different sockets, but didn't want to create a thread for each one?  Let's
say we want one thread for communicating with the network, and four threads for
reading the file off the disk.  Let's do it!

#include <iostream>

#include <mordor/iomanager.h>
#include <mordor/scheduler.h>
#include <mordor/socket.h>
#include <mordor/streams/file.h>
#include <mordor/streams/socket.h>
#include <mordor/streams/transfer.h>

using namespace Mordor;

static void doOne(const char *file, const char *destination, IOManager &ioManager, Scheduler &scheduler)
{
    try {
        std::vector<Address::ptr> addresses = Address::lookup(destination, AF_UNSPEC, SOCK_STREAM);
        Socket::ptr socket = addresses[0]->createSocket(ioManager);
        socket->connect(addresses[0]);
        Stream::ptr fileStream(new FileStream(file, FileStream::READ, FileStream::OPEN, &ioManager, &scheduler));
        Stream::ptr socketStream(new SocketStream(socket));
        transferStream(fileStream, socketStream);
    } catch (...) {
        std::cerr << boost::current_exception_diagnostic_information() << std::endl;
    }
}

int main(int argc, char **argv)
{
    if (argc % 2 != 1) {
        std::cerr << "usage: " << argv[0] << " (<file> <destination>)*" << std::endl;
        return 1;
    }
    IOManager ioManager;
    WorkerPool workerPool(4, false);
    for (int i = 1; i < argc; i += 2)
        ioManager.schedule(boost::bind(&doOne, argv[i], argv[i + 1], boost::ref(ioManager), boost::ref(workerPool)));
    ioManager.dispatch();

    return 0;
}

So we re-factored most of main into doOne, but other than that it is nearly
identical.  And it will transfer as many files as you pass on the command line
in parallel.  Using 5 threads.  The IOManager is the object used for
non-blocking network I/O, and so is passed to the Socket when it is created.
WorkerPool is just a generic thread pool, and is passed to the FileStream so
that it will automatically do its work on those threads, instead of the thread
it is running on when it is called.  Something to point out here is that when
the work is scheduled on the IOManager, each bit of work implicitly creates a
Fiber - a lightweight, cooperatively scheduled, user-mode thread.  The doOne
function is executed on its own Fiber, and is allowed to switch which thread it
is running on (inside of FileStream), without having to do any callbacks,
virtual functions on a defined interface, or anything else.  Internally, when
FileStream wants to execute on the thread pool, it suspends the current Fiber,
allowing other Fibers to run on this thread, and is resumed on a thread in the
WorkerPool.  IOManager and WorkerPool both inherit from Scheduler, which is the
base functionality for cooperatively scheduling Fibers.  Pretty cool, eh?


Dependencies

boost 1.40
OpenSSL
Zlib
Ragel (compile-time only)


Compiling for iPhone SDK

The iPhone SDK does not include OpenSSL headers or binaries. Since mordor relies
on OpenSSL, you must provide these files yourself. The Xcode project file is
configured to look for files in an iphone directory in the same directory as the
project file. 

Specifically, the compiler will look for headers in iphone/include/ and 
libraries in iphone/lib/. We recommend you create a symbolic link called 
"iphone" which points to the actual directory containing the include/ and lib/ 
directories.


License

Mordor is licensed under the New BSD License, and Copyright (c) 2009, Decho Corp.
See LICENSE for details.

Authors

Cody Cutrer (cody@mozy.com)
Patrick Bozeman (peb@mozy.com)
Jeremy Stanley (jeremy@mozy.com)
Zach Wily (zach@mozy.com)
Brian Palmer (brian@mozy.com)
