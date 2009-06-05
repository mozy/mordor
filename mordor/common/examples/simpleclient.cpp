// Copyright (c) 2009 - Decho Corp.

#include <iostream>

#include "common/fiber.h"
#include "common/iomanager.h"
#include "common/socket.h"

int main(int argc, const char *argv[])
{
    Fiber::ptr mainfiber(new Fiber());
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
    ioManager.stop();
    return 0;
}
