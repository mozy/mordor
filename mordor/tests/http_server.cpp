// Copyright (c) 2009 - Mozy, Inc.

#include <boost/bind.hpp>

#include "mordor/fiber.h"
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
#include "mordor/streams/test.h"
#include "mordor/streams/transfer.h"
#include "mordor/test/test.h"
#include "mordor/timer.h"
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

static void pipelineWait(const URI &uri, ServerRequest::ptr request)
{
    request->processNextRequest();
    if (request->request().requestLine.uri == "/one") {
        Scheduler::yield();
        Scheduler::yield();
        Scheduler::yield();
    }
    request->response().entity.extension["X-Timestamp"] =
        boost::lexical_cast<std::string>(TimerManager::now());
    respondError(request, OK);
}

MORDOR_UNITTEST(HTTPServer, pipelineResponseWaits)
{
    WorkerPool pool;
    MockConnectionBroker server(&pipelineWait);
    ClientConnection::ptr connection =
        server.getConnection("http://localhost/").first;

    Request requestHeaders;
    requestHeaders.requestLine.uri = "/one";

    ClientRequest::ptr request1 = connection->request(requestHeaders);
    request1->doRequest();
    requestHeaders.requestLine.uri = "/two";
    ClientRequest::ptr request2 = connection->request(requestHeaders);
    request2->doRequest();
    unsigned long long timeStamp1 = boost::lexical_cast<unsigned long long>(
        request1->response().entity.extension.find("X-Timestamp")->second);
    unsigned long long timeStamp2 = boost::lexical_cast<unsigned long long>(
        request2->response().entity.extension.find("X-Timestamp")->second);
    MORDOR_TEST_ASSERT_LESS_THAN(timeStamp2, timeStamp1);
}

static void respondToTwo(TestStream::ptr testOutput, Fiber::ptr &fiber,
    int &sequence)
{
    testOutput->onWrite(NULL);
    while (!fiber)
        Scheduler::yield();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 3);
    Scheduler::getThis()->schedule(fiber);
    Scheduler::yield();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 5);
}

static void yieldTwoServer(ServerRequest::ptr request, Fiber::ptr &fiber2,
    int &sequence)
{
    request->processNextRequest();
    if (request->request().requestLine.uri == "/two") {
        MORDOR_TEST_ASSERT_EQUAL(++sequence, 2);
        fiber2 = Fiber::getThis();
        Scheduler::yieldTo();
        MORDOR_TEST_ASSERT_EQUAL(++sequence, 4);
        respondError(request, OK);
        MORDOR_TEST_ASSERT_EQUAL(++sequence, 7);
        return;
    }
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
    respondError(request, OK);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 6);
}

MORDOR_UNITTEST(HTTPServer, pipelineResponseWhilePriorResponseFlushing)
{
    Fiber::ptr fiber2;
    int sequence = 0;
    Stream::ptr input(new MemoryStream(Buffer(
        "GET /one HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n"
        "GET /two HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n")));
    MemoryStream::ptr output(new MemoryStream());
    TestStream::ptr testOutput(new TestStream(output));
    testOutput->onWrite(boost::bind(&respondToTwo, testOutput,
        boost::ref(fiber2), boost::ref(sequence)), 0);
    Stream::ptr stream(new DuplexStream(input, testOutput));
    ServerConnection::ptr conn(new ServerConnection(stream,
        boost::bind(&yieldTwoServer, _1, boost::ref(fiber2),
        boost::ref(sequence))));
    WorkerPool pool;
    pool.schedule(boost::bind(&ServerConnection::processRequests, conn));
    pool.dispatch();
}

namespace {
class UnseekableStream : public FilterStream
{
public:
    UnseekableStream(Stream::ptr parent, bool seekable, bool sizeable)
        : FilterStream(parent),
          m_seekable(seekable),
          m_sizeable(sizeable)
    {}

    bool supportsSeek() { return m_seekable; }
    bool supportsSize() { return m_sizeable; }

    size_t read(Buffer &buffer, size_t length)
    { return parent()->read(buffer, length); }

private:
    bool m_seekable, m_sizeable;
};
}

static void streamServer(const URI &uri, ServerRequest::ptr request)
{
    Stream::ptr memoryStream(new MemoryStream(Buffer("hello world!")));
    if (request->request().requestLine.uri == "/unseekable")
        memoryStream.reset(new UnseekableStream(memoryStream, false, true));
    if (request->request().requestLine.uri == "/unsizeable")
        memoryStream.reset(new UnseekableStream(memoryStream, true, false));
    if (request->request().requestLine.uri == "/unseekable/unsizeable")
        memoryStream.reset(new UnseekableStream(memoryStream, false, false));
    respondStream(request, memoryStream);
}

static void doRespondStream(RequestBroker &requestBroker, bool head,
    bool seekable, bool sizeable)
{
    Request requestHeaders;
    if (head)
        requestHeaders.requestLine.method = HEAD;
    requestHeaders.requestLine.uri = "http://localhost/";
    if (!seekable)
        requestHeaders.requestLine.uri.path.append("unseekable");
    if (!sizeable)
        requestHeaders.requestLine.uri.path.append("unsizeable");

    // Request the whole stream
    MemoryStream response;
    ClientRequest::ptr request = requestBroker.request(requestHeaders);
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    MORDOR_TEST_ASSERT_EQUAL(request->response().response.acceptRanges.size(),
        1u);
    MORDOR_TEST_ASSERT_EQUAL(*request->response().response.acceptRanges.begin(),
        "bytes");
    MORDOR_TEST_ASSERT_EQUAL(request->response().entity.contentLength,
        sizeable ? 12ull : ~0ull);
    if (!sizeable)
        MORDOR_TEST_ASSERT(!request->response().general.transferEncoding.empty());
    if (!head) {
        transferStream(request->responseStream(), response);
        MORDOR_TEST_ASSERT(response.buffer() == "hello world!");
        response.truncate(0ull);
        response.seek(0ull);
    }

    // Request just the first byte
    requestHeaders.request.range.push_back(std::make_pair(0ull, 0ull));
    request = requestBroker.request(requestHeaders);
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, PARTIAL_CONTENT);
    MORDOR_TEST_ASSERT_EQUAL(request->response().response.acceptRanges.size(),
        1u);
    MORDOR_TEST_ASSERT_EQUAL(*request->response().response.acceptRanges.begin(),
        "bytes");
    MORDOR_TEST_ASSERT_EQUAL(request->response().entity.contentLength, 1ull);
    MORDOR_TEST_ASSERT_EQUAL(request->response().entity.contentRange,
        ContentRange(0ull, 0ull, sizeable ? 12ull : ~0ull));
    if (!head) {
        transferStream(request->responseStream(), response);
        MORDOR_TEST_ASSERT(response.buffer() == "h");
        response.truncate(0ull);
        response.seek(0ull);
    }

    // Request second byte until the end
    requestHeaders.request.range.clear();
    requestHeaders.request.range.push_back(std::make_pair(1ull, ~0ull));
    request = requestBroker.request(requestHeaders);
    MORDOR_TEST_ASSERT_EQUAL(request->response().response.acceptRanges.size(),
        1u);
    MORDOR_TEST_ASSERT_EQUAL(*request->response().response.acceptRanges.begin(),
        "bytes");
    if (sizeable) {
        MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, PARTIAL_CONTENT);
        MORDOR_TEST_ASSERT_EQUAL(request->response().entity.contentLength, 11ull);
        MORDOR_TEST_ASSERT_EQUAL(request->response().entity.contentRange,
            ContentRange(1ull, 11ull, 12ull));
        if (!head) {
            transferStream(request->responseStream(), response);
            MORDOR_TEST_ASSERT(response.buffer() == "ello world!");
            response.truncate(0ull);
            response.seek(0ull);
        }
    } else {
        MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
        MORDOR_TEST_ASSERT(!request->response().general.transferEncoding.empty());
        if (!head) {
            transferStream(request->responseStream(), response);
            MORDOR_TEST_ASSERT(response.buffer() == "hello world!");
            response.truncate(0ull);
            response.seek(0ull);
        }
    }

    // Request last byte
    requestHeaders.request.range.clear();
    requestHeaders.request.range.push_back(std::make_pair(~0ull, 1ull));
    request = requestBroker.request(requestHeaders);
    MORDOR_TEST_ASSERT_EQUAL(request->response().response.acceptRanges.size(),
        1u);
    MORDOR_TEST_ASSERT_EQUAL(*request->response().response.acceptRanges.begin(),
        "bytes");
    if (sizeable) {
        MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, PARTIAL_CONTENT);
        MORDOR_TEST_ASSERT_EQUAL(request->response().entity.contentLength, 1ull);
        MORDOR_TEST_ASSERT_EQUAL(request->response().entity.contentRange,
            ContentRange(11ull, 11ull, 12ull));
        if (!head) {
            transferStream(request->responseStream(), response);
            MORDOR_TEST_ASSERT(response.buffer() == "!");
            response.truncate(0ull);
            response.seek(0ull);
        }
    } else {
        MORDOR_TEST_ASSERT_EQUAL(request->response().status.status,
            OK);
        MORDOR_TEST_ASSERT(!request->response().general.transferEncoding.empty());
        if (!head) {
            transferStream(request->responseStream(), response);
            MORDOR_TEST_ASSERT(response.buffer() == "hello world!");
            response.truncate(0ull);
            response.seek(0ull);
        }
    }

    // Request entire file
    requestHeaders.request.range.clear();
    requestHeaders.request.range.push_back(std::make_pair(0ull, 11ull));
    request = requestBroker.request(requestHeaders);
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status,
        sizeable ? OK : PARTIAL_CONTENT);
    MORDOR_TEST_ASSERT_EQUAL(request->response().response.acceptRanges.size(),
        1u);
    MORDOR_TEST_ASSERT_EQUAL(*request->response().response.acceptRanges.begin(),
        "bytes");
    MORDOR_TEST_ASSERT_EQUAL(request->response().entity.contentLength, 12ull);
    if (!head) {
        transferStream(request->responseStream(), response);
        MORDOR_TEST_ASSERT(response.buffer() == "hello world!");
        response.truncate(0ull);
        response.seek(0ull);
    }

    // Request entire file, with a suffix range
    requestHeaders.request.range.clear();
    requestHeaders.request.range.push_back(std::make_pair(~0ull, 12ull));
    request = requestBroker.request(requestHeaders);
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    MORDOR_TEST_ASSERT_EQUAL(request->response().response.acceptRanges.size(),
        1u);
    MORDOR_TEST_ASSERT_EQUAL(*request->response().response.acceptRanges.begin(),
        "bytes");
    if (sizeable)
        MORDOR_TEST_ASSERT_EQUAL(request->response().entity.contentLength, 12ull);
    else
        MORDOR_TEST_ASSERT(!request->response().general.transferEncoding.empty());
    if (!head) {
        transferStream(request->responseStream(), response);
        MORDOR_TEST_ASSERT(response.buffer() == "hello world!");
        response.truncate(0ull);
        response.seek(0ull);
    }

    // Invalid range gets full file
    requestHeaders.request.range.clear();
    requestHeaders.request.range.push_back(std::make_pair(1ull, 0ull));
    request = requestBroker.request(requestHeaders);
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    MORDOR_TEST_ASSERT_EQUAL(request->response().response.acceptRanges.size(),
        1u);
    MORDOR_TEST_ASSERT_EQUAL(*request->response().response.acceptRanges.begin(),
        "bytes");
    if (sizeable)
        MORDOR_TEST_ASSERT_EQUAL(request->response().entity.contentLength, 12ull);
    else
        MORDOR_TEST_ASSERT(!request->response().general.transferEncoding.empty());
    if (!head) {
        transferStream(request->responseStream(), response);
        MORDOR_TEST_ASSERT(response.buffer() == "hello world!");
        response.truncate(0ull);
        response.seek(0ull);
    }

    // TODO: this test cannot currently be run for unsizeable but seekable
    // stream; fix it
    if (sizeable || !seekable) {
        // Beyond end of file
        requestHeaders.request.range.clear();
        requestHeaders.request.range.push_back(std::make_pair(13ull, 13ull));
        request = requestBroker.request(requestHeaders);
        MORDOR_TEST_ASSERT_EQUAL(request->response().status.status,
            REQUESTED_RANGE_NOT_SATISFIABLE);
        request->finish();
    }

    // Suffix range beyond end of file
    requestHeaders.request.range.clear();
    requestHeaders.request.range.push_back(std::make_pair(~0ull, 13ull));
    request = requestBroker.request(requestHeaders);
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    MORDOR_TEST_ASSERT_EQUAL(request->response().response.acceptRanges.size(),
        1u);
    MORDOR_TEST_ASSERT_EQUAL(*request->response().response.acceptRanges.begin(),
        "bytes");
    if (sizeable)
        MORDOR_TEST_ASSERT_EQUAL(request->response().entity.contentLength, 12ull);
    else
        MORDOR_TEST_ASSERT(!request->response().general.transferEncoding.empty());
    if (!head) {
        transferStream(request->responseStream(), response);
        MORDOR_TEST_ASSERT(response.buffer() == "hello world!");
        response.truncate(0ull);
        response.seek(0ull);
    }

    // Non-contiguous range
    requestHeaders.request.range.clear();
    requestHeaders.request.range.push_back(std::make_pair(0ull, 0ull));
    requestHeaders.request.range.push_back(std::make_pair(2ull, 3ull));
    request = requestBroker.request(requestHeaders);
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    MORDOR_TEST_ASSERT_EQUAL(request->response().entity.contentType.type,
        "multipart");
    MORDOR_TEST_ASSERT_EQUAL(request->response().entity.contentType.subtype,
        "byteranges");
    if (!head) {
        Multipart::ptr multipart = request->responseMultipart();
        BodyPart::ptr part = multipart->nextPart();
        MORDOR_TEST_ASSERT_EQUAL(part->headers().contentRange,
            ContentRange(0ull, 0ull, sizeable ? 12ull : ~0ull));
        transferStream(part->stream(), response);
        MORDOR_TEST_ASSERT(response.buffer() == "h");
        response.truncate(0ull);
        response.seek(0ull);

        part = multipart->nextPart();
        MORDOR_TEST_ASSERT_EQUAL(part->headers().contentRange,
            ContentRange(2ull, 3ull, sizeable ? 12ull : ~0ull));
        transferStream(part->stream(), response);
        MORDOR_TEST_ASSERT(response.buffer() == "ll");
        response.truncate(0ull);
        response.seek(0ull);
    }
}

MORDOR_UNITTEST(HTTPServer, respondStreamRegular)
{
    WorkerPool pool;
    MockConnectionBroker server(&streamServer);
    BaseRequestBroker requestBroker(ConnectionBroker::ptr(&server,
        &nop<ConnectionBroker *>));

    doRespondStream(requestBroker, false, true, true);
    doRespondStream(requestBroker, true, true, true);
}

MORDOR_UNITTEST(HTTPServer, respondStreamUnseekable)
{
    WorkerPool pool;
    MockConnectionBroker server(&streamServer);
    BaseRequestBroker requestBroker(ConnectionBroker::ptr(&server,
        &nop<ConnectionBroker *>));

    doRespondStream(requestBroker, false, false, true);
    doRespondStream(requestBroker, true, false, true);
}

MORDOR_UNITTEST(HTTPServer, respondStreamUnsizeable)
{
    WorkerPool pool;
    MockConnectionBroker server(&streamServer);
    BaseRequestBroker requestBroker(ConnectionBroker::ptr(&server,
        &nop<ConnectionBroker *>));

    doRespondStream(requestBroker, false, true, false);
    doRespondStream(requestBroker, true, true, false);
}

MORDOR_UNITTEST(HTTPServer, respondStreamUnseekableUnsizeable)
{
    WorkerPool pool;
    MockConnectionBroker server(&streamServer);
    BaseRequestBroker requestBroker(ConnectionBroker::ptr(&server,
        &nop<ConnectionBroker *>));

    doRespondStream(requestBroker, false, true, false);
    doRespondStream(requestBroker, true, true, false);
}

static void exceptionAfterCommitServer(const URI &uri,
    ServerRequest::ptr request)
{
    request->response().entity.contentLength = 10;
    request->responseStream()->write("hello", 5);
    request->responseStream()->flush();
    throw std::runtime_error("Expected");
}

MORDOR_UNITTEST(HTTPServer, exceptionAfterCommit)
{
    WorkerPool pool;
    MockConnectionBroker server(&exceptionAfterCommitServer);
    BaseRequestBroker requestBroker(ConnectionBroker::ptr(&server,
        &nop<ConnectionBroker *>));

    Request requestHeaders;
    requestHeaders.requestLine.uri = "http://localhost/";

    ClientRequest::ptr request = requestBroker.request(requestHeaders);
    MORDOR_TEST_ASSERT_EQUAL(request->response().status.status, OK);
    MORDOR_TEST_ASSERT_EQUAL(request->response().entity.contentLength, 10u);
    char buffer[10];
    Stream::ptr responseStream = request->responseStream();
    MORDOR_TEST_ASSERT_EQUAL(responseStream->read(buffer, 10), 5u);
    buffer[5] = '\0';
    MORDOR_TEST_ASSERT_EQUAL((const char *)buffer, "hello");
    MORDOR_TEST_ASSERT_EXCEPTION(responseStream->read(buffer, 10),
        BrokenPipeException);
}
