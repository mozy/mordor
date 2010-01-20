// Copyright (c) 2010 - Mozy, Inc.

#include "mordor/pch.h"

#include "mordor/streams/stream.h"
#include "mordor/test/test.h"

using namespace Mordor;

namespace {

class NoStream : public Stream
{
    size_t read(Buffer &buffer, size_t length) { return 0; }
};

}

MORDOR_UNITTEST(Stream, emulatedDirectReadEOF)
{
    NoStream realstream;
    Stream *stream = &realstream;
    char buf[1];
    MORDOR_TEST_ASSERT_EQUAL(stream->read(buf, 1), 0u);
}
