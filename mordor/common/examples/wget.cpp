// Copyright (c) 2009 - Decho Corp.

#include <iostream>

#include <boost/shared_ptr.hpp>

#include "common/http/client.h"
#include "common/iomanager.h"
#include "common/socket.h"
#include "common/streams/socket.h"
#include "common/streams/std.h"
#include "common/streams/transfer.h"


void main(int argc, const char *argv[])
{
    StdoutStream stdoutStream;
    Fiber::ptr mainfiber(new Fiber());
    IOManager ioManager;
    try {
        URI uri = argv[1];
        assert(uri.authority.hostDefined());
        assert(!uri.schemeDefined() || uri.scheme() == "http" || uri.scheme() == "https");

        std::vector<boost::shared_ptr<Address> > addresses =
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
        std::auto_ptr<Socket> s(addresses[0]->createSocket(&ioManager));
        s->connect(addresses[0].get());
        SocketStream *stream = new SocketStream(s.get());
        s.release();

        HTTP::ClientConnection conn(stream);
        HTTP::Request requestHeaders;
        requestHeaders.requestLine.uri.path = uri.path;
        requestHeaders.general.connection.insert("close");
        requestHeaders.request.host = uri.authority.host();
        HTTP::ClientRequest::ptr request = conn.request(requestHeaders);
        std::auto_ptr<Stream> responseStream(request->responseStream());
        transferStream(responseStream.get(), &stdoutStream);
    } catch (std::exception& ex) {
        std::cerr << "Caught " << typeid(ex).name( ) << ": "
                  << ex.what( ) << std::endl;
    }
}
