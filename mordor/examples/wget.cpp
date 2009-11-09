// Copyright (c) 2009 - Decho Corp.

#include "mordor/predef.h"

#include <iostream>

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>

#include "mordor/config.h"
#include "mordor/exception.h"
#include "mordor/http/auth.h"
#include "mordor/http/broker.h"
#include "mordor/http/client.h"
#include "mordor/iomanager.h"
#include "mordor/socket.h"
#include "mordor/streams/socket.h"
#include "mordor/streams/ssl.h"
#include "mordor/streams/std.h"
#include "mordor/streams/transfer.h"

using namespace Mordor;

int main(int argc, const char *argv[])
{
    Config::loadFromEnvironment();
    StdoutStream stdoutStream;
    IOManager ioManager;
    try {
        URI uri = argv[1];
        MORDOR_ASSERT(uri.authority.hostDefined());
        MORDOR_ASSERT(!uri.schemeDefined() || uri.scheme() == "http" || uri.scheme() == "https");

        HTTP::StreamBroker::ptr streamBroker(new HTTP::SocketStreamBroker(&ioManager));
        HTTP::ConnectionBroker::ptr connectionBroker(new HTTP::ConnectionCache(streamBroker));
        HTTP::RequestBroker::ptr requestBroker(new HTTP::BaseRequestBroker(connectionBroker));
        requestBroker.reset(new HTTP::RedirectRequestBroker(requestBroker));

        HTTP::Request requestHeaders;
        requestHeaders.requestLine.uri = uri;
        requestHeaders.request.host = uri.authority.host();
        HTTP::ClientRequest::ptr request = requestBroker->request(requestHeaders);
        if (request->hasResponseBody()) {
            if (request->response().entity.contentType.type != "multipart") {
                transferStream(request->responseStream(), stdoutStream);
            } else {
                Multipart::ptr responseMultipart = request->responseMultipart();
                for (BodyPart::ptr bodyPart = responseMultipart->nextPart(); bodyPart;
                    bodyPart = responseMultipart->nextPart()) {
                    transferStream(bodyPart->stream(), stdoutStream);
                }
            }
        }
    } catch (...) {
        std::cerr << boost::current_exception_diagnostic_information() << std::endl;
        return 1;
    }
    return 0;
}
