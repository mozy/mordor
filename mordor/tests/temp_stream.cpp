// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "mordor/streams/temp.h"
#include "mordor/test/test.h"

using namespace Mordor;

MORDOR_UNITTEST(TempStream, SystemTemp)
{
    TempStream temp;
    MORDOR_ASSERT(temp.supportsRead());
    MORDOR_ASSERT(temp.supportsWrite());
    MORDOR_ASSERT(temp.supportsSeek());
}

MORDOR_UNITTEST(TempStream, SystemTempWithPrefix)
{
    TempStream temp("mordor");
    MORDOR_ASSERT(temp.supportsRead());
    MORDOR_ASSERT(temp.supportsWrite());
    MORDOR_ASSERT(temp.supportsSeek());
}

MORDOR_UNITTEST(TempStream, SystemTempWithPrefix2)
{
    TempStream temp("m");
    MORDOR_ASSERT(temp.supportsRead());
    MORDOR_ASSERT(temp.supportsWrite());
    MORDOR_ASSERT(temp.supportsSeek());
}
