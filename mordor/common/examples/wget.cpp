// Copyright (c) 2009 - Decho Corp.

#include <iostream>

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>

#include "common/exception.h"
#include "common/http/basic.h"
#include "common/http/client.h"
#include "common/iomanager.h"
#include "common/socket.h"
#include "common/streams/socket.h"
#include "common/streams/ssl.h"
#include "common/streams/std.h"
#include "common/streams/transfer.h"

struct CredentialStore
{
    bool authorize(const URI &uri, const std::string &realm, bool proxy, std::string &u, std::string &p)
    {
        if (proxy && !hasProxy)
            return false;
        if (proxy) {
            u = proxyUsername;
            p = proxyPassword;
        } else {
            u = username;
            p = password;
        }
        return true;
    }
    std::string username;
    std::string password;
    std::string proxyUsername;
    std::string proxyPassword;
    bool hasProxy;
};

HTTP::ClientConnection::ptr establishConn(IOManager &ioManager, Address::ptr address, bool ssl)
{
    Socket::ptr s(address->createSocket(ioManager));
    s->connect(address);
    Stream::ptr stream(new SocketStream(s));
    if (ssl)
        stream.reset(new SSLStream(stream));

    HTTP::ClientConnection::ptr conn(new HTTP::ClientConnection(stream));
    return conn;
}

int main(int argc, const char *argv[])
{
    StdoutStream stdoutStream;
    Fiber::ptr mainfiber(new Fiber());
    IOManager ioManager;
    try {
        URI uri = argv[1];
        assert(uri.authority.hostDefined());
        assert(!uri.schemeDefined() || uri.scheme() == "http" || uri.scheme() == "https");

        CredentialStore store;

        std::string proxy;

        if (argc == 3 || argc == 5 || argc == 7) {
            proxy = argv[2];
            --argc;
            ++argv;
        }

        store.hasProxy = false;
        if (argc == 4 || argc == 6) {
            store.username = argv[2];
            store.password = argv[3];
        }
        if (argc == 6) {
            store.proxyUsername = argv[4];
            store.proxyPassword = argv[5];
            store.hasProxy = true;
        }

        std::vector<Address::ptr> addresses =
        Address::lookup(proxy.empty() ? uri.authority.host() : proxy, AF_UNSPEC, SOCK_STREAM);
        if (proxy.empty()) {
            IPAddress *addr = dynamic_cast<IPAddress *>(addresses[0].get());
            assert(addr);
            if (uri.authority.portDefined()) {
                addr->port(uri.authority.port());
            } else if (uri.schemeDefined() && uri.scheme() == "https") {
                addr->port(443);
            } else {
                addr->port(80);
            }
        }
        
        HTTP::ClientConnection::ptr conn = establishConn(ioManager, addresses[0], proxy.empty() && uri.schemeDefined() && uri.scheme() == "https");
        HTTP::Request requestHeaders;
        if (proxy.empty())
            requestHeaders.requestLine.uri.path = uri.path;
        else
            requestHeaders.requestLine.uri = uri;
        requestHeaders.request.host = uri.authority.host();
        HTTP::ClientRequest::ptr request = conn->request(requestHeaders);
        HTTP::BasicClientAuthenticationScheme basic(boost::bind(&CredentialStore::authorize, &store, _1, _2, _3, _4, _5));
        if (request->response().status.status == HTTP::PROXY_AUTHENTICATION_REQUIRED) {
            if (basic.authorize(request, requestHeaders, true)) {
                request->finish();
                try {
                    request = conn->request(requestHeaders);
                    request->ensureResponse();
                } catch (SocketException) {
                    conn = establishConn(ioManager, addresses[0], proxy.empty() && uri.schemeDefined() && uri.scheme() == "https");
                    request = conn->request(requestHeaders);
                } catch (HTTP::IncompleteMessageHeaderException) {
                    conn = establishConn(ioManager, addresses[0], proxy.empty() && uri.schemeDefined() && uri.scheme() == "https");
                    request = conn->request(requestHeaders);
                }
            }
        }
        if (request->response().status.status == HTTP::UNAUTHORIZED) {
            if (basic.authorize(request, requestHeaders, false)) {
                request->finish();
                request = conn->request(requestHeaders);
            }
        }
        if (request->hasResponseBody()) {
            try {
                if (request->response().entity.contentType.type != "multipart") {
                    transferStream(request->responseStream(), stdoutStream);
                } else {
                    Multipart::ptr responseMultipart = request->responseMultipart();
                    for (BodyPart::ptr bodyPart = responseMultipart->nextPart(); bodyPart;
                        bodyPart = responseMultipart->nextPart()) {
                        transferStream(bodyPart->stream(), stdoutStream);
                    }                        
                }
            } catch(...) {
                request->cancel();
                throw;
            }
        }
    } catch (std::exception& ex) {
        std::cerr << "Caught " << typeid(ex).name() << ": "
                  << ex.what( ) << std::endl;
    }
    ioManager.stop();
    return 0;
}
