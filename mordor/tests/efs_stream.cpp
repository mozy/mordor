// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#ifdef WINDOWS

#include "mordor/fiber.h"
#include "mordor/streams/file.h"
#include "mordor/streams/efs.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

MORDOR_UNITTEST(EFSStream, basic)
{
    {
        FileStream file("dummy.efs", FileStream::WRITE, FileStream::OVERWRITE_OR_CREATE);
        file.write("cody", 4);
    }
    if (!EncryptFileW(L"dummy.efs"))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("EncryptFileW");
    Buffer b, b2;
    {
        EFSStream efs("dummy.efs", true);
        efs.read(b, 65536);
        MORDOR_TEST_ASSERT_EQUAL(efs.read(b, 65536), 0u);
    }
    MORDOR_TEST_ASSERT_GREATER_THAN_OR_EQUAL(b.readAvailable(), 4u);
    {
        EFSStream efs("dummy2.efs", false);
        efs.write(b, b.readAvailable());
        efs.close();
    }
    {
        FileStream file("dummy2.efs");
        MORDOR_TEST_ASSERT_EQUAL(file.read(b2, 65536), 4u);
        MORDOR_TEST_ASSERT(b2 == "cody");
    }
    DeleteFileW(L"dummy.efs");
    DeleteFileW(L"dummy2.efs");
}

#endif
