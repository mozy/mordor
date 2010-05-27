// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#ifdef WINDOWS

#include "mordor/fiber.h"
#include "mordor/streams/file.h"
#include "mordor/streams/efs.h"
#include "mordor/streams/transfer.h"
#include "mordor/streams/random.h"
#include "mordor/streams/memory.h"
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
        Stream &fileStream = file;
        MORDOR_TEST_ASSERT_EQUAL(fileStream.read(b2, 65536), 4u);
        MORDOR_TEST_ASSERT(b2 == "cody");
    }
    DeleteFileW(L"dummy.efs");
    DeleteFileW(L"dummy2.efs");
}

MORDOR_UNITTEST(EFSStream, seek)
{
    {
        // create an EFS'd file with random content
        {
            RandomStream r;
            FileStream out("dummy3.efs", FileStream::WRITE, FileStream::OVERWRITE_OR_CREATE);
            transferStream(r, out, 100000);
        }
        if (!EncryptFileW(L"dummy3.efs"))
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("EncryptFileW");

        // now open for reading
        EFSStream efsr("dummy3.efs", true);

        // read the file serially to a MemoryStream
        MemoryStream control;
        transferStream(efsr, control);

        // now seek back to various offsets and ensure the data read is correct
        long long seek_offsets[] = {
            0,       // eof to bof
            50000,   // mid-file forward
            12345,   // mid-file backward
            0,       // mid-file to bof
            99999,   // forward, truncated read
            100000,  // eof
        };
        size_t num_tests = sizeof(seek_offsets) / sizeof(seek_offsets[0]);
        for(size_t i = 0; i < num_tests; ++i)
        {
            Buffer control_buf;
            control.seek(seek_offsets[i]);
            control.read(control_buf, 1000);

            Buffer test_buf;
            efsr.seek(seek_offsets[i]);
            // EFSStream is known to return partial reads; that is by design
            size_t to_read = 1000;
            do {
                to_read -= efsr.read(test_buf, to_read);
            } while(to_read > 0);

            MORDOR_TEST_ASSERT_EQUAL(control.tell(), efsr.tell());
            MORDOR_TEST_ASSERT_EQUAL(control_buf.readAvailable(), test_buf.readAvailable());
            MORDOR_TEST_ASSERT(control_buf == test_buf);
        }
    }

    DeleteFileW(L"dummy3.efs");
}

#endif
