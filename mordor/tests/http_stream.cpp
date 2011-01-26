// Copyright (c) 2011 - Mozy, Inc.

#include <boost/bind.hpp>

#include "mordor/http/broker.h"
#include "mordor/http/server.h"
#include "mordor/test/test.h"
#include "mordor/streams/buffer.h"
#include "mordor/streams/http.h"
#include "mordor/streams/limited.h"
#include "mordor/streams/random.h"
#include "mordor/util.h"
#include "mordor/workerpool.h"

using namespace Mordor;
using namespace Mordor::HTTP;

namespace {

class Fixture
{
public:
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4355)
#endif
    Fixture()
        : server(boost::bind(&Fixture::dummyServer, this, _1, _2)),
          baseRequestBroker(ConnectionBroker::ptr(&server,
              &nop<ConnectionBroker *>)),
          requestBroker(&baseRequestBroker, &nop<RequestBroker *>),
          sequence(0)
    {}
#ifdef _MSC_VER
#pragma warning(pop)
#pragma warning(disable: 4355)
#endif

private:
    void dummyServer(const URI &uri, ServerRequest::ptr request)
    {
        const RangeSet &range = request->request().request.range;
        const URI &requestUri = request->request().requestLine.uri;
        if (requestUri == "/startNoAdvice") {
            MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
            MORDOR_TEST_ASSERT(range.empty());
        } else if (requestUri == "/startAdvice0") {
            MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
            MORDOR_TEST_ASSERT(range.empty());
        } else if (requestUri == "/startAdvice64K") {
            MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
            MORDOR_TEST_ASSERT_EQUAL(range.size(), 1u);
            MORDOR_TEST_ASSERT_EQUAL(range,
                RangeSet(1u, std::make_pair(0, 65535)));
        } else if (requestUri == "/startAfterSeekNoAdvice") {
            MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
            MORDOR_TEST_ASSERT_EQUAL(range.size(), 1u);
            MORDOR_TEST_ASSERT_EQUAL(range,
                RangeSet(1u, std::make_pair(512 * 1024, ~0ull)));
        } else if (requestUri == "/startAfterSeekAdvice0") {
            MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
            MORDOR_TEST_ASSERT_EQUAL(range.size(), 1u);
            MORDOR_TEST_ASSERT_EQUAL(range,
                RangeSet(1u, std::make_pair(512 * 1024, ~0ull)));
        } else if (requestUri == "/startAfterSeekAdvice64K") {
            MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
            MORDOR_TEST_ASSERT_EQUAL(range.size(), 1u);
            MORDOR_TEST_ASSERT_EQUAL(range,
                RangeSet(1u, std::make_pair(512 * 1024,
                512 * 1024 + 65535)));
        } else if (requestUri == "/readNoAdvice") {
            MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
            MORDOR_TEST_ASSERT(range.empty());
        } else if (requestUri == "/readAdvice0") {
            MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
            MORDOR_TEST_ASSERT_EQUAL(range.size(), 1u);
            MORDOR_TEST_ASSERT_EQUAL(range,
                RangeSet(1u, std::make_pair(0, 4095)));
        } else if (requestUri == "/readAdvice64K") {
            MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
            MORDOR_TEST_ASSERT_EQUAL(range.size(), 1u);
            MORDOR_TEST_ASSERT_EQUAL(range,
                RangeSet(1u, std::make_pair(0, 65535)));
        } else if (requestUri == "/readAdvice4K") {
            MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
            MORDOR_TEST_ASSERT_EQUAL(range.size(), 1u);
            MORDOR_TEST_ASSERT_EQUAL(range,
                RangeSet(1u, std::make_pair(0, 65535)));
        } else if (requestUri == "/read4KAdvice4K") {
            MORDOR_TEST_ASSERT_LESS_THAN_OR_EQUAL(++sequence, 3);
            MORDOR_TEST_ASSERT_EQUAL(range.size(), 1u);
            MORDOR_TEST_ASSERT_EQUAL(range,
                RangeSet(1u, std::make_pair(4096 * (sequence - 1),
                    4096 * sequence - 1)));
        } else if (requestUri == "/adviceStillGetsEOF") {
            MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
            MORDOR_TEST_ASSERT_EQUAL(range.size(), 1u);
            MORDOR_TEST_ASSERT_EQUAL(range,
                RangeSet(1u, std::make_pair(1024 * 1024 - 4096,
                    1024 * 1024 - 1)));
        } else if (requestUri == "/adviceStillGetsEOFStraddle") {
            MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
            MORDOR_TEST_ASSERT_EQUAL(range.size(), 1u);
            MORDOR_TEST_ASSERT_EQUAL(range,
                RangeSet(1u, std::make_pair(1024 * 1024 - 2048,
                    1024 * 1024 + 2048 - 1)));
        } else {
            MORDOR_NOTREACHED();
        }
        RandomStream randomStream;
        LimitedStream limitedStream(Stream::ptr(&randomStream, &nop<Stream *>),
            1024 * 1024);
        try {
            respondStream(request, Stream::ptr(&limitedStream, &nop<Stream *>));
        } catch (BrokenPipeException &) {
            // The tests don't always read the full response; we don't care
        }
    }

private:
    WorkerPool pool;
    MockConnectionBroker server;
    BaseRequestBroker baseRequestBroker;

protected:
    RequestBroker::ptr requestBroker;
    int sequence;
};
}

MORDOR_UNITTEST_FIXTURE(Fixture, HTTPStream, startNoAdvice)
{
    HTTPStream stream("http://localhost/startNoAdvice", requestBroker);
    stream.start();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(Fixture, HTTPStream, startAdvice0)
{
    HTTPStream stream("http://localhost/startAdvice0", requestBroker);
    stream.adviseRead(0);
    stream.start();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(Fixture, HTTPStream, startAdvice64K)
{
    HTTPStream stream("http://localhost/startAdvice64K", requestBroker);
    stream.adviseRead(65536);
    stream.start();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(Fixture, HTTPStream, startAfterSeekNoAdvice)
{
    HTTPStream stream("http://localhost/startAfterSeekNoAdvice", requestBroker);
    stream.seek(512 * 1024);
    stream.start();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(Fixture, HTTPStream, startAfterSeekAdvice0)
{
    HTTPStream stream("http://localhost/startAfterSeekAdvice0", requestBroker);
    stream.adviseRead(0);
    stream.seek(512 * 1024);
    stream.start();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(Fixture, HTTPStream, startAfterSeekAdvice64K)
{
    HTTPStream stream("http://localhost/startAfterSeekAdvice64K", requestBroker);
    stream.adviseRead(65536);
    stream.seek(512 * 1024);
    stream.start();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(Fixture, HTTPStream, readNoAdvice)
{
    HTTPStream stream("http://localhost/readNoAdvice", requestBroker);
    Buffer buffer;
    stream.read(buffer, 4096);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(Fixture, HTTPStream, readAdvice0)
{
    HTTPStream stream("http://localhost/readAdvice0", requestBroker);
    stream.adviseRead(0);
    Buffer buffer;
    stream.read(buffer, 4096);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(Fixture, HTTPStream, readAdvice64K)
{
    HTTPStream stream("http://localhost/readAdvice64K", requestBroker);
    stream.adviseRead(65536);
    Buffer buffer;
    stream.read(buffer, 4096);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(Fixture, HTTPStream, readAdvice4K)
{
    HTTPStream stream("http://localhost/readAdvice4K", requestBroker);
    stream.adviseRead(4096);
    Buffer buffer;
    stream.read(buffer, 65536);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(Fixture, HTTPStream, read4KAdvice4K)
{
    HTTPStream stream("http://localhost/read4KAdvice4K", requestBroker);
    stream.adviseRead(4096);
    Buffer buffer;
    stream.read(buffer, 4096);
    stream.read(buffer, 4096);
    stream.read(buffer, 4096);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 3);
}

MORDOR_UNITTEST_FIXTURE(Fixture, HTTPStream, adviceStillGetsEOF)
{
    HTTPStream stream("http://localhost/adviceStillGetsEOF", requestBroker);
    stream.adviseRead(4096);
    stream.seek(1024 * 1024 - 4096);
    Buffer buffer;
    MORDOR_TEST_ASSERT_EQUAL(stream.read(buffer, 4096), 4096u);
    MORDOR_TEST_ASSERT_EQUAL(stream.read(buffer, 4096), 0u);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(Fixture, HTTPStream, adviceStillGetsEOFStraddle)
{
    HTTPStream stream("http://localhost/adviceStillGetsEOFStraddle", requestBroker);
    stream.adviseRead(4096);
    stream.seek(1024 * 1024 - 2048);
    Buffer buffer;
    MORDOR_TEST_ASSERT_EQUAL(stream.read(buffer, 4096), 2048u);
    MORDOR_TEST_ASSERT_EQUAL(stream.read(buffer, 4096), 0u);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}
