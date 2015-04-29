// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/streams/deflate.h"
#include "mordor/streams/memory.h"
#include "mordor/streams/singleplex.h"
#include "mordor/streams/transfer.h"
#include "mordor/test/test.h"
#include "mordor/zip.h"

using namespace Mordor;

MORDOR_UNITTEST(Zip, compressionMethod)
{
    {
        MemoryStream::ptr stream(new MemoryStream);
        Zip zip(stream);
        ZipEntry& entry = zip.addFile();
        entry.filename("test");
        entry.compressionMethod(0);

        const std::string data("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXY0123456789");
        Buffer buffer(data);
        MemoryStream mem(buffer);
        transferStream(mem, entry.stream());
        zip.close();

        stream->seek(0);
        MORDOR_TEST_ASSERT_GREATER_THAN(stream->find(data), 0);
        Stream::ptr singleEx(new SingleplexStream(stream, SingleplexStream::READ));
        Zip zipEx(singleEx);
        const ZipEntry* entryEx = zipEx.getNextEntry();
        MORDOR_TEST_ASSERT(entryEx);
        MORDOR_TEST_ASSERT_EQUAL(entryEx->compressionMethod(), 0);
    }


    {
        MemoryStream::ptr stream(new MemoryStream);
        Stream::ptr single(new SingleplexStream(stream, SingleplexStream::WRITE));
        Zip zip(single);
        ZipEntry& entry = zip.addFile();
        entry.filename("test");
        entry.compressionMethod(8);

        const std::string data("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXY0123456789");
        Buffer buffer(data);
        MemoryStream mem(buffer);
        transferStream(mem, entry.stream());
        zip.close();

        Stream::ptr out(new MemoryStream);
        Stream::ptr singleDef(new SingleplexStream(out, SingleplexStream::WRITE));
        DeflateStream deflate(singleDef, false);
        deflate.write(buffer, data.length());
        deflate.close();
        out->seek(0);
        std::string dest = boost::static_pointer_cast<MemoryStream>(out)->readBuffer().toString();

        stream->seek(0);
        MORDOR_TEST_ASSERT_GREATER_THAN(stream->find(dest), 0);
        Stream::ptr singleEx(new SingleplexStream(stream, SingleplexStream::READ));
        Zip zipEx(singleEx);
        const ZipEntry* entryEx = zipEx.getNextEntry();
        MORDOR_TEST_ASSERT(entryEx);
        MORDOR_TEST_ASSERT_EQUAL(entryEx->compressionMethod(), 8);
    }
}

