// Copyright (c) 2009 - Mozy, Inc.

#include <boost/bind.hpp>

#include "mordor/http/broker.h"
#include "mordor/http/client.h"
#include "mordor/http/multipart.h"
#include "mordor/http/parser.h"
#include "mordor/http/server.h"
#include "mordor/streams/duplex.h"
#include "mordor/streams/limited.h"
#include "mordor/streams/null.h"
#include "mordor/streams/memory.h"
#include "mordor/streams/random.h"
#include "mordor/streams/transfer.h"
#include "mordor/test/test.h"
#include "mordor/util.h"
#include "mordor/workerpool.h"

using namespace Mordor;
using namespace Mordor::HTTP;
using namespace Mordor::Test;

namespace {
struct DummyException {};
}

static void
httpRequest(ServerRequest::ptr request)
{
    const std::string &method = request->request().requestLine.method;
    if (method == HTTP::GET || method == HTTP::HEAD || method == HTTP::PUT ||
        method == HTTP::POST) {
        if (request->request().requestLine.uri == "/exceptionReadingRequest") {
            MORDOR_ASSERT(request->hasRequestBody());
            MORDOR_THROW_EXCEPTION(DummyException());
        }
        request->response().entity.contentLength = request->request().entity.contentLength;
        request->response().entity.contentType = request->request().entity.contentType;
        request->response().general.transferEncoding = request->request().general.transferEncoding;
        request->response().status.status = OK;
        request->response().entity.extension = request->request().entity.extension;
        if (request->hasRequestBody()) {
            if (request->request().requestLine.method != HEAD) {
                if (request->request().entity.contentType.type == "multipart") {
                    Multipart::ptr requestMultipart = request->requestMultipart();
                    Multipart::ptr responseMultipart = request->responseMultipart();
                    for (BodyPart::ptr requestPart = requestMultipart->nextPart();
                        requestPart;
                        requestPart = requestMultipart->nextPart()) {
                        BodyPart::ptr responsePart = responseMultipart->nextPart();
                        responsePart->headers() = requestPart->headers();
                        transferStream(requestPart->stream(), responsePart->stream());
                        responsePart->stream()->close();
                    }
                    responseMultipart->finish();
                } else {
                    respondStream(request, request->requestStream());
                    return;
                }
            } else {
                request->finish();
            }
        } else {
            request->response().entity.contentLength = 0;
            request->finish();
        }
    } else {
        respondError(request, METHOD_NOT_ALLOWED);
    }
}

static void
doSingleRequest(const char *request, Response &response)
{
    Stream::ptr input(new MemoryStream(Buffer(request)));
    MemoryStream::ptr output(new MemoryStream());
    Stream::ptr stream(new DuplexStream(input, output));
    ServerConnection::ptr conn(new ServerConnection(stream, &httpRequest));
    WorkerPool pool;
    pool.schedule(boost::bind(&ServerConnection::processRequests, conn));
    pool.dispatch();
    ResponseParser parser(response);
    parser.run(output->buffer());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT(!parser.error());
}

MORDOR_UNITTEST(HTTPServer, badRequest)
{
    Response response;
    doSingleRequest("garbage", response);
    MORDOR_TEST_ASSERT_EQUAL(response.status.status, BAD_REQUEST);
}

MORDOR_UNITTEST(HTTPServer, close10)
{
    Response response;
    doSingleRequest(
        "GET / HTTP/1.0\r\n"
        "\r\n",
        response);
    MORDOR_TEST_ASSERT_EQUAL(response.status.ver, Version(1, 0));
    MORDOR_TEST_ASSERT_EQUAL(response.status.status, OK);
    MORDOR_TEST_ASSERT(response.general.connection.find("close") != response.general.connection.end());
}

MORDOR_UNITTEST(HTTPServer, keepAlive10)
{
    Response response;
    doSingleRequest(
        "GET / HTTP/1.0\r\n"
        "Connection: Keep-Alive\r\n"
        "\r\n",
        response);
    MORDOR_TEST_ASSERT_EQUAL(response.status.ver, Version(1, 0));
    MORDOR_TEST_ASSERT_EQUAL(response.status.status, OK);
    MORDOR_TEST_ASSERT(response.general.connection.find("Keep-Alive") != response.general.connection.end());
    MORDOR_TEST_ASSERT(response.general.connection.find("close") == response.general.connection.end());
}

MORDOR_UNITTEST(HTTPServer, noHost11)
{
    Response response;
    doSingleRequest(
        "GET / HTTP/1.1\r\n"
        "\r\n",
        response);
    MORDOR_TEST_ASSERT_EQUAL(response.status.ver, Version(1, 1));
    MORDOR_TEST_ASSERT_EQUAL(response.status.status, BAD_REQUEST);
}

MORDOR_UNITTEST(HTTPServer, close11)
{
    Response response;
    doSingleRequest(
        "GET / HTTP/1.1\r\n"
        "Host: garbage\r\n"
        "Connection: close\r\n"
        "\r\n",
        response);
    MORDOR_TEST_ASSERT_EQUAL(response.status.ver, Version(1, 1));
    MORDOR_TEST_ASSERT_EQUAL(response.status.status, OK);
    MORDOR_TEST_ASSERT(response.general.connection.find("close") != response.general.connection.end());
}

MORDOR_UNITTEST(HTTPServer, keepAlive11)
{
    Response response;
    doSingleRequest(
        "GET / HTTP/1.1\r\n"
        "Host: garbage\r\n"
        "\r\n",
        response);
    MORDOR_TEST_ASSERT_EQUAL(response.status.ver, Version(1, 1));
    MORDOR_TEST_ASSERT_EQUAL(response.status.status, OK);
    MORDOR_TEST_ASSERT(response.general.connection.find("close") == response.general.connection.end());
}

MORDOR_UNITTEST(HTTPServer, exceptionReadingRequest)
{
    Response response;
    doSingleRequest(
        "GET /exceptionReadingRequest HTTP/1.1\r\n"
        "Host: garbage\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "hello",
        response);
    MORDOR_TEST_ASSERT_EQUAL(response.status.ver, Version(1, 1));
    MORDOR_TEST_ASSERT_EQUAL(response.status.status, INTERNAL_SERVER_ERROR);
}

static void
disconnectDuringResponseServer(const URI &uri, ServerRequest::ptr request)
{
    Stream::ptr random(new RandomStream());
    random.reset(new LimitedStream(random, 4096));
    request->response().status.status = OK;
    request->response().general.transferEncoding.push_back("chunked");
    Stream::ptr response = request->responseStream();
    response->flush();
    // Yield so that the request can be cancelled while the response is in progress
    Scheduler::yield();
    transferStream(random, response);
    MORDOR_TEST_ASSERT_EXCEPTION(response->flush(), BrokenPipeException);
}

MORDOR_UNITTEST(HTTPServer, disconnectDuringResponse)
{
    WorkerPool pool;
    MockConnectionBroker server(&disconnectDuringResponseServer);
    BaseRequestBroker requestBroker(ConnectionBroker::ptr(&server, &nop<ConnectionBroker *>));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "http://localhost/";

    ClientRequest::ptr request = requestBroker.request(requestHeaders);
    request->doRequest();
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    // Don't read the response body
    request->cancel(true);
}

static void quickResponseServer(const URI &uri, ServerRequest::ptr request)
{
    respondError(request, OK, "hello");
}

MORDOR_UNITTEST(HTTPServer, responseCompletesBeforeRequest)
{
    WorkerPool pool;
    MockConnectionBroker server(&quickResponseServer);
    ClientConnection::ptr connection =
        server.getConnection("http://localhost/").first;

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/responseCompletesBeforeRequest";
    requestHeaders.entity.contentLength = 5;

    ClientRequest::ptr request = connection->request(requestHeaders);
    request->doRequest();
    Stream::ptr requestStream = request->requestStream();
    requestStream->flush();
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    transferStream(request->responseStream(), NullStream::get());
    requestStream->write("hello", 5);
    requestStream->close();
}
