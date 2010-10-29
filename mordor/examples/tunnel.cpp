// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/predef.h"

#include <iostream>

#include "mordor/config.h"
#include "mordor/http/auth.h"
#include "mordor/http/client.h"
#include "mordor/http/proxy.h"
#include "mordor/iomanager.h"
#include "mordor/main.h"
#include "mordor/socket.h"
#include "mordor/streams/duplex.h"
#include "mordor/streams/socket.h"
#include "mordor/streams/ssl.h"
#include "mordor/streams/std.h"
#include "mordor/streams/transfer.h"

using namespace Mordor;

static void shuttleData(Stream::ptr oneEnd, Stream::ptr otherEnd)
{
    try {
        transferStream(oneEnd, otherEnd);
        if (otherEnd->supportsHalfClose())
            otherEnd->close(Stream::WRITE);
    } catch (std::exception &) {
        if (oneEnd->supportsHalfClose())
            oneEnd->close(Stream::READ);
    }
}

static void connectThem(Stream::ptr oneEnd, Stream::ptr otherEnd)
{
    Scheduler *scheduler = Scheduler::getThis();
    scheduler->schedule(boost::bind(&shuttleData, oneEnd, otherEnd));
    scheduler->schedule(boost::bind(&shuttleData, otherEnd, oneEnd));
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

static bool getCredentials(HTTP::ClientRequest::ptr priorRequest,
    std::string &scheme, std::string &username, std::string &password,
    const std::string &user, const std::string &pass,
    size_t attempts)
{
    if (!priorRequest)
        return false;
    if (attempts > 1)
        return false;
    username = user;
    password = pass;
    const HTTP::ChallengeList &challengeList =
        priorRequest->response().response.proxyAuthenticate;
#ifdef WINDOWS
    if (HTTP::isAcceptable(challengeList, "Negotiate")) {
        scheme = "Negotiate";
        return true;
    }
    if (HTTP::isAcceptable(challengeList, "NTLM")) {
        scheme = "NTLM";
        return true;
    }
#endif
    if (HTTP::isAcceptable(challengeList, "Digest")) {
        scheme = "Digest";
        return true;
    }
    if (HTTP::isAcceptable(challengeList, "Basic")) {
        scheme = "Basic";
        return true;
    }
    return false;
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

        HTTP::RequestBrokerOptions options;
        options.ioManager = &ioManager;
        options.getProxyCredentialsDg = boost::bind(&getCredentials, _2, _3,
            _5, _6, username, password, _7);
        HTTP::RequestBroker::ptr requestBroker = HTTP::createRequestBroker(options).first;

        Stream::ptr server = HTTP::tunnel(requestBroker, proxy, toConnectTo);
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
            Scheduler::getThis()->schedule(boost::bind(outgoing, sockstream));
        }
    } catch (std::exception &ex) {
        std::cerr << typeid(ex).name() << ": " << ex.what( ) << std::endl;
    }
}

MORDOR_MAIN(int argc, char *argv[])
{
    Config::loadFromEnvironment();
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
                Scheduler::getThis()->schedule(boost::bind(&socketServer, s,
                    boost::ref(ioManager), outgoing));
            }
            ioManager.yieldTo();
        }
    } catch (...) {
        std::cerr << boost::current_exception_diagnostic_information()
            << std::endl;
        return 1;
    }
    return 0;
}
