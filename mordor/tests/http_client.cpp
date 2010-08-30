// Copyright (c) 2009 - Mozy, Inc.

#include <boost/bind.hpp>

#include "mordor/fiber.h"
#include "mordor/http/broker.h"
#include "mordor/http/client.h"
#include "mordor/http/multipart.h"
#include "mordor/http/parser.h"
#include "mordor/http/server.h"
#include "mordor/iomanager.h"
#include "mordor/scheduler.h"
#include "mordor/sleep.h"
#include "mordor/streams/buffered.h"
#include "mordor/streams/cat.h"
#include "mordor/streams/duplex.h"
#include "mordor/streams/limited.h"
#include "mordor/streams/memory.h"
#include "mordor/streams/notify.h"
#include "mordor/streams/null.h"
#include "mordor/streams/pipe.h"
#include "mordor/streams/random.h"
#include "mordor/streams/test.h"
#include "mordor/streams/transfer.h"
#include "mordor/test/test.h"
#include "mordor/util.h"
#include "mordor/workerpool.h"

using namespace Mordor;
using namespace Mordor::HTTP;
using namespace Mordor::Test;

MORDOR_UNITTEST(HTTPClient, emptyRequest)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.general.connection.insert("close");

    ClientRequest::ptr request = conn->request(requestHeaders);
    request->doRequest();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "GET / HTTP/1.0\r\n"
        "Connection: close\r\n"
        "\r\n");
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);

    // No more requests possible, because we used Connection: close
    MORDOR_TEST_ASSERT_EXCEPTION(conn->request(requestHeaders),
        ConnectionVoluntarilyClosedException);
}

MORDOR_UNITTEST(HTTPClient, pipelinedSynchronousRequests)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "\r\n"
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "\r\n")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "garbage";

    ClientRequest::ptr request1 = conn->request(requestHeaders);
    request1->doRequest();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "GET / HTTP/1.1\r\n"
        "Host: garbage\r\n"
        "\r\n");
    MORDOR_TEST_ASSERT_EQUAL(responseStream->tell(), 0);

    requestHeaders.general.connection.insert("close");
    ClientRequest::ptr request2 = conn->request(requestHeaders);
    request2->doRequest();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "GET / HTTP/1.1\r\n"
        "Host: garbage\r\n"
        "\r\n"
        "GET / HTTP/1.1\r\n"
        "Connection: close\r\n"
        "Host: garbage\r\n"
        "\r\n");
    MORDOR_TEST_ASSERT_EQUAL(responseStream->tell(), 0);

    // No more requests possible, even pipelined ones, because we used
    // Connection: close
    MORDOR_TEST_ASSERT_EXCEPTION(conn->request(requestHeaders),
        ConnectionVoluntarilyClosedException);

    MORDOR_TEST_ASSERT_EQUAL(request1->response().status.status, OK);
    // Can't test for if half of the stream has been consumed here, because it
    // will be in a buffer somewhere
    MORDOR_TEST_ASSERT_EQUAL(request2->response().status.status, OK);
    MORDOR_TEST_ASSERT_EQUAL(responseStream->tell(), responseStream->size());
}

MORDOR_UNITTEST(HTTPClient, emptyResponseBody)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.general.connection.insert("close");

    ClientRequest::ptr request = conn->request(requestHeaders);
    request->doRequest();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "GET / HTTP/1.0\r\n"
        "Connection: close\r\n"
        "\r\n");
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    // Verify response characteristics
    MORDOR_TEST_ASSERT(request->hasResponseBody());
    Stream::ptr response = request->responseStream();
    MORDOR_TEST_ASSERT(response->supportsRead());
    MORDOR_TEST_ASSERT(response->supportsSize());
    MORDOR_TEST_ASSERT(!response->supportsTruncate());
    MORDOR_TEST_ASSERT(!response->supportsFind());
    MORDOR_TEST_ASSERT(!response->supportsUnread());
    MORDOR_TEST_ASSERT_EQUAL(response->size(), 0);

    // Verify response itself
    MemoryStream responseBody;
    transferStream(response, responseBody);
    MORDOR_TEST_ASSERT(responseBody.buffer() == "");

    response.reset();
#ifdef DEBUG
    MORDOR_TEST_ASSERT_ASSERTED(request->responseStream());
#endif
}

MORDOR_UNITTEST(HTTPClient, incompleteResponseHeaders)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Leng")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.general.connection.insert("close");

    ClientRequest::ptr request = conn->request(requestHeaders);
    request->doRequest();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "GET / HTTP/1.0\r\n"
        "Connection: close\r\n"
        "\r\n");
    MORDOR_TEST_ASSERT_EXCEPTION(request->response(), IncompleteMessageHeaderException);
    MORDOR_TEST_ASSERT_EXCEPTION(request->response(), IncompleteMessageHeaderException);
    MORDOR_TEST_ASSERT_EXCEPTION(request->response(), IncompleteMessageHeaderException);
}

MORDOR_UNITTEST(HTTPClient, simpleResponseBody)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 5\r\n"
        "Connection: close\r\n"
        "\r\n"
        "hello")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.general.connection.insert("close");

    ClientRequest::ptr request = conn->request(requestHeaders);
    request->doRequest();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "GET / HTTP/1.0\r\n"
        "Connection: close\r\n"
        "\r\n");
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    // Verify response characteristics
    MORDOR_TEST_ASSERT(request->hasResponseBody());
    Stream::ptr response = request->responseStream();
    MORDOR_TEST_ASSERT(response->supportsRead());
    MORDOR_TEST_ASSERT(!response->supportsWrite());
    MORDOR_TEST_ASSERT(!response->supportsSeek());
    MORDOR_TEST_ASSERT(response->supportsSize());
    MORDOR_TEST_ASSERT(!response->supportsTruncate());
    MORDOR_TEST_ASSERT(!response->supportsFind());
    MORDOR_TEST_ASSERT(!response->supportsUnread());
    MORDOR_TEST_ASSERT_EQUAL(response->size(), 5);

    // Verify response itself
    MemoryStream responseBody;
    transferStream(response, responseBody);
    MORDOR_TEST_ASSERT(responseBody.buffer() == "hello");

    response.reset();
#ifdef DEBUG
    MORDOR_TEST_ASSERT_ASSERTED(request->responseStream());
#endif
}

MORDOR_UNITTEST(HTTPClient, incompleteResponseBody)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 5\r\n"
        "Connection: close\r\n"
        "\r\n"
        "hel")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.general.connection.insert("close");

    ClientRequest::ptr request = conn->request(requestHeaders);
    request->doRequest();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "GET / HTTP/1.0\r\n"
        "Connection: close\r\n"
        "\r\n");
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    // Verify response characteristics
    MORDOR_TEST_ASSERT(request->hasResponseBody());
    Stream::ptr response = request->responseStream();
    MORDOR_TEST_ASSERT(response->supportsRead());
    MORDOR_TEST_ASSERT(!response->supportsWrite());
    MORDOR_TEST_ASSERT(!response->supportsSeek());
    MORDOR_TEST_ASSERT(response->supportsSize());
    MORDOR_TEST_ASSERT(!response->supportsTruncate());
    MORDOR_TEST_ASSERT(!response->supportsFind());
    MORDOR_TEST_ASSERT(!response->supportsUnread());
    MORDOR_TEST_ASSERT_EQUAL(response->size(), 5);

    // Verify response itself
    MemoryStream responseBody;
    MORDOR_TEST_ASSERT_EXCEPTION(transferStream(response, responseBody),
        UnexpectedEofException);
}

MORDOR_UNITTEST(HTTPClient, readPastEof)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 5\r\n"
        "Connection: close\r\n"
        "\r\n"
        "hello")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.general.connection.insert("close");

    ClientRequest::ptr request = conn->request(requestHeaders);
    request->doRequest();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "GET / HTTP/1.0\r\n"
        "Connection: close\r\n"
        "\r\n");
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    // Verify response characteristics
    MORDOR_TEST_ASSERT(request->hasResponseBody());

    // Verify response itself
    MemoryStream responseBody;
    Stream::ptr response = request->responseStream();
    transferStream(response, responseBody);
    MORDOR_TEST_ASSERT(responseBody.buffer() == "hello");
    Buffer buf;
    // Read EOF a few times just to be sure nothing asplodes
    MORDOR_TEST_ASSERT_EQUAL(response->read(buf, 10), 0u);
    MORDOR_TEST_ASSERT_EQUAL(response->read(buf, 10), 0u);
    MORDOR_TEST_ASSERT_EQUAL(response->read(buf, 10), 0u);
}

MORDOR_UNITTEST(HTTPClient, chunkedResponseBody)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Connection: close\r\n"
        "\r\n"
        "5\r\n"
        "hello"
        "\r\n"
        "0\r\n"
        "\r\n")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "garbage";

    ClientRequest::ptr request = conn->request(requestHeaders);
    request->doRequest();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "GET / HTTP/1.1\r\n"
        "Host: garbage\r\n"
        "\r\n");
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    // Verify response characteristics
    MORDOR_TEST_ASSERT(request->hasResponseBody());
    Stream::ptr response = request->responseStream();
    MORDOR_TEST_ASSERT(response->supportsRead());
    MORDOR_TEST_ASSERT(!response->supportsWrite());
    MORDOR_TEST_ASSERT(!response->supportsSeek());
    MORDOR_TEST_ASSERT(!response->supportsSize());
    MORDOR_TEST_ASSERT(!response->supportsTruncate());
    MORDOR_TEST_ASSERT(!response->supportsFind());
    MORDOR_TEST_ASSERT(!response->supportsUnread());

    // Verify response itself
    MemoryStream responseBody;
    transferStream(response, responseBody);
    MORDOR_TEST_ASSERT(responseBody.buffer() == "hello");
}

MORDOR_UNITTEST(HTTPClient, trailerResponse)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Connection: close\r\n"
        "\r\n"
        "0\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "garbage";

    ClientRequest::ptr request = conn->request(requestHeaders);
    request->doRequest();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "GET / HTTP/1.1\r\n"
        "Host: garbage\r\n"
        "\r\n");
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    // Verify response characteristics
    MORDOR_TEST_ASSERT(request->hasResponseBody());
    Stream::ptr response = request->responseStream();
    MORDOR_TEST_ASSERT(response->supportsRead());
    MORDOR_TEST_ASSERT(!response->supportsWrite());
    MORDOR_TEST_ASSERT(!response->supportsSeek());
    MORDOR_TEST_ASSERT(!response->supportsSize());
    MORDOR_TEST_ASSERT(!response->supportsTruncate());
    MORDOR_TEST_ASSERT(!response->supportsFind());
    MORDOR_TEST_ASSERT(!response->supportsUnread());

    // Verify response itself
    MemoryStream responseBody;
    transferStream(response, responseBody);
    MORDOR_TEST_ASSERT(responseBody.buffer() == "");

    // Trailer!
    MORDOR_TEST_ASSERT_EQUAL(request->responseTrailer().contentType.type, "text");
    MORDOR_TEST_ASSERT_EQUAL(request->responseTrailer().contentType.subtype, "plain");
}

MORDOR_UNITTEST(HTTPClient, simpleRequestBody)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.method = PUT;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.general.connection.insert("close");
    requestHeaders.entity.contentLength = 5;

    ClientRequest::ptr request = conn->request(requestHeaders);
    // Nothing has been flushed yet
    MORDOR_TEST_ASSERT_EQUAL(requestStream->size(), 0);
    Stream::ptr requestBody = request->requestStream();
    // Verify stream characteristics
    MORDOR_TEST_ASSERT(!requestBody->supportsRead());
    MORDOR_TEST_ASSERT(requestBody->supportsWrite());
    MORDOR_TEST_ASSERT(!requestBody->supportsSeek());
    MORDOR_TEST_ASSERT(requestBody->supportsSize());
    MORDOR_TEST_ASSERT(!requestBody->supportsTruncate());
    MORDOR_TEST_ASSERT(!requestBody->supportsFind());
    MORDOR_TEST_ASSERT(!requestBody->supportsUnread());
    MORDOR_TEST_ASSERT_EQUAL(requestBody->size(), 5);

    // Force a flush (of the headers)
    requestBody->flush();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "PUT / HTTP/1.0\r\n"
        "Connection: close\r\n"
        "Content-Length: 5\r\n"
        "\r\n");

    // Write the body
    MORDOR_TEST_ASSERT_EQUAL(requestBody->write("hello"), 5u);
    requestBody->close();
    MORDOR_TEST_ASSERT_EQUAL(requestBody->size(), 5);

    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "PUT / HTTP/1.0\r\n"
        "Connection: close\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "hello");

    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
}

namespace {
struct DummyException {};
}

static void throwDummyException()
{
    MORDOR_THROW_EXCEPTION(DummyException());
}

static void throwSocketException()
{
    MORDOR_THROW_EXCEPTION(ConnectionResetException());
}

MORDOR_UNITTEST(HTTPClient, simpleRequestBodyExceptionInStream)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n")));
    TestStream::ptr testStream(new TestStream(requestStream));
    testStream->onWrite(&throwSocketException, 200);
    testStream->onClose(boost::bind(&throwDummyException));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, testStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.method = PUT;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.general.connection.insert("close");
    requestHeaders.entity.contentLength = 500000;

    ClientRequest::ptr request = conn->request(requestHeaders);
    // Nothing has been flushed yet
    MORDOR_TEST_ASSERT_EQUAL(requestStream->size(), 0);
    Stream::ptr requestBody = request->requestStream();
    // Verify stream characteristics
    MORDOR_TEST_ASSERT(!requestBody->supportsRead());
    MORDOR_TEST_ASSERT(requestBody->supportsWrite());
    MORDOR_TEST_ASSERT(!requestBody->supportsSeek());
    MORDOR_TEST_ASSERT(requestBody->supportsSize());
    MORDOR_TEST_ASSERT(!requestBody->supportsTruncate());
    MORDOR_TEST_ASSERT(!requestBody->supportsFind());
    MORDOR_TEST_ASSERT(!requestBody->supportsUnread());
    MORDOR_TEST_ASSERT_EQUAL(requestBody->size(), 500000);

    // Force a flush (of the headers)
    requestBody->flush();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "PUT / HTTP/1.0\r\n"
        "Connection: close\r\n"
        "Content-Length: 500000\r\n"
        "\r\n");

    // Write the body
    RandomStream randomStream;
    MORDOR_TEST_ASSERT_EXCEPTION(transferStream(randomStream, requestBody, 500000), ConnectionResetException);
}

MORDOR_UNITTEST(HTTPClient, multipleCloseRequestBody)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.method = PUT;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.general.connection.insert("close");
    requestHeaders.entity.contentLength = 5;

    ClientRequest::ptr request = conn->request(requestHeaders);
    // Nothing has been flushed yet
    MORDOR_TEST_ASSERT_EQUAL(requestStream->size(), 0);
    Stream::ptr requestBody = request->requestStream();

    // Write the body
    MORDOR_TEST_ASSERT_EQUAL(requestBody->write("hello"), 5u);
    // Do it multiple times, to make sure nothing asplodes
    requestBody->close();
    requestBody->close();
    requestBody->close();
}

MORDOR_UNITTEST(HTTPClient, underflowRequestBody)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.method = PUT;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.general.connection.insert("close");
    requestHeaders.entity.contentLength = 5;

    ClientRequest::ptr request = conn->request(requestHeaders);
    // Nothing has been flushed yet
    MORDOR_TEST_ASSERT_EQUAL(requestStream->size(), 0);
    Stream::ptr requestBody = request->requestStream();

    // Write the body
    MORDOR_TEST_ASSERT_EQUAL(requestBody->write("hel"), 3u);
#ifdef DEBUG
    MORDOR_TEST_ASSERT_ASSERTED(requestBody->close());
#endif
}

MORDOR_UNITTEST(HTTPClient, overflowRequestBody)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.method = PUT;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.general.connection.insert("close");
    requestHeaders.entity.contentLength = 5;

    ClientRequest::ptr request = conn->request(requestHeaders);
    // Nothing has been flushed yet
    MORDOR_TEST_ASSERT_EQUAL(requestStream->size(), 0);
    Stream::ptr requestBody = request->requestStream();

    // Write the body
    MORDOR_TEST_ASSERT_EQUAL(requestBody->write("helloworld"), 5u);
    MORDOR_TEST_ASSERT_EXCEPTION(requestBody->write("hello", 5), WriteBeyondEofException);
}

MORDOR_UNITTEST(HTTPClient, chunkedRequestBody)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.method = PUT;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.general.connection.insert("close");
    requestHeaders.general.transferEncoding.push_back(ValueWithParameters("chunked"));

    ClientRequest::ptr request = conn->request(requestHeaders);
    // Nothing has been flushed yet
    MORDOR_TEST_ASSERT_EQUAL(requestStream->size(), 0);
    Stream::ptr requestBody = request->requestStream();
    // Verify stream characteristics
    MORDOR_TEST_ASSERT(!requestBody->supportsRead());
    MORDOR_TEST_ASSERT(requestBody->supportsWrite());
    MORDOR_TEST_ASSERT(!requestBody->supportsSeek());
    MORDOR_TEST_ASSERT(!requestBody->supportsSize());
    MORDOR_TEST_ASSERT(!requestBody->supportsTruncate());
    MORDOR_TEST_ASSERT(!requestBody->supportsFind());
    MORDOR_TEST_ASSERT(!requestBody->supportsUnread());

    // Force a flush (of the headers)
    requestBody->flush();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "PUT / HTTP/1.0\r\n"
        "Connection: close\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n");

    // Write the body
    MORDOR_TEST_ASSERT_EQUAL(requestBody->write("hello"), 5u);
    MORDOR_TEST_ASSERT_EQUAL(requestBody->write("world"), 5u);
    requestBody->close();

    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "PUT / HTTP/1.0\r\n"
        "Connection: close\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\n"
        "hello"
        "\r\n"
        "5\r\n"
        "world"
        "\r\n"
        "0\r\n"
        // No trailers
        "\r\n");

    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
}

MORDOR_UNITTEST(HTTPClient, simpleRequestPartialWrites)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    TestStream::ptr testStream(new TestStream(duplexStream));
    testStream->maxReadSize(10);
    testStream->maxWriteSize(10);
    ClientConnection::ptr conn(new ClientConnection(testStream));

    Request requestHeaders;
    requestHeaders.requestLine.method = GET;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.general.connection.insert("close");

    ClientRequest::ptr request = conn->request(requestHeaders);
    request->doRequest();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "GET / HTTP/1.0\r\n"
        "Connection: close\r\n"
        "\r\n");

    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
}

static void pipelinedRequests(ClientConnection::ptr conn,
    int &sequence)
{
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 2);

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "garbage";

    ClientRequest::ptr request2 = conn->request(requestHeaders);
    request2->doRequest();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 4);

    MORDOR_TEST_ASSERT_EQUAL(request2->response().status.status, NOT_FOUND);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 6);
}

MORDOR_UNITTEST(HTTPClient, pipelinedRequests)
{
    WorkerPool pool;
    int sequence = 1;

    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "\r\n"
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Length: 0\r\n"
        "\r\n")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.method = PUT;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "garbage";
    requestHeaders.entity.contentLength = 7;

    ClientRequest::ptr request1 = conn->request(requestHeaders);

    // Start the second request, which will yield to us when it can't use the conn
    Fiber::ptr request2Fiber(new Fiber(boost::bind(&pipelinedRequests,
        conn, boost::ref(sequence))));
    pool.schedule(request2Fiber);
    pool.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 3);

    MORDOR_TEST_ASSERT_EQUAL(request1->requestStream()->write("hello\r\n"), 7u);
    request1->requestStream()->close();

    // Nothing has been sent to the server yet (it's buffered up), because
    // there is a pipelined request waiting, and we only flush after
    // the last request
    MORDOR_TEST_ASSERT_EQUAL(requestStream->size(), 0);

    pool.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 5);

    // Both requests have been sent now (flush()es after last request)
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "PUT / HTTP/1.1\r\n"
        "Host: garbage\r\n"
        "Content-Length: 7\r\n"
        "\r\n"
        "hello\r\n"
        "GET / HTTP/1.1\r\n"
        "Host: garbage\r\n"
        "\r\n");

    // Nothing has been read yet
    MORDOR_TEST_ASSERT_EQUAL(responseStream->tell(), 0);

    MORDOR_TEST_ASSERT_EQUAL(request1->response().status.status, OK);
    pool.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 7);

    // Both responses have been read now
    MORDOR_TEST_ASSERT_EQUAL(responseStream->tell(), responseStream->size());
}

MORDOR_UNITTEST(HTTPClient, pipelinedEmptyRequests)
{
    WorkerPool pool;
    int sequence = 1;

    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "\r\n"
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Length: 0\r\n"
        "\r\n")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.method = PUT;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "garbage";
    requestHeaders.entity.contentLength = 0;

    // Start the second request, which will yield to us when it can't use the conn
    pool.schedule(boost::bind(&pipelinedRequests,
        conn, boost::ref(sequence)));

    ClientRequest::ptr request1 = conn->request(requestHeaders);
    request1->doRequest();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 3);

    // Nothing has been sent to the server yet (it's buffered up)
    MORDOR_TEST_ASSERT_EQUAL(requestStream->size(), 0);
    pool.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 5);

    // Both requests have been sent now (flush()es after last request)
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "PUT / HTTP/1.1\r\n"
        "Host: garbage\r\n"
        "Content-Length: 0\r\n"
        "\r\n"
        "GET / HTTP/1.1\r\n"
        "Host: garbage\r\n"
        "\r\n");

    // Nothing has been read yet
    MORDOR_TEST_ASSERT_EQUAL(responseStream->tell(), 0);

    MORDOR_TEST_ASSERT_EQUAL(request1->response().status.status, OK);
    pool.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 7);

    // Both responses have been read now
    MORDOR_TEST_ASSERT_EQUAL(responseStream->tell(), responseStream->size());
}

MORDOR_UNITTEST(HTTPClient, missingTrailerResponse)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Connection: close\r\n"
        "\r\n"
        "0\r\n")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "garbage";

    ClientRequest::ptr request = conn->request(requestHeaders);
    request->doRequest();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "GET / HTTP/1.1\r\n"
        "Host: garbage\r\n"
        "\r\n");
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    // Verify response characteristics
    MORDOR_TEST_ASSERT(request->hasResponseBody());
    Stream::ptr response = request->responseStream();
    MORDOR_TEST_ASSERT(response->supportsRead());
    MORDOR_TEST_ASSERT(!response->supportsWrite());
    MORDOR_TEST_ASSERT(!response->supportsSeek());
    MORDOR_TEST_ASSERT(!response->supportsSize());
    MORDOR_TEST_ASSERT(!response->supportsTruncate());
    MORDOR_TEST_ASSERT(!response->supportsFind());
    MORDOR_TEST_ASSERT(!response->supportsUnread());

    // Trailer hasn't been read yet
#ifdef DEBUG
    MORDOR_TEST_ASSERT_ASSERTED(request->responseTrailer());
#endif

    // Verify response itself
    MemoryStream responseBody;
    transferStream(response, responseBody);
    MORDOR_TEST_ASSERT(responseBody.buffer() == "");

    // Missing trailer
    MORDOR_TEST_ASSERT_EXCEPTION(request->responseTrailer(), IncompleteMessageHeaderException);
}

MORDOR_UNITTEST(HTTPClient, badTrailerResponse)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Connection: close\r\n"
        "\r\n"
        "0\r\n"
        "Content-Type-garbage\r\n"
        "\r\n")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "garbage";

    ClientRequest::ptr request = conn->request(requestHeaders);
    request->doRequest();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "GET / HTTP/1.1\r\n"
        "Host: garbage\r\n"
        "\r\n");
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    // Verify response characteristics
    MORDOR_TEST_ASSERT(request->hasResponseBody());
    Stream::ptr response = request->responseStream();
    MORDOR_TEST_ASSERT(response->supportsRead());
    MORDOR_TEST_ASSERT(!response->supportsWrite());
    MORDOR_TEST_ASSERT(!response->supportsSeek());
    MORDOR_TEST_ASSERT(!response->supportsSize());
    MORDOR_TEST_ASSERT(!response->supportsTruncate());
    MORDOR_TEST_ASSERT(!response->supportsFind());
    MORDOR_TEST_ASSERT(!response->supportsUnread());

    // Trailer hasn't been read yet
#ifdef DEBUG
    MORDOR_TEST_ASSERT_ASSERTED(request->responseTrailer());
#endif

    // Verify response itself
    MemoryStream responseBody;
    transferStream(response, responseBody);
    MORDOR_TEST_ASSERT(responseBody.buffer() == "");

    // Missing trailer
    MORDOR_TEST_ASSERT_EXCEPTION(request->responseTrailer(), BadMessageHeaderException);
}

MORDOR_UNITTEST(HTTPClient, cancelRequestSingle)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.method = PUT;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "garbage";
    requestHeaders.entity.contentLength = 5;

    ClientRequest::ptr request = conn->request(requestHeaders);
    request->doRequest();
    // Nothing has been flushed yet
    MORDOR_TEST_ASSERT_EQUAL(requestStream->size(), 0);
    request->cancel();

    MORDOR_TEST_ASSERT_EXCEPTION(conn->request(requestHeaders), PriorRequestFailedException);
}

MORDOR_UNITTEST(HTTPClient, cancelResponseSingle)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "hello")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "garbage";

    ClientRequest::ptr request1 = conn->request(requestHeaders);
    request1->doRequest();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "GET / HTTP/1.1\r\n"
        "Host: garbage\r\n"
        "\r\n");
    MORDOR_TEST_ASSERT_EQUAL(request1->response().status.status, OK);
    // Verify response characteristics
    MORDOR_TEST_ASSERT(request1->hasResponseBody());

    ClientRequest::ptr request2 = conn->request(requestHeaders);

    request1->cancel(true);

    MORDOR_TEST_ASSERT_EXCEPTION(request2->ensureResponse(), PriorRequestFailedException);
    MORDOR_TEST_ASSERT_EXCEPTION(request2->ensureResponse(), PriorRequestFailedException);
    MORDOR_TEST_ASSERT_EXCEPTION(request2->ensureResponse(), PriorRequestFailedException);

    /* This assertion no longer holds, because instead of closing the connection
       (gracefully), we cancelRead and cancelWrite to avoid possible exceptions.
       DuplexStream doesn't do anything when cancelling, so the data can still
       be read

    // Verify response can't be read (exception; when using a real socket it might let us
    // read to EOF)
    MemoryStream responseBody;
    MORDOR_TEST_ASSERT_EXCEPTION(transferStream(request1->responseStream(), responseBody),
        BrokenPipeException);
    */

    MORDOR_TEST_ASSERT_EXCEPTION(conn->request(requestHeaders), PriorRequestFailedException);
}

MORDOR_UNITTEST(HTTPClient, simpleRequestAbandoned)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.method = PUT;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.general.connection.insert("close");
    requestHeaders.entity.contentLength = 5;

    ClientRequest::ptr request = conn->request(requestHeaders);
    // Nothing has been flushed yet
    MORDOR_TEST_ASSERT_EQUAL(requestStream->size(), 0);
    Stream::ptr requestBody = request->requestStream();
    // Verify stream characteristics
    MORDOR_TEST_ASSERT(!requestBody->supportsRead());
    MORDOR_TEST_ASSERT(requestBody->supportsWrite());
    MORDOR_TEST_ASSERT(!requestBody->supportsSeek());
    MORDOR_TEST_ASSERT(requestBody->supportsSize());
    MORDOR_TEST_ASSERT(!requestBody->supportsTruncate());
    MORDOR_TEST_ASSERT(!requestBody->supportsFind());
    MORDOR_TEST_ASSERT(!requestBody->supportsUnread());
    MORDOR_TEST_ASSERT_EQUAL(requestBody->size(), 5);

    // Force a flush (of the headers)
    requestBody->flush();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "PUT / HTTP/1.0\r\n"
        "Connection: close\r\n"
        "Content-Length: 5\r\n"
        "\r\n");

    request.reset();
    // Write the body - not allowed because we abandoned the request
    MORDOR_TEST_ASSERT_ASSERTED(requestBody->write("hello"));
}

MORDOR_UNITTEST(HTTPClient, simpleResponseAbandoned)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 5\r\n"
        "Connection: close\r\n"
        "\r\n"
        "hello")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.general.connection.insert("close");

    ClientRequest::ptr request = conn->request(requestHeaders);
    request->doRequest();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "GET / HTTP/1.0\r\n"
        "Connection: close\r\n"
        "\r\n");
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    // Verify response characteristics
    MORDOR_TEST_ASSERT(request->hasResponseBody());
    Stream::ptr response = request->responseStream();
    // Response stream has a ref to the request
    request.reset();
    MORDOR_TEST_ASSERT(response->supportsRead());
    MORDOR_TEST_ASSERT(!response->supportsWrite());
    MORDOR_TEST_ASSERT(!response->supportsSeek());
    MORDOR_TEST_ASSERT(response->supportsSize());
    MORDOR_TEST_ASSERT(!response->supportsTruncate());
    MORDOR_TEST_ASSERT(!response->supportsFind());
    MORDOR_TEST_ASSERT(!response->supportsUnread());
    MORDOR_TEST_ASSERT_EQUAL(response->size(), 5);

    Buffer buf;
    MORDOR_TEST_ASSERT_EQUAL(response->read(buf, 3), 3u);
    MORDOR_TEST_ASSERT(buf == "hel");
    // This should be okay, but will kill the connection
    response.reset();
    MORDOR_TEST_ASSERT_EXCEPTION(conn->request(requestHeaders),
        PriorRequestFailedException);
}

MORDOR_UNITTEST(HTTPClient, simpleResponseAbandonRequest)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 5\r\n"
        "Connection: close\r\n"
        "\r\n"
        "hello")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.general.connection.insert("close");

    ClientRequest::ptr request = conn->request(requestHeaders);
    request->doRequest();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "GET / HTTP/1.0\r\n"
        "Connection: close\r\n"
        "\r\n");
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    // Verify response characteristics
    MORDOR_TEST_ASSERT(request->hasResponseBody());
    Stream::ptr response = request->responseStream();
    request.reset();
    MORDOR_TEST_ASSERT(response->supportsRead());
    MORDOR_TEST_ASSERT(!response->supportsWrite());
    MORDOR_TEST_ASSERT(!response->supportsSeek());
    MORDOR_TEST_ASSERT(response->supportsSize());
    MORDOR_TEST_ASSERT(!response->supportsTruncate());
    MORDOR_TEST_ASSERT(!response->supportsFind());
    MORDOR_TEST_ASSERT(!response->supportsUnread());
    MORDOR_TEST_ASSERT_EQUAL(response->size(), 5);

    // Verify response itself
    MemoryStream responseBody;
    transferStream(response, responseBody);
    MORDOR_TEST_ASSERT(responseBody.buffer() == "hello");
}

MORDOR_UNITTEST(HTTPClient, simpleResponseAbandonStream)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 5\r\n"
        "Connection: close\r\n"
        "\r\n"
        "hello")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.general.connection.insert("close");

    ClientRequest::ptr request = conn->request(requestHeaders);
    request->doRequest();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "GET / HTTP/1.0\r\n"
        "Connection: close\r\n"
        "\r\n");
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    // Verify response characteristics
    MORDOR_TEST_ASSERT(request->hasResponseBody());
    Stream::ptr response = request->responseStream();
    request.reset();
    MORDOR_TEST_ASSERT(response->supportsRead());
    MORDOR_TEST_ASSERT(!response->supportsWrite());
    MORDOR_TEST_ASSERT(!response->supportsSeek());
    MORDOR_TEST_ASSERT(response->supportsSize());
    MORDOR_TEST_ASSERT(!response->supportsTruncate());
    MORDOR_TEST_ASSERT(!response->supportsFind());
    MORDOR_TEST_ASSERT(!response->supportsUnread());
    MORDOR_TEST_ASSERT_EQUAL(response->size(), 5);

    // Everything should clean up properly
    response.reset();
}

MORDOR_UNITTEST(HTTPClient, simpleResponseExceptionInStream)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 250005\r\n"
        "Connection: close\r\n"
        "\r\n"
        "hello")));
    Stream::ptr randomStream(new RandomStream());
    randomStream.reset(new LimitedStream(randomStream, 250000));
    std::vector<Stream::ptr> streams;
    streams.push_back(responseStream);
    streams.push_back(randomStream);
    Stream::ptr completeResponseStream(new CatStream(streams));
    TestStream::ptr testStream(new TestStream(completeResponseStream));
    testStream->onRead(&throwSocketException, 100000);
    DuplexStream::ptr duplexStream(new DuplexStream(testStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.general.connection.insert("close");

    ClientRequest::ptr request = conn->request(requestHeaders);
    request->doRequest();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "GET / HTTP/1.0\r\n"
        "Connection: close\r\n"
        "\r\n");
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    // Verify response characteristics
    MORDOR_TEST_ASSERT(request->hasResponseBody());
    Stream::ptr response = request->responseStream();
    request.reset();
    MORDOR_TEST_ASSERT_EXCEPTION(transferStream(response, NullStream::get()),
        ConnectionResetException);
}

MORDOR_UNITTEST(HTTPClient, emptyResponseCompleteBeforeRequestComplete)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.general.connection.insert("close");
    requestHeaders.entity.contentLength = 5;

    ClientRequest::ptr request = conn->request(requestHeaders);
    request->requestStream()->flush();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "GET / HTTP/1.0\r\n"
        "Connection: close\r\n"
        "Content-Length: 5\r\n"
        "\r\n");
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, HTTP::OK);

    request->requestStream()->write("hello");
    request->requestStream()->close();
}

static void emptyResponseCompleteBeforeRequestCompletePipelinedSecondRequest(
    ClientConnection::ptr conn, ClientRequest::ptr &request2)
{
    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.entity.contentLength = 5;
    request2 = conn->request(requestHeaders);
}

MORDOR_UNITTEST(HTTPClient, emptyResponseCompleteBeforeRequestCompletePipelined)
{
    WorkerPool pool;
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "\r\n"
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "\r\n")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.entity.contentLength = 5;

    ClientRequest::ptr request1 = conn->request(requestHeaders), request2;
    // Has to happen in a separate fiber because we need to force request1 to
    // not flush (because there's already another request in the queue, before
    // this request has completed)
    pool.schedule(boost::bind(&emptyResponseCompleteBeforeRequestCompletePipelinedSecondRequest,
        conn, boost::ref(request2)));
    pool.dispatch();

    MORDOR_TEST_ASSERT_EQUAL(request1->response().status.status, HTTP::OK);
    request1->requestStream()->write("hello");
    request1->requestStream()->close();
    pool.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(request2->response().status.status, HTTP::OK);
    request2->requestStream()->write("hello");
    request2->requestStream()->close();
}

MORDOR_UNITTEST(HTTPClient, simpleResponseCompleteBeforeRequestComplete)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 5\r\n"
        "Connection: close\r\n"
        "\r\n"
        "hello")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.general.connection.insert("close");
    requestHeaders.entity.contentLength = 5;

    ClientRequest::ptr request = conn->request(requestHeaders);
    request->requestStream()->flush();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "GET / HTTP/1.0\r\n"
        "Connection: close\r\n"
        "Content-Length: 5\r\n"
        "\r\n");
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, HTTP::OK);
    MemoryStream response;
    transferStream(request->responseStream(), response);
    MORDOR_ASSERT(response.buffer() == "hello");

    request->requestStream()->write("world");
    request->requestStream()->close();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "GET / HTTP/1.0\r\n"
        "Connection: close\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "world");
}

MORDOR_UNITTEST(HTTPClient, responseFailBeforeRequestComplete)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Len")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.general.connection.insert("close");
    requestHeaders.entity.contentLength = 5;

    ClientRequest::ptr request = conn->request(requestHeaders);
    request->requestStream()->flush();
    MORDOR_TEST_ASSERT(requestStream->buffer() ==
        "GET / HTTP/1.0\r\n"
        "Connection: close\r\n"
        "Content-Length: 5\r\n"
        "\r\n");
    MORDOR_TEST_ASSERT_EXCEPTION(request->response(), IncompleteMessageHeaderException);
    MORDOR_TEST_ASSERT_EXCEPTION(request->response(), IncompleteMessageHeaderException);
    MORDOR_TEST_ASSERT_EXCEPTION(request->response(), IncompleteMessageHeaderException);
}

static void newRequestWhileFlushing(ClientConnection::ptr conn, int &sequence, NotifyStream::ptr notify)
{
    if (notify) {
        notify->notifyOnFlush = NULL;
        MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
        Scheduler::getThis()->schedule(boost::bind(&newRequestWhileFlushing, conn, boost::ref(sequence), NotifyStream::ptr()));
        Scheduler::yield();
        return;
    }
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 2);

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";

    ClientRequest::ptr request = conn->request(requestHeaders);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 4);
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, HTTP::OK);
}

MORDOR_UNITTEST(HTTPClient, newRequestWhileFlushing)
{
    WorkerPool pool;
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "\r\n"
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "\r\n")));
    NotifyStream::ptr notifyStream(new NotifyStream(requestStream));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, notifyStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    int sequence = 0;
    notifyStream->notifyOnFlush = boost::bind(&newRequestWhileFlushing, conn, boost::ref(sequence), notifyStream);

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";

    ClientRequest::ptr request = conn->request(requestHeaders);
    request->doRequest();
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, HTTP::OK);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 3);
    pool.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 5);
}

static void
zlibCausesPrematureEOFServer(const URI &uri, ServerRequest::ptr request)
{
    request->response().general.transferEncoding.push_back("deflate");
    request->response().general.transferEncoding.push_back("chunked");
    request->response().status.status = OK;
    RandomStream random;
    transferStream(random, request->responseStream(), 4096);
    request->responseStream()->close();
}

MORDOR_UNITTEST(HTTPClient, zlibCausesPrematureEOF)
{
    WorkerPool pool;
    MockConnectionBroker server(&zlibCausesPrematureEOFServer);
    BaseRequestBroker requestBroker(ConnectionBroker::ptr(&server, &nop<ConnectionBroker *>));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "http://localhost/";

    ClientRequest::ptr request = requestBroker.request(requestHeaders);
    request->doRequest();
    transferStream(request->responseStream(), NullStream::get());

    request = requestBroker.request(requestHeaders);
    request->doRequest();
    transferStream(request->responseStream(), NullStream::get());
}

static void waitForPriorResponseFailed(ClientRequest::ptr request, int &sequence)
{
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 2);
    MORDOR_TEST_ASSERT_EXCEPTION(request->response(), PriorRequestFailedException);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 4);
}

MORDOR_UNITTEST(HTTPClient, priorResponseFailedPipeline)
{
    WorkerPool pool;
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream());
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "http://localhost/";

    ClientRequest::ptr request1 = conn->request(requestHeaders);
    request1->doRequest();
    ClientRequest::ptr request2 = conn->request(requestHeaders);
    request2->doRequest();
    int sequence = 1;
    pool.schedule(boost::bind(&waitForPriorResponseFailed, request2, boost::ref(sequence)));
    pool.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 3);
    request1->cancel(true);
    pool.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 5);
}

static void
serverHangsUpOnRequestServer(const URI &uri, ServerRequest::ptr request)
{
    request->response().status.status = OK;
    request->response().entity.contentLength = 0;
    request->response().general.connection.insert("close");
}

static void
sendRequest(ClientRequest::ptr request, unsigned long long length, bool &wroteItAll, bool &excepted)
{
    try {
        RandomStream random;
        transferStream(random, request->requestStream(), length);
        wroteItAll = true;
        request->requestStream()->close();
    } catch (...) {
        excepted = true;
        throw;
    }
}

static void serverHangsUpOnRequest(long long length, bool shouldWriteItAll)
{
    WorkerPool pool;
    MockConnectionBroker server(&serverHangsUpOnRequestServer);
    BaseRequestBroker requestBroker(ConnectionBroker::ptr(&server, &nop<ConnectionBroker *>));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "http://localhost/";
    requestHeaders.entity.contentLength = length;

    bool wroteItAll = false, excepted = false;
    ClientRequest::ptr request = requestBroker.request(requestHeaders, false,
        boost::bind(&sendRequest, _1, length,
        boost::ref(wroteItAll), boost::ref(excepted)));
    MORDOR_ASSERT(excepted);
    MORDOR_TEST_ASSERT_EQUAL(wroteItAll, shouldWriteItAll);
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
}

MORDOR_UNITTEST(HTTPClient, serverHangsUpOnRequest)
{ serverHangsUpOnRequest(1024 * 1024, false); }
MORDOR_UNITTEST(HTTPClient, serverHangsUpOnRequestFlush)
{ serverHangsUpOnRequest(64 * 1024, true); }

static void throwExceptionForRequest(ClientRequest::ptr request)
{
    MORDOR_THROW_EXCEPTION(DummyException());
}

MORDOR_UNITTEST(HTTPClient, sendException)
{
    WorkerPool pool;
    MockConnectionBroker server(&serverHangsUpOnRequestServer);
    BaseRequestBroker requestBroker(ConnectionBroker::ptr(&server, &nop<ConnectionBroker *>));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "http://localhost/";
    requestHeaders.entity.contentLength = 1024 * 1024;

    MORDOR_TEST_ASSERT_EXCEPTION(
        requestBroker.request(requestHeaders, false, &throwExceptionForRequest),
        DummyException);
}

static void
serverDelaysFirstResponse(const URI &uri, ServerRequest::ptr request,
                          TimerManager &timerManager)
{
    request->processNextRequest();
    if (request->request().requestLine.uri == "/delay") {
        sleep(timerManager, 200000);
        request->response().status.status = OK;
        request->response().entity.contentLength = 0;
    } else {
        if (request->hasRequestBody())
            transferStream(request->requestStream(), NullStream::get());
        request->response().status.status = OK;
        request->response().entity.contentLength = 0;
    }
}

static void sendBodyForPointThreeSecond(ClientRequest::ptr request,
                                        TimerManager &timerManager)
{
    RandomStream random;
    unsigned long long start = TimerManager::now();
    // Transfer 64KB at a time, checking our timer
    while (TimerManager::now() - start < 300000) {
        transferStream(random, request->requestStream(), 65536);
        sleep(timerManager, 1000);
    }
    request->requestStream()->close();
}

static void firstRequest(BaseRequestBroker &requestBroker)
{
    Request requestHeaders;
    requestHeaders.requestLine.uri = "http://localhost/delay";

    // This should *not* timeout
    ClientRequest::ptr request = requestBroker.request(requestHeaders);
    request->doRequest();
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, HTTP::OK);
}

MORDOR_UNITTEST(HTTPClient, pipelineTimeout)
{
    IOManager ioManager;

    MockConnectionBroker server(boost::bind(&serverDelaysFirstResponse,
        _1, _2, boost::ref(ioManager)), &ioManager, 100000, ~0ull);
    BaseRequestBroker requestBroker(ConnectionBroker::ptr(&server, &nop<ConnectionBroker *>));

    ioManager.schedule(boost::bind(&firstRequest, boost::ref(requestBroker)));
    Scheduler::yield();

    Request requestHeaders;
    requestHeaders.requestLine.method = HTTP::PUT;
    requestHeaders.requestLine.uri = "http://localhost/somethingElse";
    requestHeaders.general.transferEncoding.push_back("chunked");

    ClientRequest::ptr request = requestBroker.request(requestHeaders, false,
        boost::bind(&sendBodyForPointThreeSecond, _1, boost::ref(ioManager)));
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, HTTP::OK);
}

static void
serverHangsUpAfterReadingRequestServer(ServerRequest::ptr request)
{
    if (request->hasRequestBody())
        transferStream(request->requestStream(), NullStream::get());
    request->response().status.status = OK;
    request->response().entity.contentLength = 0;
    request->response().general.connection.insert("close");
}

static void firstRequestGetsClosed(ClientConnection::ptr conn)
{
    Request requestHeaders;
    requestHeaders.requestLine.uri = "http://localhost/delay";

    ClientRequest::ptr request = conn->request(requestHeaders);
    request->doRequest();
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, HTTP::OK);
    MORDOR_ASSERT(request->response().general.connection.find("close") !=
        request->response().general.connection.end());
}

MORDOR_UNITTEST(HTTPClient, priorResponseCloseWhileRequestInProgress)
{
    WorkerPool pool;

    std::pair<Stream::ptr, Stream::ptr> pipes = pipeStream();
    ClientConnection::ptr client(new ClientConnection(pipes.first));
    ServerConnection::ptr server(new ServerConnection(pipes.second,
        &serverHangsUpAfterReadingRequestServer));
    pool.schedule(boost::bind(&ServerConnection::processRequests, server));

    pool.schedule(boost::bind(&firstRequestGetsClosed, client));
    Scheduler::yield();

    Request requestHeaders;
    requestHeaders.requestLine.method = HTTP::PUT;
    requestHeaders.requestLine.uri = "http://localhost/somethingElse";
    requestHeaders.general.transferEncoding.push_back("chunked");

    ClientRequest::ptr request = client->request(requestHeaders);
    RandomStream random;
    MORDOR_TEST_ASSERT_EXCEPTION(transferStream(random, request->requestStream()),
        PriorRequestFailedException);
}

MORDOR_UNITTEST(HTTPClient, responseFailedCorrectException)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n")));
    TestStream::ptr testStream(new TestStream(responseStream));
    testStream->onRead(&throwDummyException, 0);
    DuplexStream::ptr duplexStream(new DuplexStream(testStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";

    ClientRequest::ptr request = conn->request(requestHeaders);
    request->doRequest();
    MORDOR_TEST_ASSERT_EXCEPTION(request->response(), DummyException);
}

static void cancelWhileResponseQueued(ClientConnection::ptr conn, ClientRequest::ptr &request)
{
    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";

    ClientRequest::ptr realRequest = conn->request(requestHeaders);
    request = realRequest;
    request->doRequest();
    MORDOR_TEST_ASSERT_EXCEPTION(realRequest->response(), OperationAbortedException);
}

static void readStuff(Stream::ptr stream)
{
    try {
        transferStream(stream, NullStream::get());
    } catch (BrokenPipeException &) {
    }
}

MORDOR_UNITTEST(HTTPClient, cancelWhileResponseQueued)
{
    WorkerPool pool;

    std::pair<Stream::ptr, Stream::ptr> pipes = pipeStream();
    ClientConnection::ptr conn(new ClientConnection(pipes.first));

    ClientRequest::ptr request1, request2;
    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";

    pool.schedule(boost::bind(&readStuff, pipes.second));
    pool.schedule(boost::bind(&cancelWhileResponseQueued, conn,
        boost::ref(request2)));
    request1 = conn->request(requestHeaders);
    request1->doRequest();
    pool.dispatch();

    MORDOR_ASSERT(request2);
    request2->cancel();
}

MORDOR_UNITTEST(HTTPClient, abortWhileResponseQueued)
{
    WorkerPool pool;

    std::pair<Stream::ptr, Stream::ptr> pipes = pipeStream();
    ClientConnection::ptr conn(new ClientConnection(pipes.first));

    ClientRequest::ptr request1, request2;
    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";

    pool.schedule(boost::bind(&readStuff, pipes.second));
    pool.schedule(boost::bind(&cancelWhileResponseQueued, conn,
        boost::ref(request2)));
    request1 = conn->request(requestHeaders);
    request1->doRequest();
    pool.dispatch();

    MORDOR_ASSERT(request2);
    request2->cancel(true);
}

class NoFlushStream : public FilterStream
{
public:
    NoFlushStream(Stream::ptr parent)
        : FilterStream(parent)
    {}

    size_t read(Buffer &buffer, size_t length)
    { return parent()->read(buffer, length); }
    size_t write(const Buffer &buffer, size_t length)
    { return parent()->write(buffer, length); }

    void flush(bool flushParent) {}
};

MORDOR_UNITTEST(HTTPClient, requestFailOtherWaitingResponse)
{
    std::pair<Stream::ptr, Stream::ptr> pipes = pipeStream();
    // Don't bother flushing and waiting for someone to read it
    Stream::ptr noFlushStream(new NoFlushStream(pipes.first));
    // Put the bufferedStream *underneath* the testStream, so we make sure
    // to generate the exception on the write() in doRequest, not in the
    // flush()
    BufferedStream::ptr bufferedStream(new BufferedStream(noFlushStream));
    TestStream::ptr testStream(new TestStream(bufferedStream));
    ClientConnection::ptr conn(new ClientConnection(testStream));

    ClientRequest::ptr request1, request2;
    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";

    request1 = conn->request(requestHeaders);
    request1->doRequest();
    testStream->onWrite(&throwDummyException, 0);

    request2 = conn->request(requestHeaders);
    MORDOR_TEST_ASSERT_EXCEPTION(request2->doRequest(), DummyException);
}

static void waitFor200(ClientRequest::ptr request, int &sequence)
{
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    ++sequence;
}

MORDOR_UNITTEST(HTTPClient, requestFailOthersWaitingResponse)
{
    WorkerPool pool;

    std::pair<Stream::ptr, Stream::ptr> pipes = pipeStream();
    // Don't bother flushing and waiting for someone to read it
    Stream::ptr noFlushStream(new NoFlushStream(pipes.first));
    // Put the bufferedStream *underneath* the testStream, so we make sure
    // to generate the exception on the write() in doRequest, not in the
    // flush()
    BufferedStream::ptr bufferedStream(new BufferedStream(noFlushStream));
    bufferedStream->allowPartialReads(true);
    TestStream::ptr testStream(new TestStream(bufferedStream));
    ClientConnection::ptr conn(new ClientConnection(testStream));

    ClientRequest::ptr request1, request2, request3, request4;
    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "localhost";

    int sequence = 0;
    request1 = conn->request(requestHeaders);
    request1->doRequest();
    request2 = conn->request(requestHeaders);
    request2->doRequest();
    request3 = conn->request(requestHeaders);
    request3->doRequest();
    pool.schedule(boost::bind(&waitFor200, request1, boost::ref(sequence)));
    pool.schedule(boost::bind(&waitFor200, request2, boost::ref(sequence)));
    pool.schedule(boost::bind(&waitFor200, request3, boost::ref(sequence)));
    Scheduler::yield();

    testStream->onWrite(&throwDummyException, 0);
    request4 = conn->request(requestHeaders);
    MORDOR_TEST_ASSERT_EXCEPTION(request4->doRequest(), DummyException);
    // Finish requests 1 through 3
    pipes.second->write("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"
        "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"
        "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
    pipes.second->flush();
    pool.dispatch();
    // *all* responses should be complete
    MORDOR_TEST_ASSERT_EQUAL(sequence, 3);
}

MORDOR_UNITTEST(HTTPClient, requestBodyFailOthersWaitingResponse)
{
    WorkerPool pool;

    std::pair<Stream::ptr, Stream::ptr> pipes = pipeStream();
    // Don't bother flushing and waiting for someone to read it
    Stream::ptr noFlushStream(new NoFlushStream(pipes.first));
    // Put the bufferedStream *underneath* the testStream, so we make sure
    // to generate the exception on the write() in doRequest, not in the
    // flush()
    BufferedStream::ptr bufferedStream(new BufferedStream(noFlushStream));
    bufferedStream->allowPartialReads(true);
    TestStream::ptr testStream(new TestStream(bufferedStream));
    ClientConnection::ptr conn(new ClientConnection(testStream));

    ClientRequest::ptr request1, request2, request3;
    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "localhost";

    int sequence = 0;
    request1 = conn->request(requestHeaders);
    request1->doRequest();
    request2 = conn->request(requestHeaders);
    request2->doRequest();
    request3 = conn->request(requestHeaders);
    request3->doRequest();
    pool.schedule(boost::bind(&waitFor200, request1, boost::ref(sequence)));
    pool.schedule(boost::bind(&waitFor200, request2, boost::ref(sequence)));
    pool.schedule(boost::bind(&waitFor200, request3, boost::ref(sequence)));
    Scheduler::yield();

    requestHeaders.entity.contentLength = 1;
    ClientRequest::ptr request4 = conn->request(requestHeaders);
    request4->doRequest();
    testStream->onWrite(&throwDummyException, 0);
    MORDOR_TEST_ASSERT_EXCEPTION(request4->requestStream()->write("a", 1), DummyException);
    request4->cancel();
    // Finish requests 1 through 3
    pipes.second->write("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"
        "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"
        "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
    pipes.second->flush();
    pool.dispatch();
    // *all* responses should be complete
    MORDOR_TEST_ASSERT_EQUAL(sequence, 3);
}

static void waitForOperationAborted(ClientRequest::ptr request, bool &excepted)
{
    MORDOR_TEST_ASSERT_EXCEPTION(request->response(), OperationAbortedException);
    excepted = true;
}

static void waitForPriorRequestFailedOnRequest(ClientConnection::ptr conn, bool &excepted)
{
    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "localhost";
    MORDOR_TEST_ASSERT_EXCEPTION(conn->request(requestHeaders), PriorRequestFailedException);
    excepted = true;
}

MORDOR_UNITTEST(HTTPClient, requestBodyFailOthersAndSelfWaitingResponse)
{
    WorkerPool pool;

    std::pair<Stream::ptr, Stream::ptr> pipes = pipeStream();
    // Don't bother flushing and waiting for someone to read it
    Stream::ptr noFlushStream(new NoFlushStream(pipes.first));
    // Put the bufferedStream *underneath* the testStream, so we make sure
    // to generate the exception on the write() in doRequest, not in the
    // flush()
    BufferedStream::ptr bufferedStream(new BufferedStream(noFlushStream));
    bufferedStream->allowPartialReads(true);
    TestStream::ptr testStream(new TestStream(bufferedStream));
    ClientConnection::ptr conn(new ClientConnection(testStream));

    ClientRequest::ptr request1, request2, request3;
    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "localhost";

    int sequence = 0;
    request1 = conn->request(requestHeaders);
    request1->doRequest();
    request2 = conn->request(requestHeaders);
    request2->doRequest();
    request3 = conn->request(requestHeaders);
    request3->doRequest();
    pool.schedule(boost::bind(&waitFor200, request1, boost::ref(sequence)));
    pool.schedule(boost::bind(&waitFor200, request2, boost::ref(sequence)));
    pool.schedule(boost::bind(&waitFor200, request3, boost::ref(sequence)));
    Scheduler::yield();

    bool excepted1 = false;
    bool excepted2 = false;
    requestHeaders.entity.contentLength = 1;
    ClientRequest::ptr request4 = conn->request(requestHeaders);
    request4->doRequest();
    pool.schedule(boost::bind(&waitForOperationAborted, request4, boost::ref(excepted1)));
    pool.schedule(boost::bind(&waitForPriorRequestFailedOnRequest, conn, boost::ref(excepted2)));
    Scheduler::yield();
    testStream->onWrite(&throwDummyException, 0);
    MORDOR_TEST_ASSERT_EXCEPTION(request4->requestStream()->write("a", 1), DummyException);
    request4->cancel();
    // Finish requests 1 through 3
    pipes.second->write("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"
        "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"
        "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
    pipes.second->flush();
    pool.dispatch();
    // *all* responses should be complete
    MORDOR_TEST_ASSERT_EQUAL(sequence, 3);
    MORDOR_ASSERT(excepted1);
    MORDOR_ASSERT(excepted2);
}

static void waitForPriorRequestFailedOnResponse(ClientRequest::ptr request, bool &excepted)
{
    MORDOR_TEST_ASSERT_EXCEPTION(request->response(), PriorRequestFailedException);
    excepted = true;
}

MORDOR_UNITTEST(HTTPClient, responseFailsAnotherWaitingResponseAnotherWaitingRequest)
{
    WorkerPool pool;

    std::pair<Stream::ptr, Stream::ptr> pipes = pipeStream();
    // Don't bother flushing and waiting for someone to read it
    Stream::ptr noFlushStream(new NoFlushStream(pipes.first));
    // Put the bufferedStream *underneath* the testStream, so we make sure
    // to generate the exception on the write() in doRequest, not in the
    // flush()
    BufferedStream::ptr bufferedStream(new BufferedStream(noFlushStream));
    bufferedStream->allowPartialReads(true);
    TestStream::ptr testStream(new TestStream(bufferedStream));
    ClientConnection::ptr conn(new ClientConnection(testStream));

    ClientRequest::ptr request1, request2, request3;
    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "localhost";

    bool excepted1 = false, excepted2 = false;
    request1 = conn->request(requestHeaders);
    request1->doRequest();
    requestHeaders.entity.contentLength = 5;
    request2 = conn->request(requestHeaders);
    request2->doRequest();
    pool.schedule(boost::bind(&waitForPriorRequestFailedOnResponse, request2, boost::ref(excepted1)));
    pool.schedule(boost::bind(&waitForPriorRequestFailedOnRequest, conn, boost::ref(excepted2)));
    Scheduler::yield();

    testStream->onRead(&throwDummyException, 0);
    MORDOR_TEST_ASSERT_EXCEPTION(request1->response(), DummyException);
    pool.dispatch();

    MORDOR_ASSERT(excepted1);
    MORDOR_ASSERT(excepted2);
}

MORDOR_UNITTEST(HTTPClient, responseCancelsWhileRequestScheduled)
{
    WorkerPool pool;

    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 7\r\n"
        "\r\n"
        "hello\r\n")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.entity.contentLength = 5;

    bool excepted = false;
    ClientRequest::ptr request1 = conn->request(requestHeaders);
    request1->doRequest();

    pool.schedule(boost::bind(&waitForPriorRequestFailedOnRequest, conn, boost::ref(excepted)));
    Scheduler::yield();
    request1->requestStream()->write("hello");
    request1->requestStream()->close();
    request1->response();
    request1->cancel();
    pool.dispatch();
    MORDOR_ASSERT(excepted);
}


MORDOR_UNITTEST(HTTPClient, responseFailsReadAfterAbort)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream(Buffer(
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 7\r\n"
        "\r\n"
        "hello\r\n")));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    // Put the bufferedStream *underneath* the testStream, so we make sure
    // to generate the exception on the read() after ensureResponse()
    BufferedStream::ptr bufferedStream(new BufferedStream(duplexStream));
    bufferedStream->allowPartialReads(true);
    TestStream::ptr testStream(new TestStream(bufferedStream));
    ClientConnection::ptr conn(new ClientConnection(testStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";

    ClientRequest::ptr request = conn->request(requestHeaders);
    request->doRequest();
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    request->cancel();
    Stream::ptr response = request->responseStream();
    testStream->onRead(&throwDummyException, 0);
    char buf[7];
    response->read(buf, 7);
}

MORDOR_UNITTEST(HTTPClient, responseFailsAfterLaterResponseFails)
{
    WorkerPool pool;

    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream());
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    BufferedStream::ptr bufferedStream(new BufferedStream(duplexStream));
    bufferedStream->allowPartialReads(true);
    TestStream::ptr testStream(new TestStream(bufferedStream));
    testStream->onRead(&throwDummyException, 0);
    ClientConnection::ptr conn(new ClientConnection(testStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";

    ClientRequest::ptr request1 = conn->request(requestHeaders);
    request1->doRequest();
    ClientRequest::ptr request2 = conn->request(requestHeaders);
    request2->doRequest();
    bool excepted = false;
    pool.schedule(boost::bind(&waitForPriorRequestFailedOnResponse, request2, boost::ref(excepted)));
    Scheduler::yield();

    requestHeaders.entity.contentLength = 1;
    ClientRequest::ptr request3 = conn->request(requestHeaders);
    testStream->onWrite(&throwDummyException, 0);
    MORDOR_TEST_ASSERT_EXCEPTION(request3->requestStream()->write("a"), DummyException);
    request3->cancel();

    Scheduler::yield();
    MORDOR_ASSERT(!excepted);

    MORDOR_TEST_ASSERT_EXCEPTION(request1->response(), DummyException);

    pool.dispatch();
    MORDOR_ASSERT(excepted);
}

MORDOR_UNITTEST(HTTPClient, cancelWhileReadingHeadersThenFinish)
{
    WorkerPool pool;

    std::pair<Stream::ptr, Stream::ptr> pipes = pipeStream();
    BufferedStream::ptr bufferedStream(new BufferedStream(pipes.first));
    bufferedStream->allowPartialReads(true);
    TestStream::ptr testStream(new TestStream(bufferedStream));
    ClientConnection::ptr conn(new ClientConnection(testStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "localhost";
    requestHeaders.entity.contentLength = 5;

    int sequence = 0;
    ClientRequest::ptr request = conn->request(requestHeaders);
    request->doRequest();
    pool.schedule(boost::bind(&waitFor200, request, boost::ref(sequence)));
    Scheduler::yield();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 0);
    testStream->onWrite(&throwDummyException, 0);
    MORDOR_TEST_ASSERT_EXCEPTION(request->requestStream()->write("hi"), DummyException);
    request->cancel();
    pipes.second->write("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n");
    Scheduler::yield();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
    request->finish();
}

MORDOR_UNITTEST(HTTPClient, abortWhileReadingHeadersThenFinish)
{
    WorkerPool pool;

    std::pair<Stream::ptr, Stream::ptr> pipes = pipeStream();
    BufferedStream::ptr bufferedStream(new BufferedStream(pipes.first));
    bufferedStream->allowPartialReads(true);
    TestStream::ptr testStream(new TestStream(bufferedStream));
    ClientConnection::ptr conn(new ClientConnection(testStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "localhost";
    requestHeaders.entity.contentLength = 5;

    bool excepted = false;
    ClientRequest::ptr request = conn->request(requestHeaders);
    request->doRequest();
    pool.schedule(boost::bind(&waitForOperationAborted, request, boost::ref(excepted)));
    Scheduler::yield();
    MORDOR_ASSERT(!excepted);
    testStream->onWrite(&throwDummyException, 0);
    MORDOR_TEST_ASSERT_EXCEPTION(request->requestStream()->write("hi"), DummyException);
    request->cancel(true);
    pipes.second->write("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n");
    Scheduler::yield();
    MORDOR_ASSERT(excepted);
    request->finish();
}

MORDOR_UNITTEST(HTTPClient, forceSkipsInRequestNumberBecauseIntermediateRequestAborted)
{
    WorkerPool pool;

    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream());
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    BufferedStream::ptr bufferedStream(new BufferedStream(duplexStream));
    bufferedStream->allowPartialReads(true);
    TestStream::ptr testStream(new TestStream(bufferedStream));
    testStream->onRead(&throwDummyException, 0);
    ClientConnection::ptr conn(new ClientConnection(testStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";

    ClientRequest::ptr request1 = conn->request(requestHeaders);
    request1->doRequest();
    ClientRequest::ptr request2 = conn->request(requestHeaders);
    request2->doRequest();
    ClientRequest::ptr request3 = conn->request(requestHeaders);
    request3->doRequest();
    bool excepted = false;
    pool.schedule(boost::bind(&waitForPriorRequestFailedOnResponse, request3, boost::ref(excepted)));
    Scheduler::yield();

    requestHeaders.entity.contentLength = 1;
    ClientRequest::ptr request4 = conn->request(requestHeaders);
    request4->doRequest();
    testStream->onWrite(&throwDummyException, 0);
    MORDOR_TEST_ASSERT_EXCEPTION(request4->requestStream()->write("a"), DummyException);

    Scheduler::yield();
    MORDOR_ASSERT(!excepted);

    MORDOR_TEST_ASSERT_EXCEPTION(request1->response(), DummyException);

    pool.dispatch();
    MORDOR_ASSERT(excepted);
}

MORDOR_UNITTEST(HTTPClient, priorResponseFailsThenRequestFails)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream());
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    TestStream::ptr testStream(new TestStream(duplexStream));
    testStream->onRead(&throwDummyException, 0);
    ClientConnection::ptr conn(new ClientConnection(testStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";

    ClientRequest::ptr request1 = conn->request(requestHeaders);
    request1->doRequest();

    requestHeaders.entity.contentLength = 5;
    ClientRequest::ptr request2 = conn->request(requestHeaders);
    request2->requestStream()->write("hello");
    testStream->onWrite(&throwDummyException, 0);
    MORDOR_TEST_ASSERT_EXCEPTION(request1->response(), DummyException);
    MORDOR_TEST_ASSERT_EXCEPTION(request2->response(), PriorRequestFailedException);
    // The DummyException gets translated to a PriorRequestFailedException
    MORDOR_TEST_ASSERT_EXCEPTION(request2->requestStream()->close(), PriorRequestFailedException);
}

MORDOR_UNITTEST(HTTPClient, failWhileFlushNoRequestBodyResponsePending)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream());
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    TestStream::ptr testStream(new TestStream(duplexStream));
    testStream->onWrite(&throwDummyException, 0);
    ClientConnection::ptr conn(new ClientConnection(testStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "localhost";

    ClientRequest::ptr request = conn->request(requestHeaders);
    MORDOR_TEST_ASSERT_EXCEPTION(request->doRequest(), DummyException);
}

MORDOR_UNITTEST(HTTPClient, failWhileFlushRequestBody)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    TestStream::ptr testStream(new TestStream(duplexStream));
    testStream->onWrite(&throwDummyException, 0);
    ClientConnection::ptr conn(new ClientConnection(testStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "localhost";
    requestHeaders.entity.contentLength = 5;

    ClientRequest::ptr request = conn->request(requestHeaders);
    request->requestStream()->write("hello");
    MORDOR_TEST_ASSERT_EXCEPTION(request->requestStream()->close(), DummyException);
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
}

MORDOR_UNITTEST(HTTPClient, priorResponseClosesWhileWaitingOnResponseAndWritingRequestBody)
{
    WorkerPool pool;

    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream("HTTP/1.1 200 OK\r\nConnection: close\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n"));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    ClientConnection::ptr conn(new ClientConnection(duplexStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "localhost";
    requestHeaders.entity.contentLength = 5;

    ClientRequest::ptr request1 = conn->request(requestHeaders);
    request1->requestStream()->write("hello");
    request1->requestStream()->close();
    ClientRequest::ptr request2 = conn->request(requestHeaders);
    request2->doRequest();
    bool excepted = false;
    pool.schedule(boost::bind(&waitForPriorRequestFailedOnResponse, request2, boost::ref(excepted)));
    Scheduler::yield();

    MORDOR_TEST_ASSERT_EQUAL(request1->response().status.status, OK);
    request1->finish();
    Scheduler::yield();
    MORDOR_ASSERT(excepted);
}

static void scheduleRequestAndThrowDummyException(ClientConnection::ptr conn,
    bool &excepted)
{
    Scheduler::getThis()->schedule(boost::bind(&waitForPriorRequestFailedOnRequest,
        conn, boost::ref(excepted)));
    Scheduler::yield();
    MORDOR_THROW_EXCEPTION(DummyException());
}


MORDOR_UNITTEST(HTTPClient, failWhileFlushOtherWaiting)
{
    WorkerPool pool;

    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream());
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    TestStream::ptr testStream(new TestStream(duplexStream));
    ClientConnection::ptr conn(new ClientConnection(testStream));
    bool excepted = false;
    testStream->onWrite(boost::bind(&scheduleRequestAndThrowDummyException, conn, boost::ref(excepted)), 0);

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "localhost";

    ClientRequest::ptr request = conn->request(requestHeaders);
    MORDOR_TEST_ASSERT_EXCEPTION(request->doRequest(), DummyException);
    Scheduler::yield();
    MORDOR_ASSERT(excepted);
}


MORDOR_UNITTEST(HTTPClient, failWhileRequestingOtherWaiting)
{
    WorkerPool pool;

    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream());
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    BufferedStream::ptr bufferedStream(new BufferedStream(duplexStream));
    TestStream::ptr testStream(new TestStream(bufferedStream));
    ClientConnection::ptr conn(new ClientConnection(testStream));
    bool excepted = false;
    testStream->onWrite(boost::bind(&scheduleRequestAndThrowDummyException, conn, boost::ref(excepted)), 0);

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "localhost";

    ClientRequest::ptr request = conn->request(requestHeaders);
    MORDOR_TEST_ASSERT_EXCEPTION(request->doRequest(), DummyException);
    Scheduler::yield();
    MORDOR_ASSERT(excepted);
}

MORDOR_UNITTEST(HTTPClient, failWhileFlushNoRequestBodyResponseHeaders)
{
    WorkerPool pool;

    std::pair<Stream::ptr, Stream::ptr> pipes = pipeStream();
    TestStream::ptr testStream(new TestStream(pipes.first));
    testStream->onWrite(&throwDummyException, 0);
    ClientConnection::ptr conn(new ClientConnection(testStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "localhost";

    bool excepted = false;
    ClientRequest::ptr request = conn->request(requestHeaders);
    pool.schedule(boost::bind(&waitForOperationAborted, request, boost::ref(excepted)));
    Scheduler::yield();
    MORDOR_ASSERT(!excepted);
    MORDOR_TEST_ASSERT_EXCEPTION(request->doRequest(), DummyException);
    Scheduler::yield();
    MORDOR_ASSERT(excepted);
}

// No need to test failWhileFlushRequestBodyResponseHeaders/Body/Complete
// because the request actually made it to the server, so the response won't
// get cancelled

MORDOR_UNITTEST(HTTPClient, failWhileFlushNoRequestBodyResponseBody)
{
    std::pair<Stream::ptr, Stream::ptr> pipes = pipeStream();
    TestStream::ptr testStream(new TestStream(pipes.first));
    testStream->onWrite(&throwDummyException, 0);
    ClientConnection::ptr conn(new ClientConnection(testStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "localhost";

    ClientRequest::ptr request = conn->request(requestHeaders);
    pipes.second->write("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello");
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    Stream::ptr responseStream = request->responseStream();
    MORDOR_TEST_ASSERT_EXCEPTION(request->doRequest(), DummyException);
    char dummy[6];
    dummy[5] = '\0';
    MORDOR_TEST_ASSERT_EQUAL(responseStream->read(dummy, 5), 5u);
    MORDOR_TEST_ASSERT_EQUAL((const char *)dummy, "hello");
}

MORDOR_UNITTEST(HTTPClient, failWhileFlushNoRequestBodyResponseWaiting)
{
    WorkerPool pool;

    std::pair<Stream::ptr, Stream::ptr> pipes = pipeStream();
    TestStream::ptr testStream(new TestStream(pipes.first));
    ClientConnection::ptr conn(new ClientConnection(testStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "localhost";

    bool excepted = false;
    ClientRequest::ptr request1 = conn->request(requestHeaders);
    pool.schedule(boost::bind(&readStuff, pipes.second));
    request1->doRequest();
    testStream->onWrite(&throwDummyException, 0);
    ClientRequest::ptr request2 = conn->request(requestHeaders);
    pool.schedule(boost::bind(&waitForOperationAborted, request2, boost::ref(excepted)));
    Scheduler::yield();
    MORDOR_ASSERT(!excepted);
    MORDOR_TEST_ASSERT_EXCEPTION(request2->doRequest(), DummyException);
    Scheduler::yield();
    MORDOR_ASSERT(excepted);
}

MORDOR_UNITTEST(HTTPClient, failWhileFlushNoRequestBodyResponseComplete)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    TestStream::ptr testStream(new TestStream(duplexStream));
    testStream->onWrite(&throwDummyException, 0);
    ClientConnection::ptr conn(new ClientConnection(testStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "localhost";

    ClientRequest::ptr request = conn->request(requestHeaders);
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    MORDOR_TEST_ASSERT_EXCEPTION(request->doRequest(), DummyException);
}

MORDOR_UNITTEST(HTTPClient, failWhileFlushRequestBodyResponseComplete)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"));
    DuplexStream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    TestStream::ptr testStream(new TestStream(duplexStream));
    testStream->onWrite(&throwDummyException, 0);
    ClientConnection::ptr conn(new ClientConnection(testStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "localhost";
    requestHeaders.entity.contentLength = 5;

    ClientRequest::ptr request = conn->request(requestHeaders);
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    request->requestStream()->write("hello");
    MORDOR_TEST_ASSERT_EXCEPTION(request->requestStream()->close(), DummyException);
}

MORDOR_UNITTEST(HTTPClient, failWhileRequestingResponseHeaders)
{
    WorkerPool pool;

    std::pair<Stream::ptr, Stream::ptr> pipes = pipeStream();
    pipes.first.reset(new BufferedStream(pipes.first));
    TestStream::ptr testStream(new TestStream(pipes.first));
    testStream->onWrite(&throwDummyException, 0);
    ClientConnection::ptr conn(new ClientConnection(testStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "localhost";

    bool excepted = false;
    ClientRequest::ptr request = conn->request(requestHeaders);
    pool.schedule(boost::bind(&waitForOperationAborted, request, boost::ref(excepted)));
    Scheduler::yield();
    MORDOR_ASSERT(!excepted);
    MORDOR_TEST_ASSERT_EXCEPTION(request->doRequest(), DummyException);
    Scheduler::yield();
    MORDOR_ASSERT(excepted);
}

MORDOR_UNITTEST(HTTPClient, failWhileRequestingResponseWaiting)
{
    WorkerPool pool;

    std::pair<Stream::ptr, Stream::ptr> pipes = pipeStream();
    pipes.first.reset(new BufferedStream(pipes.first));
    TestStream::ptr testStream(new TestStream(pipes.first));
    ClientConnection::ptr conn(new ClientConnection(testStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "localhost";

    bool excepted = false;
    ClientRequest::ptr request1 = conn->request(requestHeaders);
    pool.schedule(boost::bind(&readStuff, pipes.second));
    request1->doRequest();
    testStream->onWrite(&throwDummyException, 0);
    ClientRequest::ptr request2 = conn->request(requestHeaders);
    pool.schedule(boost::bind(&waitForOperationAborted, request2, boost::ref(excepted)));
    Scheduler::yield();
    MORDOR_ASSERT(!excepted);
    MORDOR_TEST_ASSERT_EXCEPTION(request2->doRequest(), DummyException);
    Scheduler::yield();
    MORDOR_ASSERT(excepted);
}

MORDOR_UNITTEST(HTTPClient, failWhileRequestingResponseBody)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello"));
    Stream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    duplexStream.reset(new BufferedStream(duplexStream));
    TestStream::ptr testStream(new TestStream(duplexStream));
    testStream->onWrite(&throwDummyException, 0);
    ClientConnection::ptr conn(new ClientConnection(testStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "localhost";

    ClientRequest::ptr request = conn->request(requestHeaders);
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    MORDOR_TEST_ASSERT_EXCEPTION(request->doRequest(), DummyException);
    char dummy[6];
    dummy[5] = '\0';
    MORDOR_TEST_ASSERT_EQUAL(request->responseStream()->read(dummy, 5), 5u);
    MORDOR_TEST_ASSERT_EQUAL((const char *)dummy, "hello");
}

MORDOR_UNITTEST(HTTPClient, failWhileRequestingResponseComplete)
{
    MemoryStream::ptr requestStream(new MemoryStream());
    MemoryStream::ptr responseStream(new MemoryStream("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"));
    Stream::ptr duplexStream(new DuplexStream(responseStream, requestStream));
    duplexStream.reset(new BufferedStream(duplexStream));
    TestStream::ptr testStream(new TestStream(duplexStream));
    testStream->onWrite(&throwDummyException, 0);
    ClientConnection::ptr conn(new ClientConnection(testStream));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/";
    requestHeaders.request.host = "localhost";

    ClientRequest::ptr request = conn->request(requestHeaders);
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    MORDOR_TEST_ASSERT_EXCEPTION(request->doRequest(), DummyException);
}
