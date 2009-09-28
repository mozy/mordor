// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include <iostream>

#include "mordor/common/config.h"
#include "mordor/common/http/auth.h"
#include "mordor/common/http/tunnel.h"
#include "mordor/common/iomanager.h"
#include "mordor/common/socket.h"
#include "mordor/common/streams/duplex.h"
#include "mordor/common/streams/socket.h"
#include "mordor/common/streams/ssl.h"
#include "mordor/common/streams/std.h"
#include "mordor/common/streams/transfer.h"

static void shuttleData(Stream::ptr oneEnd, Stream::ptr otherEnd)
{
    try {
        transferStream(oneEnd, otherEnd);
        otherEnd->close(Stream::WRITE);
    } catch (std::exception &) {
        oneEnd->close(Stream::READ);
    }
}

static void connectThem(Stream::ptr oneEnd, Stream::ptr otherEnd)
{
    Scheduler *scheduler = Scheduler::getThis();
    scheduler->schedule(Fiber::ptr(new Fiber(boost::bind(&shuttleData,
        oneEnd, otherEnd))));
    scheduler->schedule(Fiber::ptr(new Fiber(boost::bind(&shuttleData,
        otherEnd, oneEnd))));
}

static void outgoingConnection(Stream::ptr client, IOManager &ioManager,
                               const std::string &toConnectTo, bool ssl)
{
    try {
        std::vector<Address::ptr> addresses =
            Address::lookup(toConnectTo, AF_UNSPEC, SOCK_STREAM);
        Socket::ptr sock;
        for (std::vector<Address::ptr>::const_iterator it(addresses.begin());
            it != addresses.end();
            ) {
            sock = (*it)->createSocket(ioManager);
            try {
                sock->connect(*it);
                break;
            } catch (std::exception &) {
                if (++it == addresses.end())
                    throw;
                sock.reset();
            }
        }
        Stream::ptr server(new SocketStream(sock));
        if (ssl) {
            SSLStream::ptr serverSSL(new SSLStream(server));
            serverSSL->connect();
            server = serverSSL;
        }
        connectThem(client, server);        
    } catch (std::exception &ex) {
        std::cerr << typeid(ex).name() << ": " << ex.what( ) << std::endl;
    }
}

static
HTTP::ClientConnection::ptr establishConn(IOManager &ioManager, const std::string &to)
{
    std::vector<Address::ptr> addresses =
        Address::lookup(to, AF_UNSPEC, SOCK_STREAM);
    Socket::ptr sock;
    for (std::vector<Address::ptr>::const_iterator it(addresses.begin());
        it != addresses.end();
        ) {
        sock = (*it)->createSocket(ioManager);
        try {
            sock->connect(*it);
            break;
        } catch (std::exception &) {
            if (++it == addresses.end())
                throw;
            sock.reset();
        }
    }
    Stream::ptr stream(new SocketStream(sock));
    HTTP::ClientConnection::ptr conn(new HTTP::ClientConnection(stream));
    return conn;
}

static void outgoingProxyConnection(Stream::ptr client, IOManager &ioManager,
                                    const std::string &proxy,
                                    const std::string &username,
                                    const std::string &password,
                                    const std::string &toConnectTo, bool ssl)
{
    try {
        std::vector<Address::ptr> addresses =
        Address::lookup(proxy, AF_UNSPEC, SOCK_STREAM);
        
        HTTP::ClientAuthBroker authBroker(boost::bind(&establishConn,
            boost::ref(ioManager), proxy), "", "", username,
            password);

        Stream::ptr server = HTTP::tunnel(authBroker, proxy, toConnectTo);
        if (ssl) {
            SSLStream::ptr serverSSL(new SSLStream(server));
            serverSSL->connect();
            server = serverSSL;
        }
        connectThem(client, server);
    } catch (std::exception &ex) {
        std::cerr << typeid(ex).name() << ": " << ex.what( ) << std::endl;
    }
}

static void socketServer(Socket::ptr s, IOManager &ioManager,
                         boost::function<void (Stream::ptr)> outgoing)
{
    s->listen(10);

    try {
        while (true) {
            Socket::ptr newsocket = s->accept();
            Stream::ptr sockstream(new SocketStream(newsocket));
            Scheduler::getThis()->schedule(Fiber::ptr(new Fiber(boost::bind(
                outgoing, sockstream), 65536)));
        }
    } catch (std::exception &ex) {
        std::cerr << typeid(ex).name() << ": " << ex.what( ) << std::endl;
    }
}

int main(int argc, const char *argv[])
{
    Config::loadFromEnvironment();
    Fiber::ptr mainfiber(new Fiber());
    IOManager ioManager;
    if (argc <= 2) {
        std::cerr << "Usage: (<listen> | -) <to> [-ssl] [<proxy> [<username> <password>]]"
            << std::endl;
        return 2;
    }
    try {
        std::string from, to, username, password;
        boost::function<void (Stream::ptr)> outgoing;
        bool ssl = false;
        from = argv[1];
        to = argv[2];
        
        if (argc > 3 && strcmp(argv[3], "-ssl") == 0) {
            ssl = true;
            --argc;
            ++argv;
        }
        if (argc > 5)
            password = argv[5];
        if (argc > 4)
            username = argv[4];
        if (argc > 3) {
            outgoing = boost::bind(&outgoingProxyConnection, _1,
                boost::ref(ioManager), argv[3], username, password, to,
                ssl);
        } else {
            outgoing = boost::bind(&outgoingConnection, _1,
                boost::ref(ioManager), to, ssl);
        }
        
        if (to == "-") {
            Stream::ptr stdIn(new StdinStream());
            Stream::ptr stdOut(new StdoutStream());
            Stream::ptr std(new DuplexStream(stdIn, stdOut));
            outgoing(std);
        } else {
            std::vector<Address::ptr> addresses =
                Address::lookup(from, AF_UNSPEC, SOCK_STREAM);

            for (std::vector<Address::ptr>::const_iterator it(addresses.begin());
                it != addresses.end();
                ++it) {
                Socket::ptr s = (*it)->createSocket(ioManager);
                s->bind(*it);
                Scheduler::getThis()->schedule(Fiber::ptr(new Fiber(boost::bind(
                    &socketServer, s, boost::ref(ioManager), outgoing))));
            }
            ioManager.yieldTo();
        }
    } catch (std::exception &ex) {
        std::cerr << typeid(ex).name() << ": " << ex.what( ) << std::endl;
        return 1;
    }
    return 0;
}
