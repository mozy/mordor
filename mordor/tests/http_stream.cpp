// Copyright (c) 2011 - Mozy, Inc.

#include <boost/bind.hpp>

#include "mordor/http/broker.h"
#include "mordor/http/server.h"
#include "mordor/test/test.h"
#include "mordor/streams/buffer.h"
#include "mordor/streams/http.h"
#include "mordor/streams/limited.h"
#include "mordor/streams/null.h"
#include "mordor/streams/random.h"
#include "mordor/streams/transfer.h"
#include "mordor/util.h"
#include "mordor/workerpool.h"

using namespace Mordor;
using namespace Mordor::HTTP;

namespace {
class ReadAdviceFixture
{
public:
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4355)
#endif
    ReadAdviceFixture()
        : server(boost::bind(&ReadAdviceFixture::dummyServer, this, _1, _2)),
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

MORDOR_UNITTEST_FIXTURE(ReadAdviceFixture, HTTPStream, startNoAdvice)
{
    HTTPStream stream("http://localhost/startNoAdvice", requestBroker);
    stream.start();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(ReadAdviceFixture, HTTPStream, startAdvice0)
{
    HTTPStream stream("http://localhost/startAdvice0", requestBroker);
    stream.adviseRead(0);
    stream.start();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(ReadAdviceFixture, HTTPStream, startAdvice64K)
{
    HTTPStream stream("http://localhost/startAdvice64K", requestBroker);
    stream.adviseRead(65536);
    stream.start();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(ReadAdviceFixture, HTTPStream, startAfterSeekNoAdvice)
{
    HTTPStream stream("http://localhost/startAfterSeekNoAdvice", requestBroker);
    stream.seek(512 * 1024);
    stream.start();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(ReadAdviceFixture, HTTPStream, startAfterSeekAdvice0)
{
    HTTPStream stream("http://localhost/startAfterSeekAdvice0", requestBroker);
    stream.adviseRead(0);
    stream.seek(512 * 1024);
    stream.start();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(ReadAdviceFixture, HTTPStream, startAfterSeekAdvice64K)
{
    HTTPStream stream("http://localhost/startAfterSeekAdvice64K", requestBroker);
    stream.adviseRead(65536);
    stream.seek(512 * 1024);
    stream.start();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(ReadAdviceFixture, HTTPStream, readNoAdvice)
{
    HTTPStream stream("http://localhost/readNoAdvice", requestBroker);
    Buffer buffer;
    stream.read(buffer, 4096);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(ReadAdviceFixture, HTTPStream, readAdvice0)
{
    HTTPStream stream("http://localhost/readAdvice0", requestBroker);
    stream.adviseRead(0);
    Buffer buffer;
    stream.read(buffer, 4096);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(ReadAdviceFixture, HTTPStream, readAdvice64K)
{
    HTTPStream stream("http://localhost/readAdvice64K", requestBroker);
    stream.adviseRead(65536);
    Buffer buffer;
    stream.read(buffer, 4096);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(ReadAdviceFixture, HTTPStream, readAdvice4K)
{
    HTTPStream stream("http://localhost/readAdvice4K", requestBroker);
    stream.adviseRead(4096);
    Buffer buffer;
    stream.read(buffer, 65536);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(ReadAdviceFixture, HTTPStream, read4KAdvice4K)
{
    HTTPStream stream("http://localhost/read4KAdvice4K", requestBroker);
    stream.adviseRead(4096);
    Buffer buffer;
    stream.read(buffer, 4096);
    stream.read(buffer, 4096);
    stream.read(buffer, 4096);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 3);
}

MORDOR_UNITTEST_FIXTURE(ReadAdviceFixture, HTTPStream, adviceStillGetsEOF)
{
    HTTPStream stream("http://localhost/adviceStillGetsEOF", requestBroker);
    stream.adviseRead(4096);
    stream.seek(1024 * 1024 - 4096);
    Buffer buffer;
    MORDOR_TEST_ASSERT_EQUAL(stream.read(buffer, 4096), 4096u);
    MORDOR_TEST_ASSERT_EQUAL(stream.read(buffer, 4096), 0u);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(ReadAdviceFixture, HTTPStream, adviceStillGetsEOFStraddle)
{
    HTTPStream stream("http://localhost/adviceStillGetsEOFStraddle", requestBroker);
    stream.adviseRead(4096);
    stream.seek(1024 * 1024 - 2048);
    Buffer buffer;
    MORDOR_TEST_ASSERT_EQUAL(stream.read(buffer, 4096), 2048u);
    MORDOR_TEST_ASSERT_EQUAL(stream.read(buffer, 4096), 0u);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

namespace {
class WriteAdviceFixture
{
public:
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4355)
#endif
    WriteAdviceFixture()
        : server(boost::bind(&WriteAdviceFixture::dummyServer, this, _1, _2)),
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
        const unsigned long long &contentLength =
            request->request().entity.contentLength;
        const ContentRange &contentRange =
            request->request().entity.contentRange;
        const URI &requestUri = request->request().requestLine.uri;
        bool chunked = !request->request().general.transferEncoding.empty();
        if (requestUri == "/writeNoAdvice") {
            MORDOR_TEST_ASSERT_EQUAL(1, ++sequence);
            MORDOR_TEST_ASSERT(chunked);
            MORDOR_TEST_ASSERT_EQUAL(contentLength, ~0ull);
            MORDOR_TEST_ASSERT_EQUAL(contentRange, ContentRange());
            MORDOR_TEST_ASSERT_EQUAL(transferStream(request->requestStream(),
                NullStream::get()), 1024 * 1024ull);
        } else if (requestUri == "/writeSizeAdvice") {
            MORDOR_TEST_ASSERT_EQUAL(1, ++sequence);
            MORDOR_TEST_ASSERT(!chunked);
            MORDOR_TEST_ASSERT_EQUAL(contentLength, 1024 * 1024ull);
            MORDOR_TEST_ASSERT_EQUAL(contentRange, ContentRange());
            MORDOR_TEST_ASSERT_EQUAL(transferStream(request->requestStream(),
                NullStream::get()), 1024 * 1024ull);
        } else if (requestUri == "/writeWriteAdvice") {
            MORDOR_TEST_ASSERT_LESS_THAN_OR_EQUAL(++sequence, 16);
            MORDOR_TEST_ASSERT(chunked);
            MORDOR_TEST_ASSERT_EQUAL(contentLength, ~0ull);
            MORDOR_TEST_ASSERT_EQUAL(contentRange,
                ContentRange(65536 * (sequence - 1)));
            MORDOR_TEST_ASSERT_EQUAL(transferStream(request->requestStream(),
                NullStream::get()), 65536ull);
        } else if (requestUri == "/writeWriteAndSizeAdvice") {
            MORDOR_TEST_ASSERT_LESS_THAN_OR_EQUAL(++sequence, 16);
            MORDOR_TEST_ASSERT(!chunked);
            MORDOR_TEST_ASSERT_EQUAL(contentLength, 65536ull);
            MORDOR_TEST_ASSERT_EQUAL(contentRange,
                ContentRange(65536 * (sequence - 1), 65536 * sequence - 1,
                    1024 * 1024));
            MORDOR_TEST_ASSERT_EQUAL(transferStream(request->requestStream(),
                NullStream::get()), 65536ull);
        } else if (requestUri == "/writeWriteAndSizeAdviceStraddle") {
            MORDOR_TEST_ASSERT_LESS_THAN_OR_EQUAL(++sequence, 16);
            MORDOR_TEST_ASSERT(!chunked);
            if (sequence != 16) {
                MORDOR_TEST_ASSERT_EQUAL(contentLength, 65536ull);
                MORDOR_TEST_ASSERT_EQUAL(contentRange,
                    ContentRange(65536 * (sequence - 1), 65536 * sequence - 1,
                        1024 * 1024 - 4096));
                MORDOR_TEST_ASSERT_EQUAL(transferStream(
                    request->requestStream(), NullStream::get()), 65536ull);
            } else {
                MORDOR_TEST_ASSERT_EQUAL(contentLength, 65536 - 4096ull);
                MORDOR_TEST_ASSERT_EQUAL(contentRange,
                    ContentRange(1024 * 1024 - 65536, 1024 * 1024 - 4096 - 1,
                        1024 * 1024 - 4096));
                MORDOR_TEST_ASSERT_EQUAL(transferStream(
                    request->requestStream(), NullStream::get()),
                    65536 - 4096ull);
            }
        } else if (requestUri == "/truncate") {
            MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
            MORDOR_TEST_ASSERT(!chunked);
            MORDOR_TEST_ASSERT_EQUAL(contentLength, 0ull);
            MORDOR_TEST_ASSERT_EQUAL(contentRange,
                ContentRange(~0ull, ~0ull, 65536));
        } else if (requestUri == "/writeZeroLength") {
            MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
            MORDOR_TEST_ASSERT_EQUAL(contentLength, 0ull);
        } else {
            MORDOR_NOTREACHED();
        }
        respondError(request, OK);
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

MORDOR_UNITTEST_FIXTURE(WriteAdviceFixture, HTTPStream, writeNoAdvice)
{
    HTTPStream stream("http://localhost/writeNoAdvice", requestBroker);
    RandomStream random;
    transferStream(random, stream, 1024 * 1024);
    stream.close();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(WriteAdviceFixture, HTTPStream, writeSizeAdvice)
{
    HTTPStream stream("http://localhost/writeSizeAdvice", requestBroker);
    stream.adviseSize(1024 * 1024);
    RandomStream random;
    transferStream(random, stream, 1024 * 1024);
    stream.close();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(WriteAdviceFixture, HTTPStream, writeWriteAdvice)
{
    HTTPStream stream("http://localhost/writeWriteAdvice", requestBroker);
    stream.adviseWrite(65536);
    RandomStream random;
    transferStream(random, stream, 1024 * 1024);
    stream.close();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 16);
}

MORDOR_UNITTEST_FIXTURE(WriteAdviceFixture, HTTPStream, writeWriteAndSizeAdvice)
{
    HTTPStream stream("http://localhost/writeWriteAndSizeAdvice", requestBroker);
    stream.adviseWrite(65536);
    stream.adviseSize(1024 * 1024);
    RandomStream random;
    transferStream(random, stream, 1024 * 1024);
    stream.close();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 16);
}

MORDOR_UNITTEST_FIXTURE(WriteAdviceFixture, HTTPStream, writeWriteAndSizeAdviceStraddle)
{
    HTTPStream stream("http://localhost/writeWriteAndSizeAdviceStraddle", requestBroker);
    stream.adviseWrite(65536);
    stream.adviseSize(1024 * 1024 - 4096);
    RandomStream random;
    transferStream(random, stream, 1024 * 1024 - 4096);
    stream.close();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 16);
}

MORDOR_UNITTEST_FIXTURE(WriteAdviceFixture, HTTPStream, truncate)
{
    HTTPStream stream("http://localhost/truncate", requestBroker);
    stream.truncate(65536);
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}

MORDOR_UNITTEST_FIXTURE(WriteAdviceFixture, HTTPStream, writeZeroLength)
{
    HTTPStream stream("http://localhost/writeZeroLength", requestBroker);
    stream.adviseSize(0);
    stream.close();
    MORDOR_TEST_ASSERT_EQUAL(sequence, 1);
}
