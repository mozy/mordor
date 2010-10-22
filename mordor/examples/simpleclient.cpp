// Copyright (c) 2009 - Decho Corporation

#include "mordor/predef.h"

#include <iostream>

#include "mordor/config.h"
#include "mordor/fiber.h"
#include "mordor/iomanager.h"
#include "mordor/main.h"
#include "mordor/socket.h"

using namespace Mordor;

MORDOR_MAIN(int argc, char *argv[])
{
    try {
        Config::loadFromEnvironment();
        IOManager ioManager;
        std::vector<Address::ptr> addresses =
            Address::lookup(argv[1], AF_UNSPEC, SOCK_STREAM);
        Socket::ptr s(addresses[0]->createSocket(ioManager));
        s->connect(addresses[0]);
        size_t rc = s->send("hello\r\n", 7);
        char buf[8192];
        rc = s->receive(buf, 8192);
        buf[rc] = 0;
        std::cout << "Read " << buf << " from conn" << std::endl;
        s->shutdown();
    } catch (...) {
        std::cerr << boost::current_exception_diagnostic_information() << std::endl;
    }
    return 0;
}
