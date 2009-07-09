// Copyright (c) 2009 - Decho Corp.

#include <boost/bind.hpp>

#include "mordor/common/http/parser.h"
#include "mordor/common/http/server.h"
#include "mordor/common/scheduler.h"
#include "mordor/common/streams/duplex.h"
#include "mordor/common/streams/memory.h"
#include "mordor/common/streams/transfer.h"
#include "mordor/test/test.h"

// Simplest success case
TEST_WITH_SUITE(HTTP, simpleRequest)
{
    HTTP::Request request;
    HTTP::RequestParser parser(request);

    parser.run("GET / HTTP/1.0\r\n\r\n");
    TEST_ASSERT(!parser.error());
    TEST_ASSERT(parser.complete());
    TEST_ASSERT_EQUAL(request.requestLine.method, HTTP::GET);
    TEST_ASSERT_EQUAL(request.requestLine.uri, URI("/"));
    TEST_ASSERT_EQUAL(request.requestLine.ver, HTTP::Version(1, 0));
}

TEST_WITH_SUITE(HTTP, emptyRequest)
{
    HTTP::Request request;
    HTTP::RequestParser parser(request);

    parser.run("");
    TEST_ASSERT(!parser.error());
    TEST_ASSERT(!parser.complete());
}

TEST_WITH_SUITE(HTTP, garbageRequest)
{
    HTTP::Request request;
    HTTP::RequestParser parser(request);

    parser.run("#*((@Nflk:J");
    TEST_ASSERT(parser.error());
    TEST_ASSERT(!parser.complete());
}

TEST_WITH_SUITE(HTTP, missingNewlineRequest)
{
    HTTP::Request request;
    HTTP::RequestParser parser(request);

    parser.run("GET / HTTP/1.0\r\n");
    TEST_ASSERT(!parser.error());
    TEST_ASSERT(!parser.complete());
    // Even though it's not complete, we should have parsed as much as was there
    TEST_ASSERT_EQUAL(request.requestLine.method, HTTP::GET);
    TEST_ASSERT_EQUAL(request.requestLine.uri, URI("/"));
    TEST_ASSERT_EQUAL(request.requestLine.ver, HTTP::Version(1, 0));
}

TEST_WITH_SUITE(HTTP, requestWithSimpleHeader)
{
    HTTP::Request request;
    HTTP::RequestParser parser(request);

    parser.run("GET / HTTP/1.0\r\n"
               "Connection: close\r\n"
               "\r\n");
    TEST_ASSERT(!parser.error());
    TEST_ASSERT(parser.complete());
    TEST_ASSERT_EQUAL(request.requestLine.method, HTTP::GET);
    TEST_ASSERT_EQUAL(request.requestLine.uri, URI("/"));
    TEST_ASSERT_EQUAL(request.requestLine.ver, HTTP::Version(1, 0));
    TEST_ASSERT_EQUAL(request.general.connection.size(), 1u);
    TEST_ASSERT(request.general.connection.find("close")
        != request.general.connection.end());
}

TEST_WITH_SUITE(HTTP, requestWithComplexHeader)
{
    HTTP::Request request;
    HTTP::RequestParser parser(request);

    parser.run("GET / HTTP/1.0\r\n"
               "Connection:\r\n"
               " keep-alive,  keep-alive\r\n"
               "\t, close\r\n"
               "\r\n");
    TEST_ASSERT(!parser.error());
    TEST_ASSERT(parser.complete());
    TEST_ASSERT_EQUAL(request.requestLine.method, HTTP::GET);
    TEST_ASSERT_EQUAL(request.requestLine.uri, URI("/"));
    TEST_ASSERT_EQUAL(request.requestLine.ver, HTTP::Version(1, 0));
    TEST_ASSERT_EQUAL(request.general.connection.size(), 2u);
    TEST_ASSERT(request.general.connection.find("close")
        != request.general.connection.end());
    TEST_ASSERT(request.general.connection.find("keep-alive")
        != request.general.connection.end());
}

TEST_WITH_SUITE(HTTP, rangeHeader)
{
    HTTP::Request request;
    HTTP::RequestParser parser(request);

    parser.run("GET / HTTP/1.1\r\n"
               "Range: bytes=0-499, 500-999, -500, 9500-, 0-0,-1\r\n"
               " ,500-600\r\n"
               "\r\n");
    TEST_ASSERT(!parser.error());
    TEST_ASSERT(parser.complete());
    TEST_ASSERT_EQUAL(request.requestLine.method, HTTP::GET);
    TEST_ASSERT_EQUAL(request.requestLine.uri, URI("/"));
    TEST_ASSERT_EQUAL(request.requestLine.ver, HTTP::Version(1, 1));
    TEST_ASSERT_EQUAL(request.request.range.size(), 7u);
    HTTP::RangeSet::const_iterator it = request.request.range.begin();
    TEST_ASSERT_EQUAL(it->first, 0u);
    TEST_ASSERT_EQUAL(it->second, 499u);
    ++it;
    TEST_ASSERT_EQUAL(it->first, 500u);
    TEST_ASSERT_EQUAL(it->second, 999u);
    ++it;
    TEST_ASSERT_EQUAL(it->first, ~0ull);
    TEST_ASSERT_EQUAL(it->second, 500u);
    ++it;
    TEST_ASSERT_EQUAL(it->first, 9500u);
    TEST_ASSERT_EQUAL(it->second, ~0ull);
    ++it;
    TEST_ASSERT_EQUAL(it->first, 0u);
    TEST_ASSERT_EQUAL(it->second, 0u);
    ++it;
    TEST_ASSERT_EQUAL(it->first, ~0ull);
    TEST_ASSERT_EQUAL(it->second, 1u);
    ++it;
    TEST_ASSERT_EQUAL(it->first, 500u);
    TEST_ASSERT_EQUAL(it->second, 600u);
}

TEST_WITH_SUITE(HTTP, versionComparison)
{
    HTTP::Version ver10(1, 0), ver11(1, 1);
    TEST_ASSERT(ver10 == ver10);
    TEST_ASSERT(ver11 == ver11);
    TEST_ASSERT(ver10 != ver11);
    TEST_ASSERT(ver10 <= ver11);
    TEST_ASSERT(ver10 < ver11);
    TEST_ASSERT(ver11 >= ver10);
    TEST_ASSERT(ver11 > ver10);
    TEST_ASSERT(ver10 <= ver10);
    TEST_ASSERT(ver10 >= ver10);
}

TEST_WITH_SUITE(HTTP, quoting)
{
    TEST_ASSERT_EQUAL(HTTP::quote(""), "\"\"");
    TEST_ASSERT_EQUAL(HTTP::quote("token"), "token");
    TEST_ASSERT_EQUAL(HTTP::quote("multiple words"), "\"multiple words\"");
    TEST_ASSERT_EQUAL(HTTP::quote("\tlotsa  whitespace\t"),
        "\"\tlotsa  whitespace\t\"");
    TEST_ASSERT_EQUAL(HTTP::quote("\""), "\"\\\"\"");
    TEST_ASSERT_EQUAL(HTTP::quote("\\"), "\"\\\\\"");
    TEST_ASSERT_EQUAL(HTTP::quote("weird < chars >"), "\"weird < chars >\"");
    TEST_ASSERT_EQUAL(HTTP::quote("multiple\\ escape\" sequences\\  "),
        "\"multiple\\\\ escape\\\" sequences\\\\  \"");
    TEST_ASSERT_EQUAL(HTTP::quote("tom"), "tom");
    TEST_ASSERT_EQUAL(HTTP::quote("\""), "\"\\\"\"");
    TEST_ASSERT_EQUAL(HTTP::quote("co\\dy"), "\"co\\\\dy\"");
}

static void
httpRequest(HTTP::ServerRequest::ptr request)
{
    switch (request->request().requestLine.method) {
        case HTTP::GET:
        case HTTP::HEAD:
        case HTTP::PUT:
        case HTTP::POST:
            request->response().entity.contentLength = request->request().entity.contentLength;
            request->response().entity.contentType = request->request().entity.contentType;
            request->response().general.transferEncoding = request->request().general.transferEncoding;
            request->response().status.status = HTTP::OK;
            request->response().entity.extension = request->request().entity.extension;
            if (request->hasRequestBody()) {
                if (request->request().requestLine.method != HTTP::HEAD) {
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
            break;
        default:
            respondError(request, HTTP::METHOD_NOT_ALLOWED);
            break;
    }
}

TEST_WITH_SUITE(HTTPServer, close10)
{
    Stream::ptr input(new MemoryStream(Buffer("GET / HTTP/1.0\r\n\r\n")));
    MemoryStream::ptr output(new MemoryStream());
    Stream::ptr stream(new DuplexStream(input, output));
    HTTP::ServerConnection::ptr conn(new HTTP::ServerConnection(stream, &httpRequest));
    Fiber::ptr mainfiber(new Fiber());
    WorkerPool pool;
    pool.schedule(Fiber::ptr(new Fiber(boost::bind(&HTTP::ServerConnection::processRequests, conn))));
    pool.yieldTo();
    TEST_ASSERT(output->buffer() == "HTTP/1.0 200 OK\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
}
