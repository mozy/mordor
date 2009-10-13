// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include <iostream>

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>

#include "mordor/config.h"
#include "mordor/exception.h"
#include "mordor/http/auth.h"
#include "mordor/http/client.h"
#include "mordor/iomanager.h"
#include "mordor/socket.h"
#include "mordor/streams/socket.h"
#include "mordor/streams/ssl.h"
#include "mordor/streams/std.h"
#include "mordor/streams/transfer.h"

using namespace Mordor;

HTTP::ClientConnection::ptr establishConn(IOManager &ioManager, Address::ptr address,
                                          const std::string &host, bool ssl)
{
    Socket::ptr s(address->createSocket(ioManager));
    s->connect(address);
    Stream::ptr stream(new SocketStream(s));
    if (ssl) {
        SSLStream::ptr sslStream(new SSLStream(stream));
        sslStream->connect();
        try {
            try {
                sslStream->verifyPeerCertificate();
            } catch (CertificateVerificationException &ex) {
                if (ex.verifyResult() != X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY)
                    throw;
            }
            sslStream->verifyPeerCertificate(host);
        } catch (CertificateVerificationException &ex) {
            std::cerr << "Warning: " << ex.what() << std::endl;
        }
        stream = sslStream;
    }

    HTTP::ClientConnection::ptr conn(new HTTP::ClientConnection(stream));
    return conn;
}

int main(int argc, const char *argv[])
{
    Config::loadFromEnvironment();
    StdoutStream stdoutStream;
    Fiber::ptr mainfiber(new Fiber());
    IOManager ioManager;
    try {
        URI uri = argv[1];
        MORDOR_ASSERT(uri.authority.hostDefined());
        MORDOR_ASSERT(!uri.schemeDefined() || uri.scheme() == "http" || uri.scheme() == "https");

        std::string username, password, proxy, proxyUsername, proxyPassword;

        if (argc == 3 || argc == 5 || argc == 7) {
            proxy = argv[2];
            --argc;
            ++argv;
        }

        if (argc == 4 || argc == 6) {
            username = argv[2];
            password = argv[3];
        }
        if (argc == 6) {
            proxyUsername = argv[4];
            proxyPassword = argv[5];
        }

        std::vector<Address::ptr> addresses =
        Address::lookup(proxy.empty() ? uri.authority.host() : proxy, AF_UNSPEC, SOCK_STREAM);
        if (proxy.empty()) {
            IPAddress *addr = dynamic_cast<IPAddress *>(addresses[0].get());
            MORDOR_ASSERT(addr);
            if (uri.authority.portDefined()) {
                addr->port(uri.authority.port());
            } else if (uri.schemeDefined() && uri.scheme() == "https") {
                addr->port(443);
            } else {
                addr->port(80);
            }
        }
        
        HTTP::ClientAuthBroker authBroker(boost::bind(&establishConn,
            boost::ref(ioManager), addresses[0], uri.authority.host(),
            proxy.empty() && uri.schemeDefined() && uri.scheme() == "https"),
            username, password, proxyUsername, proxyPassword);

        HTTP::Request requestHeaders;
        if (proxy.empty())
            requestHeaders.requestLine.uri.path = uri.path;
        else
            requestHeaders.requestLine.uri = uri;
        requestHeaders.request.host = uri.authority.host();
        if (!proxy.empty()) {
            requestHeaders.general.connection.insert("Proxy-Connection");
            requestHeaders.entity.extension["Proxy-Connection"] = "Keep-Alive";
        }
        HTTP::ClientRequest::ptr request = authBroker.request(requestHeaders);
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
