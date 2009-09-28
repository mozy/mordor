// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#ifdef WINDOWS

#include "mordor/common/fiber.h"
#include "mordor/common/streams/file.h"
#include "mordor/common/streams/efs.h"
#include "mordor/test/test.h"

TEST_WITH_SUITE(EFSStream, basic)
{
    Fiber::ptr mainFiber(new Fiber());
    {
        FileStream file("dummy.efs", FileStream::WRITE, CREATE_ALWAYS);
        file.write("cody", 4);
    }
    if (!EncryptFileW(L"dummy.efs"))
        throwExceptionFromLastError("EncryptFileW");
    Buffer b, b2;
    {
        EFSStream efs("dummy.efs", true);
        efs.read(b, 65536);
        TEST_ASSERT_EQUAL(efs.read(b, 65536), 0u);
    }
    TEST_ASSERT_GREATER_THAN_OR_EQUAL(b.readAvailable(), 4u);
    {
        EFSStream efs("dummy2.efs", false);
        efs.write(b, b.readAvailable());
        efs.close();
    }
    {
        FileStream file("dummy2.efs");
        TEST_ASSERT_EQUAL(file.read(b2, 65536), 4u);
        TEST_ASSERT(b2 == "cody");
    }
    DeleteFileW(L"dummy.efs");
    DeleteFileW(L"dummy2.efs");
}

#endif
