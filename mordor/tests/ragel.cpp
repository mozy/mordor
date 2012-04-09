#include "mordor/http/http.h"
#include "mordor/http/parser.h"
#include "mordor/streams/buffer.h"
#include "mordor/streams/memory.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

MORDOR_UNITTEST(RagelParser, parseFromBuffer)
{
    Buffer buf("foo,bar");
    HTTP::StringSet stringset;
    HTTP::ListParser parser(stringset);
    parser.run(buf);
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(stringset.size(), 2U);
    MORDOR_TEST_ASSERT(stringset.find("foo") != stringset.end());
    MORDOR_TEST_ASSERT(stringset.find("bar") != stringset.end());
}

MORDOR_UNITTEST(RagelParser, parseFromBufferRemains)
{
    Buffer buf("list\n");
    HTTP::StringSet stringset;
    HTTP::ListParser parser(stringset);
    parser.run(buf);
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(!parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(stringset.size(), 1U);
    MORDOR_TEST_ASSERT_EQUAL(*stringset.begin(), std::string("list"));
}

MORDOR_UNITTEST(RagelParser, parseFromStream)
{
    Stream::ptr stream(new MemoryStream(Buffer("foo,bar")));
    HTTP::StringSet stringset;
    HTTP::ListParser parser(stringset);
    parser.run(stream);
    MORDOR_TEST_ASSERT(!parser.error());
    MORDOR_TEST_ASSERT(parser.complete());
    MORDOR_TEST_ASSERT_EQUAL(stringset.size(), 2U);
    MORDOR_TEST_ASSERT(stringset.find("foo") != stringset.end());
    MORDOR_TEST_ASSERT(stringset.find("bar") != stringset.end());
}
