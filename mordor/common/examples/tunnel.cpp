// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include <iostream>

#include <boost/bind.hpp>

#include "mordor/common/config.h"
#include "mordor/common/exception.h"
#include "mordor/common/http/auth.h"
#include "mordor/common/http/tunnel.h"
#include "mordor/common/iomanager.h"
#include "mordor/common/socket.h"
#include "mordor/common/streams/socket.h"
#include "mordor/common/streams/std.h"
#include "mordor/common/streams/transfer.h"

static
HTTP::ClientConnection::ptr establishConn(IOManager &ioManager, Address::ptr address)
{
    Socket::ptr s(address->createSocket(ioManager));
    s->connect(address);
    Stream::ptr stream(new SocketStream(s));
    HTTP::ClientConnection::ptr conn(new HTTP::ClientConnection(stream));
    return conn;
}

int main(int argc, const char *argv[])
{
    Config::loadFromEnvironment();
    Fiber::ptr mainfiber(new Fiber());
    IOManager ioManager;
    StdinStream stdinStream(ioManager);
    StdoutStream stdoutStream(ioManager);
    try {
        std::string proxy, target, proxyUsername, proxyPassword;
        proxy = argv[1];
        target = argv[2];

        if (argc == 5) {
            proxyUsername = argv[3];
            proxyPassword = argv[4];
        }

        std::vector<Address::ptr> addresses =
        Address::lookup(proxy, AF_UNSPEC, SOCK_STREAM);
        
        HTTP::ClientAuthBroker authBroker(boost::bind(&establishConn,
            boost::ref(ioManager), addresses[0]), "", "", proxyUsername,
            proxyPassword);

        Stream::ptr tunnelStream = HTTP::tunnel(authBroker, proxy, target);
        std::vector<boost::function<void ()> > dgs;
        dgs.push_back(boost::bind(
            (void (*)(Stream &, Stream::ptr, unsigned long long *, unsigned long long *))&transferStream,
            boost::ref(stdinStream), tunnelStream,
            (unsigned long long *)NULL, (unsigned long long *)NULL));
        dgs.push_back(boost::bind(
            (void (*)(Stream::ptr, Stream &, unsigned long long *, unsigned long long *))&transferStream,
            tunnelStream, boost::ref(stdoutStream),
            (unsigned long long *)NULL, (unsigned long long *)NULL));
        parallel_do(dgs);
    } catch (std::exception& ex) {
        std::cerr << "Caught " << typeid(ex).name() << ": "
                  << ex.what( ) << std::endl;
    }
    ioManager.stop();
    return 0;
}
