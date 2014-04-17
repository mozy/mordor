// Copyright (c) 2009 - Mozy, Inc.

#include <boost/bind.hpp>
#include <boost/format.hpp>

#include "mordor/http/parser.h"
#include "mordor/streams/buffered.h"
#include "mordor/streams/duplex.h"
#include "mordor/streams/memory.h"
#include "mordor/streams/null.h"
#include "mordor/test/test.h"
#include "mordor/util.h"

using namespace Mordor;
using namespace Mordor::HTTP;
using namespace Mordor::Test;

// Simplest success case
MORDOR_UNITTEST(HTTP, simpleRequest)
{
    Request request;
    RequestParser parser(request);

    parser.run("GET / HTTP/1.0\r\n\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.method, GET);
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.uri, URI("/"));
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.ver, Version(1, 0));
}

MORDOR_UNITTEST(HTTP, requestWithQuery)
{
    Request request;
    RequestParser parser(request);

    parser.run("POST /ab/d/e/wasdkfe/?ohai=1 HTTP/1.1\r\n\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.method, POST);
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.uri, URI("/ab/d/e/wasdkfe/?ohai=1"));
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.ver, Version(1, 1));
}

MORDOR_UNITTEST(HTTP, emptyRequest)
{
    Request request;
    RequestParser parser(request);

    parser.run("");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(!parser.complete());
}

MORDOR_UNITTEST(HTTP, garbageRequest)
{
    Request request;
    RequestParser parser(request);

    parser.run("#*((@Nflk:J");
    MORDOR_TEST_ASSERT(parser.error());
    MORDOR_TEST_ASSERT(!parser.complete());
}

MORDOR_UNITTEST(HTTP, missingNewlineRequest)
{
    Request request;
    RequestParser parser(request);

    parser.run("GET / HTTP/1.0\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(!parser.complete());
    // Even though it's not complete, we should have parsed as much as was there
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.method, GET);
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.uri, URI("/"));
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.ver, Version(1, 0));
}

MORDOR_UNITTEST(HTTP, requestWithSimpleHeader)
{
    Request request;
    RequestParser parser(request);

    parser.run("GET / HTTP/1.0\r\n"
               "Connection: close\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.method, GET);
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.uri, URI("/"));
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.ver, Version(1, 0));
    MORDOR_TEST_ASSERT_EQUAL(request.general.connection.size(), 1u);
    MORDOR_TEST_ASSERT(request.general.connection.find("close")
        != request.general.connection.end());
}

MORDOR_UNITTEST(HTTP, requestWithComplexHeader)
{
    Request request;
    RequestParser parser(request);

    parser.run("GET / HTTP/1.0\r\n"
               "Connection:\r\n"
               " keep-alive,  keep-alive\r\n"
               "\t, close\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.method, GET);
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.uri, URI("/"));
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.ver, Version(1, 0));
    MORDOR_TEST_ASSERT_EQUAL(request.general.connection.size(), 2u);
    MORDOR_TEST_ASSERT(request.general.connection.find("close")
        != request.general.connection.end());
    MORDOR_TEST_ASSERT(request.general.connection.find("keep-alive")
        != request.general.connection.end());
}

MORDOR_UNITTEST(HTTP, requestsWithBoundaryOnBufferBoundary)
{
    MemoryStream::ptr memoryStream(new MemoryStream());
    // Ensure that the two requests are in two separate buffers
    // in memory
    Buffer buf1("GET /blah1 HTTP/1.0\r\n\r\n"),
        buf2("PUT /blah2 HTTP/1.0\r\n\r\n");
    memoryStream->write(buf1, buf1.readAvailable());
    memoryStream->write(buf2, buf2.readAvailable());
    memoryStream->seek(0);
    DuplexStream::ptr duplexStream(new DuplexStream(memoryStream,
        Stream::ptr(&NullStream::get(), &nop<Stream *>)));
    BufferedStream::ptr bufferedStream(new BufferedStream(duplexStream));

    Request request1;
    RequestParser parser1(request1);

    parser1.run(bufferedStream);
    MORDOR_ASSERT(!parser1.error());
    MORDOR_ASSERT(parser1.complete());
    MORDOR_TEST_ASSERT_EQUAL(request1.requestLine.method, GET);
    MORDOR_TEST_ASSERT_EQUAL(request1.requestLine.uri, URI("/blah1"));
    MORDOR_TEST_ASSERT_EQUAL(request1.requestLine.ver, Version(1, 0));

    Request request2;
    RequestParser parser2(request2);

    parser2.run(bufferedStream);
    MORDOR_ASSERT(!parser2.error());
    MORDOR_ASSERT(parser2.complete());
    MORDOR_TEST_ASSERT_EQUAL(request2.requestLine.method, PUT);
    MORDOR_TEST_ASSERT_EQUAL(request2.requestLine.uri, URI("/blah2"));
    MORDOR_TEST_ASSERT_EQUAL(request2.requestLine.ver, Version(1, 0));
}

MORDOR_UNITTEST(HTTP, ifMatchHeader)
{
    Request request;
    RequestParser parser(request);
    std::set<ETag>::iterator it;
    std::ostringstream os;

    parser.run("GET / HTTP/1.0\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.method, GET);
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.uri, URI("/"));
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.ver, Version(1, 0));
    MORDOR_TEST_ASSERT(request.request.ifMatch.empty());
    os << request;
    MORDOR_TEST_ASSERT_EQUAL(os.str(),
        "GET / HTTP/1.0\r\n"
        "\r\n");

    request = Request();
    parser.run("GET / HTTP/1.0\r\n"
               "If-Match: *\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.method, GET);
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.uri, URI("/"));
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.ver, Version(1, 0));
    MORDOR_TEST_ASSERT_EQUAL(request.request.ifMatch.size(), 1u);
    it = request.request.ifMatch.begin();
    MORDOR_TEST_ASSERT(it->unspecified);
    MORDOR_TEST_ASSERT(!it->weak);
    MORDOR_TEST_ASSERT_EQUAL(it->value, "");
    os.str("");
    os << request;
    MORDOR_TEST_ASSERT_EQUAL(os.str(),
        "GET / HTTP/1.0\r\n"
        "If-Match: *\r\n"
        "\r\n");

    request = Request();
    parser.run("GET / HTTP/1.0\r\n"
               "If-Match: \"\", W/\"other\", \"something\"\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.method, GET);
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.uri, URI("/"));
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.ver, Version(1, 0));
    MORDOR_TEST_ASSERT_EQUAL(request.request.ifMatch.size(), 3u);
    it = request.request.ifMatch.begin();
    MORDOR_TEST_ASSERT(!it->unspecified);
    MORDOR_TEST_ASSERT(!it->weak);
    MORDOR_TEST_ASSERT_EQUAL(it->value, "");
    ++it;
    MORDOR_TEST_ASSERT(!it->unspecified);
    MORDOR_TEST_ASSERT(!it->weak);
    MORDOR_TEST_ASSERT_EQUAL(it->value, "something");
    ++it;
    MORDOR_TEST_ASSERT(!it->unspecified);
    MORDOR_TEST_ASSERT(it->weak);
    MORDOR_TEST_ASSERT_EQUAL(it->value, "other");
    os.str("");
    os << request;
    MORDOR_TEST_ASSERT_EQUAL(os.str(),
        "GET / HTTP/1.0\r\n"
        "If-Match: \"\", \"something\", W/\"other\"\r\n"
        "\r\n");

    // * is only allowed once
    request = Request();
    parser.run("GET / HTTP/1.0\r\n"
               "If-Match: \"first\", \"second\", *\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.method, GET);
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.uri, URI("/"));
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.ver, Version(1, 0));
    MORDOR_TEST_ASSERT_EQUAL(request.request.ifMatch.size(), 2u);
    it = request.request.ifMatch.begin();
    MORDOR_TEST_ASSERT(!it->unspecified);
    MORDOR_TEST_ASSERT(!it->weak);
    MORDOR_TEST_ASSERT_EQUAL(it->value, "first");
    ++it;
    MORDOR_TEST_ASSERT(!it->unspecified);
    MORDOR_TEST_ASSERT(!it->weak);
    MORDOR_TEST_ASSERT_EQUAL(it->value, "second");
    MORDOR_TEST_ASSERT(request.entity.extension.find("If-Match") != request.entity.extension.end());
    MORDOR_TEST_ASSERT_EQUAL(request.entity.extension["If-Match"], "\"first\", \"second\", *");
}

MORDOR_UNITTEST(HTTP, upgradeHeader)
{
    Request request;
    RequestParser parser(request);
    ProductList::iterator it;
    std::ostringstream os;

    parser.run("GET / HTTP/1.0\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.method, GET);
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.uri, URI("/"));
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.ver, Version(1, 0));
    MORDOR_TEST_ASSERT(request.general.upgrade.empty());
    os << request;
    MORDOR_TEST_ASSERT_EQUAL(os.str(),
        "GET / HTTP/1.0\r\n"
        "\r\n");

    request = Request();
    parser.run("GET / HTTP/1.0\r\n"
               "Upgrade: HTTP/2.0, SHTTP/1.3, IRC/6.9, RTA/x11\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.method, GET);
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.uri, URI("/"));
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.ver, Version(1, 0));
    MORDOR_TEST_ASSERT_EQUAL(request.general.upgrade.size(), 4u);
    it = request.general.upgrade.begin();
    MORDOR_TEST_ASSERT_EQUAL(it->product, "HTTP");
    MORDOR_TEST_ASSERT_EQUAL(it->version, "2.0");
    ++it;
    MORDOR_TEST_ASSERT_EQUAL(it->product, "SHTTP");
    MORDOR_TEST_ASSERT_EQUAL(it->version, "1.3");
    ++it;
    MORDOR_TEST_ASSERT_EQUAL(it->product, "IRC");
    MORDOR_TEST_ASSERT_EQUAL(it->version, "6.9");
    ++it;
    MORDOR_TEST_ASSERT_EQUAL(it->product, "RTA");
    MORDOR_TEST_ASSERT_EQUAL(it->version, "x11");
    ++it;
    os.str("");
    os << request;
    MORDOR_TEST_ASSERT_EQUAL(os.str(),
        "GET / HTTP/1.0\r\n"
        "Upgrade: HTTP/2.0, SHTTP/1.3, IRC/6.9, RTA/x11\r\n"
        "\r\n");

    request = Request();
    parser.run("GET / HTTP/1.0\r\n"
               "Upgrade: HTTP/2.0, SHTTP/1.<3, IRC/6.9, RTA/x11\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.method, GET);
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.uri, URI("/"));
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.ver, Version(1, 0));
    MORDOR_TEST_ASSERT_EQUAL(request.general.upgrade.size(), 1u);
    it = request.general.upgrade.begin();
    MORDOR_TEST_ASSERT_EQUAL(it->product, "HTTP");
    MORDOR_TEST_ASSERT_EQUAL(it->version, "2.0");
    MORDOR_TEST_ASSERT(request.entity.extension.find("Upgrade") !=
        request.entity.extension.end());
    MORDOR_TEST_ASSERT_EQUAL(request.entity.extension["Upgrade"],
        "HTTP/2.0, SHTTP/1.<3, IRC/6.9, RTA/x11");
}

MORDOR_UNITTEST(HTTP, serverHeader)
{
    Response response;
    ResponseParser parser(response);
    ProductAndCommentList::iterator it;
    std::ostringstream os;

    parser.run("HTTP/1.0 200 OK\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(response.status.ver, Version(1, 0));
    MORDOR_TEST_ASSERT_EQUAL(response.status.status, OK);
    MORDOR_TEST_ASSERT_EQUAL(response.status.reason, "OK");
    MORDOR_TEST_ASSERT(response.general.upgrade.empty());
    os << response;
    MORDOR_TEST_ASSERT_EQUAL(os.str(),
        "HTTP/1.0 200 OK\r\n"
        "\r\n");

    response = Response();
    parser.run("HTTP/1.0 200 OK\r\n"
               "Server: Apache/2.2.3 (Debian) mod_fastcgi/2.4.2 mod_python/3.2.10 Python/2.4.4 PHP/4.4.4-8+etch6\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(response.status.ver, Version(1, 0));
    MORDOR_TEST_ASSERT_EQUAL(response.status.status, OK);
    MORDOR_TEST_ASSERT_EQUAL(response.status.reason, "OK");
    MORDOR_TEST_ASSERT_EQUAL(response.response.server.size(), 6u);
    it = response.response.server.begin();
    MORDOR_TEST_ASSERT_EQUAL(boost::get<Product>(*it).product, "Apache");
    MORDOR_TEST_ASSERT_EQUAL(boost::get<Product>(*it).version, "2.2.3");
    ++it;
    MORDOR_TEST_ASSERT_EQUAL(boost::get<std::string>(*it), "Debian");
    ++it;
    MORDOR_TEST_ASSERT_EQUAL(boost::get<Product>(*it).product, "mod_fastcgi");
    MORDOR_TEST_ASSERT_EQUAL(boost::get<Product>(*it).version, "2.4.2");
    ++it;
    MORDOR_TEST_ASSERT_EQUAL(boost::get<Product>(*it).product, "mod_python");
    MORDOR_TEST_ASSERT_EQUAL(boost::get<Product>(*it).version, "3.2.10");
    ++it;
    MORDOR_TEST_ASSERT_EQUAL(boost::get<Product>(*it).product, "Python");
    MORDOR_TEST_ASSERT_EQUAL(boost::get<Product>(*it).version, "2.4.4");
    ++it;
    MORDOR_TEST_ASSERT_EQUAL(boost::get<Product>(*it).product, "PHP");
    MORDOR_TEST_ASSERT_EQUAL(boost::get<Product>(*it).version, "4.4.4-8+etch6");
    ++it;
    os.str("");
    os << response;
    MORDOR_TEST_ASSERT_EQUAL(os.str(),
        "HTTP/1.0 200 OK\r\n"
        "Server: Apache/2.2.3 (Debian) mod_fastcgi/2.4.2 mod_python/3.2.10 Python/2.4.4 PHP/4.4.4-8+etch6\r\n"
        "\r\n");

    response = Response();
    parser.run("HTTP/1.0 200 OK\r\n"
        "Server: (Some (nested) (comments ((are)) crazy))\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(response.status.ver, Version(1, 0));
    MORDOR_TEST_ASSERT_EQUAL(response.status.status, OK);
    MORDOR_TEST_ASSERT_EQUAL(response.status.reason, "OK");
    MORDOR_TEST_ASSERT_EQUAL(response.response.server.size(), 1u);
    it = response.response.server.begin();
    MORDOR_TEST_ASSERT_EQUAL(boost::get<std::string>(*it), "Some (nested) (comments ((are)) crazy)");
    os.str("");
    os << response;
    MORDOR_TEST_ASSERT_EQUAL(os.str(),
        "HTTP/1.0 200 OK\r\n"
        "Server: (Some (nested) (comments ((are)) crazy))\r\n"
        "\r\n");

    response = Response();
    parser.run("HTTP/1.0 405 Method Not Allowed\r\n"
        "Allow: GET, HEAD, PUT\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(response.status.ver, Version(1, 0));
    MORDOR_TEST_ASSERT_EQUAL(response.status.status, METHOD_NOT_ALLOWED);
    MORDOR_TEST_ASSERT_EQUAL(response.status.reason, "Method Not Allowed");
    MORDOR_TEST_ASSERT_EQUAL(response.entity.allow.size(), (size_t)3);
    MORDOR_TEST_ASSERT_EQUAL(response.entity.allow[0], "GET");
    MORDOR_TEST_ASSERT_EQUAL(response.entity.allow[1], "HEAD");
    MORDOR_TEST_ASSERT_EQUAL(response.entity.allow[2], "PUT");
    os.str("");
    os << response;
    MORDOR_TEST_ASSERT_EQUAL(os.str(),
        "HTTP/1.0 405 Method Not Allowed\r\n"
        "Allow: GET, HEAD, PUT\r\n"
        "\r\n");
}

MORDOR_UNITTEST(HTTP, teHeader)
{
    Request request;
    RequestParser parser(request);
    std::ostringstream os;

    parser.run("GET / HTTP/1.0\r\n"
               "TE: deflate, chunked;q=0, x-gzip;q=0.050\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(request.request.te.size(), 3u);
    AcceptListWithParameters::iterator it = request.request.te.begin();
    MORDOR_TEST_ASSERT_EQUAL(it->value, "deflate");
    MORDOR_TEST_ASSERT_EQUAL(it->qvalue, ~0u);
    ++it;
    MORDOR_TEST_ASSERT_EQUAL(it->value, "chunked");
    MORDOR_TEST_ASSERT_EQUAL(it->qvalue, 0u);
    ++it;
    MORDOR_TEST_ASSERT_EQUAL(it->value, "x-gzip");
    MORDOR_TEST_ASSERT_EQUAL(it->qvalue, 50u);

    os << request;
    MORDOR_TEST_ASSERT_EQUAL(os.str(),
        "GET / HTTP/1.0\r\n"
        "TE: deflate, chunked;q=0, x-gzip;q=0.05\r\n"
        "\r\n");
}

MORDOR_UNITTEST(HTTP, trailer)
{
    EntityHeaders trailer;
    TrailerParser parser(trailer);

    parser.run("Content-Type: text/plain\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(trailer.contentType.type, "text");
    MORDOR_TEST_ASSERT_EQUAL(trailer.contentType.subtype, "plain");
}

MORDOR_UNITTEST(HTTP, rangeHeader)
{
    Request request;
    RequestParser parser(request);

    parser.run("GET / HTTP/1.1\r\n"
               "Range: bytes=0-499, 500-999, -500, 9500-, 0-0,-1\r\n"
               " ,500-600\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.method, GET);
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.uri, URI("/"));
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.ver, Version(1, 1));
    MORDOR_TEST_ASSERT_EQUAL(request.request.range.size(), 7u);
    RangeSet::const_iterator it = request.request.range.begin();
    MORDOR_TEST_ASSERT_EQUAL(it->first, 0u);
    MORDOR_TEST_ASSERT_EQUAL(it->second, 499u);
    ++it;
    MORDOR_TEST_ASSERT_EQUAL(it->first, 500u);
    MORDOR_TEST_ASSERT_EQUAL(it->second, 999u);
    ++it;
    MORDOR_TEST_ASSERT_EQUAL(it->first, ~0ull);
    MORDOR_TEST_ASSERT_EQUAL(it->second, 500u);
    ++it;
    MORDOR_TEST_ASSERT_EQUAL(it->first, 9500u);
    MORDOR_TEST_ASSERT_EQUAL(it->second, ~0ull);
    ++it;
    MORDOR_TEST_ASSERT_EQUAL(it->first, 0u);
    MORDOR_TEST_ASSERT_EQUAL(it->second, 0u);
    ++it;
    MORDOR_TEST_ASSERT_EQUAL(it->first, ~0ull);
    MORDOR_TEST_ASSERT_EQUAL(it->second, 1u);
    ++it;
    MORDOR_TEST_ASSERT_EQUAL(it->first, 500u);
    MORDOR_TEST_ASSERT_EQUAL(it->second, 600u);

    {
        RequestParser parser(request, true);
        MORDOR_TEST_ASSERT_EXCEPTION(
            parser.run("GET / HTTP/1.1\r\n"
                       "Range: bites=0-499, 500-999, -500, 9500-, 0-0,-1\r\n"
                       " ,500-600\r\n"
                       "\r\n"),
            BadFieldValueException);
        MORDOR_TEST_ASSERT_EXCEPTION(
            parser.run("GET / HTTP/1.1\r\n"
                       "Range: bytes=0-Agg\r\n"
                       "\r\n"),
            BadFieldValueException);
    }
}

MORDOR_UNITTEST(HTTP, contentMD5Header)
{
    Request request;
    RequestParser parser(request);

    const std::string md5("p5/WA/oEr30qrEEl21PAqw==");
    const std::string formatter("Content-MD5: %1%\r\n");
    parser.run(std::string("CONNECT mozy.com:443 HTTP/1.1\r\n") +
               (boost::format(formatter) % md5).str() +
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(request.entity.contentMD5, md5);

    // check output
    MORDOR_TEST_ASSERT_EQUAL(boost::lexical_cast<std::string>(request.entity),
                             (boost::format(formatter) % md5).str());

    // invalid MD5, non-strict
    {
        Request request;
        RequestParser parser(request, false);
        parser.run("CONNECT mozy.com:443 HTTP/1.1\r\n"
                   "Content-MD5: invalid\r\n"
                   "\r\n");
        MORDOR_TEST_ASSERT(!parser.error());
        MORDOR_TEST_ASSERT(parser.complete());
        MORDOR_TEST_ASSERT_EQUAL(request.entity.contentMD5, "");
    }

    // invalid MD5, strict
    {
        Request request;
        RequestParser parser(request, true);
        MORDOR_TEST_ASSERT_EXCEPTION(
            parser.run("CONNECT mozy.com:443 HTTP/1.1\r\n"
                       "Content-MD5: invalid\r\n"
                       "\r\n"),
            BadFieldValueException);
    }
}

MORDOR_UNITTEST(HTTP, contentTypeHeader)
{
    Response response;
    ResponseParser parser(response);

    parser.run("HTTP/1.1 200 OK\r\n"
               "Content-Type: text/plain\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(response.status.ver, Version(1, 1));
    MORDOR_TEST_ASSERT_EQUAL(response.status.status, OK);
    MORDOR_TEST_ASSERT_EQUAL(response.status.reason, "OK");
    MORDOR_TEST_ASSERT_EQUAL(response.entity.contentType.type, "text");
    MORDOR_TEST_ASSERT_EQUAL(response.entity.contentType.subtype, "plain");
}

MORDOR_UNITTEST(HTTP, eTagHeader)
{
    Response response;
    ResponseParser parser(response);
    std::ostringstream os;

    parser.run("HTTP/1.1 200 OK\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(response.status.ver, Version(1, 1));
    MORDOR_TEST_ASSERT_EQUAL(response.status.status, OK);
    MORDOR_TEST_ASSERT_EQUAL(response.status.reason, "OK");
    MORDOR_TEST_ASSERT(response.response.eTag.unspecified);
    MORDOR_TEST_ASSERT(!response.response.eTag.weak);
    MORDOR_TEST_ASSERT_EQUAL(response.response.eTag.value, "");
    os << response;
    MORDOR_TEST_ASSERT_EQUAL(os.str(),
        "HTTP/1.1 200 OK\r\n"
        "\r\n");

    response = Response();
    parser.run("HTTP/1.1 200 OK\r\n"
               "ETag: \"\"\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(response.status.ver, Version(1, 1));
    MORDOR_TEST_ASSERT_EQUAL(response.status.status, OK);
    MORDOR_TEST_ASSERT_EQUAL(response.status.reason, "OK");
    MORDOR_TEST_ASSERT(!response.response.eTag.unspecified);
    MORDOR_TEST_ASSERT(!response.response.eTag.weak);
    MORDOR_TEST_ASSERT_EQUAL(response.response.eTag.value, "");
    os.str("");
    os << response;
    MORDOR_TEST_ASSERT_EQUAL(os.str(),
        "HTTP/1.1 200 OK\r\n"
        "ETag: \"\"\r\n"
        "\r\n");

    response = Response();
    parser.run("HTTP/1.1 200 OK\r\n"
               "ETag: W/\"\"\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(response.status.ver, Version(1, 1));
    MORDOR_TEST_ASSERT_EQUAL(response.status.status, OK);
    MORDOR_TEST_ASSERT_EQUAL(response.status.reason, "OK");
    MORDOR_TEST_ASSERT(!response.response.eTag.unspecified);
    MORDOR_TEST_ASSERT(response.response.eTag.weak);
    MORDOR_TEST_ASSERT_EQUAL(response.response.eTag.value, "");

    response = Response();
    parser.run("HTTP/1.1 200 OK\r\n"
               "ETag: \"sometag\"\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(response.status.ver, Version(1, 1));
    MORDOR_TEST_ASSERT_EQUAL(response.status.status, OK);
    MORDOR_TEST_ASSERT_EQUAL(response.status.reason, "OK");
    MORDOR_TEST_ASSERT(!response.response.eTag.unspecified);
    MORDOR_TEST_ASSERT(!response.response.eTag.weak);
    MORDOR_TEST_ASSERT_EQUAL(response.response.eTag.value, "sometag");

    response = Response();
    parser.run("HTTP/1.1 200 OK\r\n"
               "ETag: *\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(response.status.ver, Version(1, 1));
    MORDOR_TEST_ASSERT_EQUAL(response.status.status, OK);
    MORDOR_TEST_ASSERT_EQUAL(response.status.reason, "OK");
    MORDOR_TEST_ASSERT(response.response.eTag.unspecified);
    MORDOR_TEST_ASSERT(response.entity.extension.find("ETag") != response.entity.extension.end());
    MORDOR_TEST_ASSERT_EQUAL(response.entity.extension["ETag"], "*");

    response = Response();
    parser.run("HTTP/1.1 200 OK\r\n"
               "ETag: token\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(response.status.ver, Version(1, 1));
    MORDOR_TEST_ASSERT_EQUAL(response.status.status, OK);
    MORDOR_TEST_ASSERT_EQUAL(response.status.reason, "OK");
    MORDOR_TEST_ASSERT(response.response.eTag.unspecified);
    MORDOR_TEST_ASSERT(response.entity.extension.find("ETag") != response.entity.extension.end());
    MORDOR_TEST_ASSERT_EQUAL(response.entity.extension["ETag"], "token");
}

MORDOR_UNITTEST(HTTP, locationHeader)
{
    Response response;
    ResponseParser parser(response);

    parser.run("HTTP/1.1 307 OK\r\n"
               "Location: /partialObjects/"
        "49ZtbkNPlEEi8T+sQLb5mh9zm1DcyaaRoyHUOC9sEfaKIgLh+eKZNUrqR+j3Iybhx321iz"
        "y3J+Mw7gZmIlVcZrP0qHNDxuEQHMUHLqxhXoXcN18+x4XedNLqc8KhnJtHLXndcKMJu5Cg"
        "xp2BI9NXDDDuBmYiVVxms/Soc0PG4RAcxQcurGFehSY0Wf0fG5eWquA0b0hozVjE4xxyAF"
        "TkSU39Hl3XcsUUMO4GZiJVXGaz9KhzQ8bhEBzFBy6sYV6F9718Fox0OiJ3PqBvo2gr352W"
        "vZBqmEeUV1n0CkcClc0w7gZmIlVcZrP0qHNDxuEQHMUHLqxhXoWapmDUfha0WO9SjTUn4F"
        "Jeht8Gjdy6mYpDqvUbB+3OoDDuBmYiVVxms/Soc0PG4RAcxQcurGFehcefjKkVeAR2HShU"
        "2UpBh5g/89ZP9czSJ8qKSKCPGyHWMO4GZiJVXGaz9KhzQ8bhEBzFBy6sYV6FAig0fJADqV"
        "eInu5RU/pgEXJlZ1MBce/F+rv7MI3g5jgw7gZmIlVcZrP0qHNDxuEQHMUHLqxhXoW4GIxe"
        "C1lnhkTtrAv3jhk17r3ZwL8Fq7CvpUHeAQl/JTDuBmYiVVxms/Soc0PG4RAcxQcurGFehc"
        "s4fMw9uBwTihHQAPFbcyDTjZtTMGlaovGaP6xe1H1TMO4GZiJVXGaz9KhzQ8bhEBzFBy6s"
        "YV6FFAhiH0dzP8E0IRZP+oxeL2JkfxiO5v8r7eWnYtMY8d4w7gZmIlVcZrP0qHNDxuEQHM"
        "UHLqxhXoUgoQ1pQreM2tYMR9QaJ7CsSOSJs+Qi5KIzV50DBUYLDjDuBmYiVVxms/Soc0PG"
        "4RAcxQcurGFehdeUg8nHldHqihIknc3OP/QRtBawAyEFY4p0RKlRxnA0MO4GZiJVXGaz9K"
        "hzQ8bhEBzFBy6sYV6FbRY5v48No3N72yRSA9JiYPhS/YTYcUFz\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(response.status.ver, Version(1, 1));
    MORDOR_TEST_ASSERT_EQUAL(response.status.status, TEMPORARY_REDIRECT);
    MORDOR_TEST_ASSERT_EQUAL(response.status.reason, "OK");
    MORDOR_TEST_ASSERT_EQUAL(response.response.location, "/partialObjects/"
        "49ZtbkNPlEEi8T+sQLb5mh9zm1DcyaaRoyHUOC9sEfaKIgLh+eKZNUrqR+j3Iybhx321iz"
        "y3J+Mw7gZmIlVcZrP0qHNDxuEQHMUHLqxhXoXcN18+x4XedNLqc8KhnJtHLXndcKMJu5Cg"
        "xp2BI9NXDDDuBmYiVVxms/Soc0PG4RAcxQcurGFehSY0Wf0fG5eWquA0b0hozVjE4xxyAF"
        "TkSU39Hl3XcsUUMO4GZiJVXGaz9KhzQ8bhEBzFBy6sYV6F9718Fox0OiJ3PqBvo2gr352W"
        "vZBqmEeUV1n0CkcClc0w7gZmIlVcZrP0qHNDxuEQHMUHLqxhXoWapmDUfha0WO9SjTUn4F"
        "Jeht8Gjdy6mYpDqvUbB+3OoDDuBmYiVVxms/Soc0PG4RAcxQcurGFehcefjKkVeAR2HShU"
        "2UpBh5g/89ZP9czSJ8qKSKCPGyHWMO4GZiJVXGaz9KhzQ8bhEBzFBy6sYV6FAig0fJADqV"
        "eInu5RU/pgEXJlZ1MBce/F+rv7MI3g5jgw7gZmIlVcZrP0qHNDxuEQHMUHLqxhXoW4GIxe"
        "C1lnhkTtrAv3jhk17r3ZwL8Fq7CvpUHeAQl/JTDuBmYiVVxms/Soc0PG4RAcxQcurGFehc"
        "s4fMw9uBwTihHQAPFbcyDTjZtTMGlaovGaP6xe1H1TMO4GZiJVXGaz9KhzQ8bhEBzFBy6s"
        "YV6FFAhiH0dzP8E0IRZP+oxeL2JkfxiO5v8r7eWnYtMY8d4w7gZmIlVcZrP0qHNDxuEQHM"
        "UHLqxhXoUgoQ1pQreM2tYMR9QaJ7CsSOSJs+Qi5KIzV50DBUYLDjDuBmYiVVxms/Soc0PG"
        "4RAcxQcurGFehdeUg8nHldHqihIknc3OP/QRtBawAyEFY4p0RKlRxnA0MO4GZiJVXGaz9K"
        "hzQ8bhEBzFBy6sYV6FbRY5v48No3N72yRSA9JiYPhS/YTYcUFz");
}

MORDOR_UNITTEST(HTTP, responseWithInvalidStandardHeader)
{
    Response response;
    ResponseParser parser(response);

    parser.run("HTTP/1.1 200 OK\r\n"
               "Expires: -1\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(response.status.status, OK);
    MORDOR_TEST_ASSERT_EQUAL(response.status.ver, Version(1, 1));
    MORDOR_TEST_ASSERT(response.entity.expires.is_not_a_date_time());
    MORDOR_TEST_ASSERT(response.entity.extension.find("Expires") !=
        response.entity.extension.end());
    MORDOR_TEST_ASSERT_EQUAL(response.entity.extension["Expires"], "-1");

    {
        ResponseParser parser(response, true);
        MORDOR_TEST_ASSERT_EXCEPTION(
            parser.run("HTTP/1.1 200 OK\r\n"
                       "Expires: -1\r\n"
                       "\r\n"),
            BadFieldValueException);
    }

}

MORDOR_UNITTEST(HTTP, versionComparison)
{
    Version ver10(1, 0), ver11(1, 1);
    MORDOR_TEST_ASSERT(ver10 == ver10);
    MORDOR_TEST_ASSERT(ver11 == ver11);
    MORDOR_TEST_ASSERT(ver10 != ver11);
    MORDOR_TEST_ASSERT(ver10 <= ver11);
    MORDOR_TEST_ASSERT(ver10 < ver11);
    MORDOR_TEST_ASSERT(ver11 >= ver10);
    MORDOR_TEST_ASSERT(ver11 > ver10);
    MORDOR_TEST_ASSERT(ver10 <= ver10);
    MORDOR_TEST_ASSERT(ver10 >= ver10);
}

static void testQuotingRoundTrip(const std::string &unquoted, const std::string &quoted)
{
    MORDOR_TEST_ASSERT_EQUAL(quote(unquoted), quoted);
    MORDOR_TEST_ASSERT_EQUAL(unquote(quoted), unquoted);
}

MORDOR_UNITTEST(HTTP, quoting)
{
    // Empty string needs to be quoted (otherwise ambiguous)
    testQuotingRoundTrip("", "\"\"");
    // Tokens do not need to be quoted
    testQuotingRoundTrip("token", "token");
    testQuotingRoundTrip("tom", "tom");
    testQuotingRoundTrip("token.non-separator+chars", "token.non-separator+chars");
    // Whitespace is quoted, but not escaped
    testQuotingRoundTrip("multiple words", "\"multiple words\"");
    testQuotingRoundTrip("\tlotsa  whitespace\t",
        "\"\tlotsa  whitespace\t\"");
    // Backslashes and quotes are escaped
    testQuotingRoundTrip("\"", "\"\\\"\"");
    testQuotingRoundTrip("\\", "\"\\\\\"");
    testQuotingRoundTrip("co\\dy", "\"co\\\\dy\"");
    testQuotingRoundTrip("multiple\\ escape\" sequences\\  ",
        "\"multiple\\\\ escape\\\" sequences\\\\  \"");
    // Separators are quoted, but not escaped
    testQuotingRoundTrip("weird < chars >", "\"weird < chars >\"");
    // CTL gets escaped
    testQuotingRoundTrip(std::string("\0", 1), std::string("\"\\\0\"", 4));
    testQuotingRoundTrip("\x7f", "\"\\\x7f\"");
    // > 127 is quoted, but not escaped
    testQuotingRoundTrip("\x80", "\"\x80\"");

    // ETag even quotes tokens
    MORDOR_TEST_ASSERT_EQUAL(quote("token", true), "\"token\"");
}

static void testCommentRoundTrip(const std::string &unquoted, const std::string &quoted)
{
    MORDOR_TEST_ASSERT_EQUAL(quote(unquoted, true, true), quoted);
    MORDOR_TEST_ASSERT_EQUAL(unquote(quoted), unquoted);
}

MORDOR_UNITTEST(HTTP, comments)
{
    // Empty string needs to be quoted (otherwise ambiguous)
    testCommentRoundTrip("", "()");
    // Tokens are quoted
    testCommentRoundTrip("token", "(token)");
    testCommentRoundTrip("token.non-separator+chars", "(token.non-separator+chars)");
    // Whitespace is quoted, but not escaped
    testCommentRoundTrip("multiple words", "(multiple words)");
    testCommentRoundTrip("\tlotsa  whitespace\t",
        "(\tlotsa  whitespace\t)");
    // Backslashes are escaped
    testCommentRoundTrip("\\", "(\\\\)");
    testCommentRoundTrip("co\\dy", "(co\\\\dy)");
    // Quotes are not escaped
    testCommentRoundTrip("\"", "(\")");
    // Separators are quoted, but not escaped
    testCommentRoundTrip("weird < chars >", "(weird < chars >)");
    // CTL gets escaped
    testCommentRoundTrip(std::string("\0", 1), std::string("(\\\0)", 4));
    testCommentRoundTrip("\x7f", "(\\\x7f)");
    // > 127 is quoted, but not escaped
    testCommentRoundTrip("\x80", "(\x80)");
    // Parens are not escaped, if they're matched
    testCommentRoundTrip("()", "(())");
    testCommentRoundTrip("(", "(\\()");
    testCommentRoundTrip(")", "(\\))");
    testCommentRoundTrip("(()", "((\\())");
    testCommentRoundTrip("())", "(()\\))");
    testCommentRoundTrip(")(", "(\\)\\()");
    testCommentRoundTrip("(()))()", "((())\\)())");
}

MORDOR_UNITTEST(HTTP, CONNECT)
{
    Request request;
    RequestParser parser(request);

    parser.run("CONNECT http:80 HTTP/1.0\r\n\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.method, CONNECT);
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.uri, "//http:80");
    MORDOR_TEST_ASSERT_EQUAL(request.requestLine.ver, Version(1, 0));

    MORDOR_TEST_ASSERT_EQUAL(boost::lexical_cast<std::string>(request),
        "CONNECT http:80 HTTP/1.0\r\n\r\n");
}

MORDOR_UNITTEST(HTTP, ETagComparison)
{
    // Strong comparisons
    MORDOR_TEST_ASSERT(ETag("abc").strongCompare(ETag("abc")));
    MORDOR_TEST_ASSERT(!ETag("abc").strongCompare(ETag("abc", true)));
    MORDOR_TEST_ASSERT(!ETag("abc", true).strongCompare(ETag("abc", true)));
    MORDOR_TEST_ASSERT(ETag().strongCompare(ETag()));
    MORDOR_TEST_ASSERT(!ETag("abc").strongCompare(ETag("xyz")));
    MORDOR_TEST_ASSERT(!ETag("abc").strongCompare(ETag("xyz", true)));
    MORDOR_TEST_ASSERT(!ETag("abc", true).strongCompare(ETag("xyz", true)));

    // Weak comparisons
    MORDOR_TEST_ASSERT(ETag("abc").weakCompare(ETag("abc")));
    MORDOR_TEST_ASSERT(ETag("abc").weakCompare(ETag("abc", true)));
    MORDOR_TEST_ASSERT(ETag("abc", true).weakCompare(ETag("abc", true)));
    MORDOR_TEST_ASSERT(ETag().weakCompare(ETag()));
    MORDOR_TEST_ASSERT(!ETag("abc").weakCompare(ETag("xyz")));
    MORDOR_TEST_ASSERT(!ETag("abc").weakCompare(ETag("xyz", true)));
    MORDOR_TEST_ASSERT(!ETag("abc", true).weakCompare(ETag("xyz", true)));

    // Exact comparisons
    MORDOR_TEST_ASSERT_EQUAL(ETag("abc"), ETag("abc"));
    MORDOR_TEST_ASSERT_NOT_EQUAL(ETag("abc"), ETag("abc", true));
    MORDOR_TEST_ASSERT_EQUAL(ETag("abc", true), ETag("abc", true));
    MORDOR_TEST_ASSERT_EQUAL(ETag(), ETag());
    MORDOR_TEST_ASSERT_NOT_EQUAL(ETag("abc"), ETag("xyz"));
    MORDOR_TEST_ASSERT_NOT_EQUAL(ETag("abc"), ETag("xyz", true));
    MORDOR_TEST_ASSERT_NOT_EQUAL(ETag("abc", true), ETag("xyz", true));
}

MORDOR_UNITTEST(HTTP, doubleSlashRequest)
{
    Request request;
    RequestParser parser(request);

    parser.run("GET //hi HTTP/1.0\r\n\r\n");
    MORDOR_TEST_ASSERT(parser.error());

#ifndef NDEBUG
    request.requestLine.uri.path = "//hi";
    MORDOR_TEST_ASSERT_ASSERTED(boost::lexical_cast<std::string>(request));
#endif
}

MORDOR_UNITTEST(HTTP, proxyAuthorizationHeader)
{
    Request request;
    RequestParser parser(request);

    parser.run("CONNECT mozy.com:443 HTTP/1.1\r\n"
        "Connection: Proxy-Connection\r\n"
        "Proxy-Connection: Keep-Alive\r\n"
        "Host: mozy.com:443\r\n"
        "Proxy-Authorization: NTLM TlRMTVNTUAABAAAAt4II4gAAAAAAAAAAAAAAAAAAAAAGAbAdAAAADw==\r\n"
        "\r\n");

    MORDOR_TEST_ASSERT_EQUAL(request.request.proxyAuthorization.scheme, "NTLM");
    MORDOR_TEST_ASSERT_EQUAL(request.request.proxyAuthorization.param, "TlRMTVNTUAABAAAAt4II4gAAAAAAAAAAAAAAAAAAAAAGAbAdAAAADw==");
}

MORDOR_UNITTEST(HTTP, boundary)
{
    std::string type = "multipart";
    std::string subtype = "byterange";
    // let's include '/' in boundary
    std::string boundary = "0123456789/9876543210";

    Response resp;
    resp.status.status = OK;
    resp.status.reason = "OK";
    MediaType& mt = resp.entity.contentType;
    mt.type = type;
    mt.subtype = subtype;
    mt.parameters["boundary"] = boundary;

    // serialize to string
    std::ostringstream os;
    os << resp;
    std::string msg = os.str();

    // deserialize from string
    Response resp2;
    ResponseParser parser(resp2);
    parser.run(msg);

    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MediaType& mt2 = resp2.entity.contentType;
    MORDOR_TEST_ASSERT_EQUAL(mt2.type, type);
    MORDOR_TEST_ASSERT_EQUAL(mt2.subtype, subtype);
    MORDOR_TEST_ASSERT_EQUAL(mt2.parameters.size(), 1u);
    MORDOR_TEST_ASSERT_EQUAL(mt2.parameters["boundary"], boundary);
}

MORDOR_UNITTEST(HTTP, authHeaderOAuth)
{
    Request request;
    RequestParser parser(request);

    parser.run("GET / HTTP/1.1\r\n"
               "Authorization: OAuth oauth_consumer_key=\"It's Bob \\\" pub key\" , oauth_version=1.0\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.final());

    AuthParams & auth = request.request.authorization;
    MORDOR_TEST_ASSERT_EQUAL(auth.scheme, "OAuth");
    MORDOR_TEST_ASSERT_EQUAL(auth.parameters.size(), 2u);

    StringMap::iterator it;
    it = auth.parameters.find("oauth_version");
    MORDOR_TEST_ASSERT(it != auth.parameters.end());
    MORDOR_TEST_ASSERT_EQUAL(it->second, "1.0");

    it = auth.parameters.find("oauth_consumer_key");
    MORDOR_TEST_ASSERT(it != auth.parameters.end());
    MORDOR_TEST_ASSERT_EQUAL(it->second, "It's Bob \" pub key");
}

MORDOR_UNITTEST(HTTP, authHeaderBasic)
{
    Request request;
    RequestParser parser(request);

    parser.run("GET / HTTP/1.1\r\n"
               "Authorization: Basic dXNlcm5hbWU6cGFzc3dvcmQ=\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.final());

    AuthParams & auth = request.request.authorization;
    MORDOR_TEST_ASSERT_EQUAL(auth.scheme, "Basic");
    MORDOR_TEST_ASSERT_EQUAL(auth.param, "dXNlcm5hbWU6cGFzc3dvcmQ=");
    MORDOR_TEST_ASSERT_EQUAL(auth.parameters.size(), 0u);
}

MORDOR_UNITTEST(HTTP, authHeaderAWS)
{
    Request request;
    RequestParser parser(request);

    parser.run("GET / HTTP/1.1\r\n"
               "Authorization: AWS accesskey:c2lnbmF0dXJl\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.final());

    AuthParams & auth = request.request.authorization;
    MORDOR_TEST_ASSERT_EQUAL(auth.scheme, "AWS");
    MORDOR_TEST_ASSERT_EQUAL(auth.param, "accesskey:c2lnbmF0dXJl");
    MORDOR_TEST_ASSERT_EQUAL(auth.parameters.size(), 0u);
}

MORDOR_UNITTEST(HTTP, acceptEncodingHeader)
{
    Request request;
    RequestParser parser(request);
    std::ostringstream os;

    parser.run("GET / HTTP/1.0\r\n"
               "Accept-Encoding: identity, x-ciphertext;q=0.25, x-syzygy;q=0.800\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());

    MORDOR_TEST_ASSERT_EQUAL(request.request.acceptEncoding.size(), 3u);
    AcceptList::iterator it = request.request.acceptEncoding.begin();
    MORDOR_TEST_ASSERT_EQUAL(it->value, "identity");
    MORDOR_TEST_ASSERT_EQUAL(it->qvalue, ~0u);
    ++it;
    MORDOR_TEST_ASSERT_EQUAL(it->value, "x-ciphertext");
    MORDOR_TEST_ASSERT_EQUAL(it->qvalue, 250u);
    ++it;
    MORDOR_TEST_ASSERT_EQUAL(it->value, "x-syzygy");
    MORDOR_TEST_ASSERT_EQUAL(it->qvalue, 800u);

    os << request;
    MORDOR_TEST_ASSERT_EQUAL(os.str(),
        "GET / HTTP/1.0\r\n"
        "Accept-Encoding: identity, x-ciphertext;q=0.25, x-syzygy;q=0.8\r\n"
        "\r\n");
}

MORDOR_UNITTEST(HTTP, preferredAcceptEncoding)
{
    Request request;
    RequestParser parser(request);

    parser.run("GET / HTTP/1.0\r\n"
               "Accept-Encoding: identity,x-ciphertext;q=0.25,x-syzygy;q=0.800,x-none;q=0\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());

    const AcceptList &accept = request.request.acceptEncoding;

    AcceptList available;
    available.push_back(AcceptValue("x-ciphertext", 1000));
    available.push_back(AcceptValue("x-syzygy", 1000));
    available.push_back(AcceptValue("identity", 1000));
    const AcceptValue *encoding = preferred(accept, available);
    MORDOR_TEST_ASSERT(encoding);
    MORDOR_TEST_ASSERT_EQUAL(encoding->value, "identity");

    available.clear();
    available.push_back(AcceptValue("x-none", 1000));
    available.push_back(AcceptValue("x-ciphertext", 500));
    available.push_back(AcceptValue("x-syzygy", 500));
    encoding = preferred(accept, available);
    MORDOR_TEST_ASSERT(encoding);
    MORDOR_TEST_ASSERT_EQUAL(encoding->value, "x-syzygy");

    available.clear();
    available.push_back(AcceptValue("x-you-dont-know", 1000));
    available.push_back(AcceptValue("x-none", 500));
    encoding = preferred(accept, available);
    MORDOR_TEST_ASSERT(!encoding);

    // [RFC2616] if q is not given, it will be the same as claiming "q=1".
    // we should treat these explicit/implicit values in same way.
    parser.run("GET / HTTP/1.0\r\n"
               "Accept-Encoding: plaintext;q=1,identity\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());

    available.clear();
    available.push_back(AcceptValue("plaintext", 1000));
    available.push_back(AcceptValue("identity", 1000));
    encoding = preferred(request.request.acceptEncoding, available);
    MORDOR_TEST_ASSERT(encoding);
    MORDOR_TEST_ASSERT_EQUAL(encoding->value, "plaintext");

    parser.run("GET / HTTP/1.0\r\n"
               "Accept-Encoding: plaintext,identity;q=1\r\n"
               "\r\n");
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());

    available.clear();
    available.push_back(AcceptValue("plaintext", 1000));
    available.push_back(AcceptValue("identity", 1000));
    encoding = preferred(request.request.acceptEncoding, available);
    MORDOR_TEST_ASSERT(encoding);
    MORDOR_TEST_ASSERT_EQUAL(encoding->value, "plaintext");
}
