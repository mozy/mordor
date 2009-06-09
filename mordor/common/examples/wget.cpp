// Copyright (c) 2009 - Decho Corp.

#include <iostream>

#include <boost/shared_ptr.hpp>

#include "common/http/client.h"
#include "common/iomanager.h"
#include "common/socket.h"
#include "common/streams/socket.h"
#include "common/streams/ssl.h"
#include "common/streams/std.h"
#include "common/streams/transfer.h"

int main(int argc, const char *argv[])
{
    StdoutStream stdoutStream;
    Fiber::ptr mainfiber(new Fiber());
    IOManager ioManager;
    try {
        URI uri = argv[1];
        assert(uri.authority.hostDefined());
        assert(!uri.schemeDefined() || uri.scheme() == "http" || uri.scheme() == "https");

        std::vector<Address::ptr> addresses =
        Address::lookup(uri.authority.host(), AF_UNSPEC, SOCK_STREAM);
        IPAddress *addr = dynamic_cast<IPAddress *>(addresses[0].get());
        assert(addr);
        if (uri.authority.portDefined()) {
            addr->port(uri.authority.port());
        } else if (uri.schemeDefined() && uri.scheme() == "https") {
            addr->port(443);
        } else {
            addr->port(80);
        }
        Socket::ptr s(addresses[0]->createSocket(ioManager));
        s->connect(addresses[0]);
        Stream::ptr stream(new SocketStream(s));
        if (uri.schemeDefined() && uri.scheme() == "https")
            stream.reset(new SSLStream(stream));

        HTTP::ClientConnection::ptr conn(new HTTP::ClientConnection(stream));
        HTTP::Request requestHeaders;
        requestHeaders.requestLine.uri.path = uri.path;
        requestHeaders.general.connection.insert("close");
        requestHeaders.request.host = uri.authority.host();
        HTTP::ClientRequest::ptr request = conn->request(requestHeaders);
        Stream::ptr responseStream = request->responseStream();
        try {
           transferStream(request->responseStream(), stdoutStream);
        } catch(...) {
            request->cancel();
            throw;
        }
    } catch (std::exception& ex) {
        std::cerr << "Caught " << typeid(ex).name() << ": "
                  << ex.what( ) << std::endl;
    }
    ioManager.stop();
    return 0;
}
