// Copyright (c) 2009 - Mozy, Inc.

#ifdef POSIX

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "mordor/streams/buffer.h"
#include "mordor/streams/crypto.h"
#include "mordor/streams/file.h"
#include "mordor/streams/memory.h"
#include "mordor/streams/temp.h"
#include "mordor/streams/transfer.h"
#include "mordor/tar.h"
#include "mordor/test/test.h"

using namespace Mordor;

class TarTestFixture
{
public:
    TarTestFixture()
        : m_key("12345678123456781234567812345678") {
        // create temp test folder
        m_testFolder = TempStream("tar_test_folder").path() + "/"; // get a temp name
        int rc = mkdir(m_testFolder.c_str(), 0755);
        MORDOR_TEST_ASSERT_EQUAL(rc, 0);
    }

    ~TarTestFixture() {
        int rc = system(("rm -rf " + m_testFolder).c_str());
        MORDOR_TEST_ASSERT_EQUAL(rc, 0);
    }

protected:
    std::string m_key;
    std::string m_testFolder;
};

MORDOR_UNITTEST_FIXTURE(TarTestFixture, Tar, extract)
{
    const char* data1 = "this is temp1 file for tar test";
    TempStream temp1(m_testFolder, false);
    std::string path1 = temp1.path();
    temp1.write(data1, strlen(data1));
    temp1.close();

    const char* data2 = "this is temp2 file for tar test";
    TempStream temp2(m_testFolder, false);
    std::string path2 = temp2.path();
    temp2.write(data2, strlen(data2));
    temp2.close();

    TempStream temp3(m_testFolder);
    std::string path3 = temp3.path() + "/";
    int rc = mkdir(path3.c_str(), 0755);
    MORDOR_TEST_ASSERT_EQUAL(rc, 0);

    TempStream temp4(m_testFolder);
    std::string path4 = temp4.path();
    rc = symlink(path1.c_str(), path4.c_str());
    MORDOR_TEST_ASSERT_EQUAL(rc, 0);

    std::string tarfile = m_testFolder + "test.tar";
    rc = system(("tar -P -cf " + tarfile + " " + path1 + " " + path2 + " " + path3 + " " + path4).c_str());
    MORDOR_TEST_ASSERT_EQUAL(rc, 0);

    FileStream::ptr stream(new FileStream(tarfile, FileStream::READ));
    Tar tar(stream);
    {
        const TarEntry* entry = tar.getNextEntry();
        MORDOR_TEST_ASSERT_EQUAL(entry->filename(), path1);
        Stream::ptr stream = entry->stream();
        MORDOR_TEST_ASSERT(stream);
        MemoryStream buf;
        transferStream(stream, buf);
        MORDOR_TEST_ASSERT(buf.buffer() == data1);
    }
    {
        const TarEntry* entry = tar.getNextEntry();
        MORDOR_TEST_ASSERT_EQUAL(entry->filename(), path2);
        Stream::ptr stream = entry->stream();
        MORDOR_TEST_ASSERT(stream);
        MemoryStream buf;
        transferStream(stream, buf);
        MORDOR_TEST_ASSERT(buf.buffer() == data2);
    }
    {
        const TarEntry* entry = tar.getNextEntry();
        MORDOR_TEST_ASSERT_EQUAL(entry->filename(), path3);
        MORDOR_TEST_ASSERT_EQUAL(entry->filetype(), TarEntry::DIRECTORY);
        Stream::ptr stream = entry->stream();
        MORDOR_TEST_ASSERT(!stream);
    }
    {
        const TarEntry* entry = tar.getNextEntry();
        MORDOR_TEST_ASSERT_EQUAL(entry->filename(), path4);
        MORDOR_TEST_ASSERT_EQUAL(entry->linkname(), path1);
        MORDOR_TEST_ASSERT_EQUAL(entry->filetype(), TarEntry::SYMLINK);
        Stream::ptr stream = entry->stream();
        MORDOR_TEST_ASSERT(!stream);
    }
    MORDOR_TEST_ASSERT(!tar.getNextEntry());
}

MORDOR_UNITTEST_FIXTURE(TarTestFixture, Tar, create)
{
    const char* data1 = "this is temp1 file for tar test";
    MemoryStream temp1((Buffer(data1)));
    std::string path1 = m_testFolder + "tartest1";

    const char* data2 = "this is temp2 file for tar test";
    MemoryStream temp2((Buffer(data2)));
    std::string path2 = m_testFolder + "tartest2";

    std::string path3 = m_testFolder + "tartest_dir/";
    std::string path4 = m_testFolder + "tartest_symlnk";

    std::string tarfile = m_testFolder + "test.tar";
    Stream::ptr stream(new FileStream(tarfile, FileStream::WRITE, FileStream::OVERWRITE_OR_CREATE));
    Tar tar(stream);
    {
        TarEntry& entry = tar.addFile();
        entry.filename(path1);
        entry.size(strlen(data1));
        entry.mode(0644);
        entry.mtime(time(NULL));
        entry.filetype(TarEntry::REGULAR);
        transferStream(temp1, entry.stream());
        temp1.close();
    }
    {
        TarEntry& entry = tar.addFile();
        entry.filename(path2);
        entry.size(strlen(data2));
        entry.mode(0644);
        entry.mtime(time(NULL));
        entry.filetype(TarEntry::REGULAR);
        transferStream(temp2, entry.stream());
        temp2.close();
    }
    {
        TarEntry& entry = tar.addFile();
        entry.filename(path3);
        entry.mode(0755);
        entry.mtime(time(NULL));
        entry.filetype(TarEntry::DIRECTORY);
    }
    {
        TarEntry& entry = tar.addFile();
        entry.filename(path4);
        entry.linkname(path1);
        entry.mode(0777);
        entry.mtime(time(NULL));
        entry.filetype(TarEntry::SYMLINK);
    }
    tar.close();

    int rc = system(("tar -P -xf " + tarfile).c_str());
    MORDOR_TEST_ASSERT_EQUAL(rc, 0);

    {
        FileStream::ptr file(new FileStream(path1, FileStream::READ));
        MemoryStream buf;
        transferStream(file, buf);
        MORDOR_TEST_ASSERT(buf.buffer() == data1);
    }
    {
        FileStream::ptr file(new FileStream(path2, FileStream::READ));
        MemoryStream buf;
        transferStream(file, buf);
        MORDOR_TEST_ASSERT(buf.buffer() == data2);
    }

    struct stat st;
    rc = stat(path3.c_str(), &st);
    MORDOR_TEST_ASSERT_EQUAL(rc, 0);
    MORDOR_TEST_ASSERT(st.st_mode & S_IFDIR);

    rc = stat(path4.c_str(), &st);
    MORDOR_TEST_ASSERT_EQUAL(rc, 0);
    MORDOR_TEST_ASSERT(st.st_mode & S_IFLNK);
}

MORDOR_UNITTEST_FIXTURE(TarTestFixture, Tar, longnameRead)
{
    const char* data = "this is temp file with long name for tar reading test";
    TempStream temp(m_testFolder + "tartest_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", false);
    std::string path = temp.path();
    MORDOR_TEST_ASSERT_GREATER_THAN(path.length(), 100u);
    temp.write(data, strlen(data));
    temp.close();

    std::string tarfile = m_testFolder + "test.tar";
    int rc = system(("tar -P -cf " + tarfile + " " + path).c_str());
    MORDOR_TEST_ASSERT_EQUAL(rc, 0);

    FileStream::ptr tarstream(new FileStream(tarfile, FileStream::READ));
    Tar tar(tarstream);
    const TarEntry* entry = tar.getNextEntry();
    MORDOR_TEST_ASSERT_EQUAL(entry->filename(), path);
    Stream::ptr stream = entry->stream();
    MORDOR_TEST_ASSERT(stream);
    MemoryStream buf;
    transferStream(stream, buf);
    MORDOR_TEST_ASSERT(buf.buffer() == data);
    MORDOR_TEST_ASSERT(!tar.getNextEntry());
}

MORDOR_UNITTEST_FIXTURE(TarTestFixture, Tar, longnameWrite)
{
    const char* data = "this is temp file with long name for tar writing test";
    MemoryStream temp((Buffer(data)));
    std::string path = m_testFolder + "tartest_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    MORDOR_TEST_ASSERT_GREATER_THAN(path.length(), 100u);

    std::string tarfile = m_testFolder + "test.tar";
    Stream::ptr stream(new FileStream(tarfile, FileStream::WRITE, FileStream::OVERWRITE_OR_CREATE));
    Tar tar(stream);
    TarEntry& entry = tar.addFile();
    entry.filename(path);
    entry.size(strlen(data));
    entry.mode(0644);
    entry.mtime(time(NULL));
    entry.filetype(TarEntry::REGULAR);
    transferStream(temp, entry.stream());
    temp.close();
    tar.close();

    int rc = system(("tar -P -xf " + tarfile).c_str());
    MORDOR_TEST_ASSERT_EQUAL(rc, 0);
    FileStream::ptr file(new FileStream(path, FileStream::READ));
    MemoryStream buf;
    transferStream(file, buf);
    MORDOR_TEST_ASSERT(buf.buffer() == data);
}

MORDOR_UNITTEST_FIXTURE(TarTestFixture, Tar, metaRead)
{
    const char* data = "this is temp file with metadata for tar reading test";
    TempStream temp(m_testFolder, false);
    std::string path = temp.path();
    temp.write(data, strlen(data));
    temp.close();

    struct stat st;
    int rc = stat(path.c_str(), &st);
    MORDOR_TEST_ASSERT_EQUAL(rc, 0);

    std::string tarfile = m_testFolder + "test.tar";
    rc = system(("tar -P --format pax -cf " + tarfile + " " + path).c_str());
    MORDOR_TEST_ASSERT_EQUAL(rc, 0);

    FileStream::ptr tarstream(new FileStream(tarfile, FileStream::READ));
    Tar tar(tarstream);
    const TarEntry* entry = tar.getNextEntry();
    MORDOR_TEST_ASSERT_EQUAL(entry->filename(), path);
    MORDOR_TEST_ASSERT_EQUAL(entry->size(), st.st_size);
    MORDOR_TEST_ASSERT_EQUAL(entry->mode() & 0777, st.st_mode & 0777);
    MORDOR_TEST_ASSERT_EQUAL(entry->gid(), st.st_gid);
    MORDOR_TEST_ASSERT_EQUAL(entry->uid(), st.st_uid);
    MORDOR_TEST_ASSERT_EQUAL(entry->mtime(), st.st_mtime);
    MORDOR_TEST_ASSERT_EQUAL(entry->atime(), st.st_atime);
    MORDOR_TEST_ASSERT_EQUAL(entry->ctime(), st.st_ctime);

    Stream::ptr stream = entry->stream();
    MORDOR_TEST_ASSERT(stream);
    MemoryStream buf;
    transferStream(stream, buf);
    MORDOR_TEST_ASSERT(buf.buffer() == data);
    MORDOR_TEST_ASSERT(!tar.getNextEntry());
}

MORDOR_UNITTEST_FIXTURE(TarTestFixture, Tar, metaWrite)
{
    const char* data = "this is temp file with long name for tar writing test";
    MemoryStream temp((Buffer(data)));
    std::string path = m_testFolder + "tartest";

    std::string tarfile = m_testFolder + "test.tar";
    Stream::ptr stream(new FileStream(tarfile, FileStream::WRITE, FileStream::OVERWRITE_OR_CREATE));
    Tar tar(stream);
    TarEntry& entry = tar.addFile();
    time_t now = time(NULL);
    uid_t uid = getuid();
    gid_t gid = getgid();
    entry.filename(path);
    entry.size(strlen(data));
    entry.mode(0644);
    entry.uid(uid);
    entry.gid(gid);
    entry.mtime(now);
    entry.atime(now);
    entry.ctime(now);
    entry.filetype(TarEntry::REGULAR);
    transferStream(temp, entry.stream());
    temp.close();
    tar.close();

    int rc = system(("tar -P -xf " + tarfile).c_str());
    MORDOR_TEST_ASSERT_EQUAL(rc, 0);

    struct stat st;
    rc = stat(path.c_str(), &st);
    MORDOR_TEST_ASSERT_EQUAL(rc, 0);
    MORDOR_TEST_ASSERT_EQUAL(strlen(data), size_t(st.st_size));
    MORDOR_TEST_ASSERT_EQUAL(mode_t(0644), st.st_mode & 0777);
    MORDOR_TEST_ASSERT_EQUAL(gid, st.st_gid);
    MORDOR_TEST_ASSERT_EQUAL(uid, st.st_uid);
    MORDOR_TEST_ASSERT_EQUAL(now, st.st_mtime);
    MORDOR_TEST_ASSERT_ABOUT_EQUAL(now, st.st_atime, 3);
    MORDOR_TEST_ASSERT_ABOUT_EQUAL(now, st.st_ctime, 3);

    FileStream::ptr file(new FileStream(path, FileStream::READ));
    MemoryStream buf;
    transferStream(file, buf);
    MORDOR_TEST_ASSERT(buf.buffer() == data);
}

MORDOR_UNITTEST_FIXTURE(TarTestFixture, Tar, encrypt)
{
    const char* data1 = "this is temp1 file for encrypted tar test";
    MemoryStream temp1((Buffer(data1)));
    std::string path1 = m_testFolder + "tartest1";

    const char* data2 = "this is temp2 file for encrypted tar test";
    MemoryStream temp2((Buffer(data2)));
    std::string path2 = m_testFolder + "tartest2";

    std::string path3 = m_testFolder + "tartest_dir/";
    std::string path4 = m_testFolder + "tartest_symlnk";

    std::string encrytedfile = m_testFolder + "test.tar.x";
    std::string tarfile = m_testFolder + "test.tar";
    Stream::ptr stream(new FileStream(encrytedfile, FileStream::WRITE, FileStream::OVERWRITE_OR_CREATE));
    stream.reset(new CryptoStream(stream, EVP_aes_256_cbc(), m_key));
    Tar tar(stream);
    {
        TarEntry& entry = tar.addFile();
        entry.filename(path1);
        entry.size(strlen(data1));
        entry.mode(0644);
        entry.mtime(time(NULL));
        entry.filetype(TarEntry::REGULAR);
        transferStream(temp1, entry.stream());
        temp1.close();
    }
    {
        TarEntry& entry = tar.addFile();
        entry.filename(path2);
        entry.size(strlen(data2));
        entry.mode(0644);
        entry.mtime(time(NULL));
        entry.filetype(TarEntry::REGULAR);
        transferStream(temp2, entry.stream());
        temp2.close();
    }
    {
        TarEntry& entry = tar.addFile();
        entry.filename(path3);
        entry.mode(0755);
        entry.mtime(time(NULL));
        entry.filetype(TarEntry::DIRECTORY);
        MORDOR_TEST_ASSERT(!entry.stream());
    }
    {
        TarEntry& entry = tar.addFile();
        entry.filename(path4);
        entry.linkname(path1);
        entry.mode(0777);
        entry.mtime(time(NULL));
        entry.filetype(TarEntry::SYMLINK);
        MORDOR_TEST_ASSERT(!entry.stream());
    }
    tar.close();

    {
        // decrypt to tar first
        Stream::ptr in(new FileStream(encrytedfile, FileStream::READ));
        in.reset(new CryptoStream(in, EVP_aes_256_cbc(), m_key));

        Stream::ptr out(new FileStream(tarfile, FileStream::WRITE, FileStream::OVERWRITE_OR_CREATE));
        transferStream(in, out);
    }

    int rc = system(("tar -P -xf " + tarfile).c_str());
    MORDOR_TEST_ASSERT_EQUAL(rc, 0);

    {
        FileStream::ptr file(new FileStream(path1, FileStream::READ));
        MemoryStream buf;
        transferStream(file, buf);
        MORDOR_TEST_ASSERT(buf.buffer() == data1);
    }
    {
        FileStream::ptr file(new FileStream(path2, FileStream::READ));
        MemoryStream buf;
        transferStream(file, buf);
        MORDOR_TEST_ASSERT(buf.buffer() == data2);
    }

    struct stat st;
    rc = stat(path3.c_str(), &st);
    MORDOR_TEST_ASSERT_EQUAL(rc, 0);
    MORDOR_TEST_ASSERT(st.st_mode & S_IFDIR);

    rc = stat(path4.c_str(), &st);
    MORDOR_TEST_ASSERT_EQUAL(rc, 0);
    MORDOR_TEST_ASSERT(st.st_mode & S_IFLNK);
}

MORDOR_UNITTEST_FIXTURE(TarTestFixture, Tar, decrypt)
{
    const char* data1 = "this is temp1 file for decrypted tar test";
    MemoryStream temp1((Buffer(data1)));
    std::string path1 = m_testFolder + "tartest1";

    const char* data2 = "this is temp2 file for decrypted tar test";
    MemoryStream temp2((Buffer(data2)));
    std::string path2 = m_testFolder + "tartest2";

    std::string path3 = m_testFolder + "tartest_dir/";
    std::string path4 = m_testFolder + "tartest_symlnk";

    std::string encrytedfile = m_testFolder + "test.tar.x";
    std::string tarfile = m_testFolder + "test.tar";

    {
        Stream::ptr stream(new FileStream(encrytedfile, FileStream::WRITE, FileStream::OVERWRITE_OR_CREATE));
        stream.reset(new CryptoStream(stream, EVP_aes_256_cbc(), m_key));
        Tar tar(stream);
        {
            TarEntry& entry = tar.addFile();
            entry.filename(path1);
            entry.size(strlen(data1));
            entry.mode(0644);
            entry.mtime(time(NULL));
            entry.filetype(TarEntry::REGULAR);
            transferStream(temp1, entry.stream());
            temp1.close();
        }
        {
            TarEntry& entry = tar.addFile();
            entry.filename(path2);
            entry.size(strlen(data2));
            entry.mode(0644);
            entry.mtime(time(NULL));
            entry.filetype(TarEntry::REGULAR);
            transferStream(temp2, entry.stream());
            temp2.close();
        }
        {
            TarEntry& entry = tar.addFile();
            entry.filename(path3);
            entry.mode(0755);
            entry.mtime(time(NULL));
            entry.filetype(TarEntry::DIRECTORY);
            MORDOR_TEST_ASSERT(!entry.stream());
        }
        {
            TarEntry& entry = tar.addFile();
            entry.filename(path4);
            entry.linkname(path1);
            entry.mode(0777);
            entry.mtime(time(NULL));
            entry.filetype(TarEntry::SYMLINK);
            MORDOR_TEST_ASSERT(!entry.stream());
        }
        tar.close();
    }

    {
        Stream::ptr stream(new FileStream(encrytedfile, FileStream::READ));
        stream.reset(new CryptoStream(stream, EVP_aes_256_cbc(), m_key));
        Tar tar(stream);
        {
            const TarEntry* entry = tar.getNextEntry();
            MORDOR_TEST_ASSERT_EQUAL(entry->filename(), path1);
            Stream::ptr stream = entry->stream();
            MORDOR_TEST_ASSERT(stream);
            MemoryStream buf;
            transferStream(stream, buf);
            MORDOR_TEST_ASSERT(buf.buffer() == data1);
        }
        {
            const TarEntry* entry = tar.getNextEntry();
            MORDOR_TEST_ASSERT_EQUAL(entry->filename(), path2);
            Stream::ptr stream = entry->stream();
            MORDOR_TEST_ASSERT(stream);
            MemoryStream buf;
            transferStream(stream, buf);
            MORDOR_TEST_ASSERT(buf.buffer() == data2);
        }
        {
            const TarEntry* entry = tar.getNextEntry();
            MORDOR_TEST_ASSERT_EQUAL(entry->filename(), path3);
            MORDOR_TEST_ASSERT_EQUAL(entry->filetype(), TarEntry::DIRECTORY);
            Stream::ptr stream = entry->stream();
            MORDOR_TEST_ASSERT(!stream);
        }
        {
            const TarEntry* entry = tar.getNextEntry();
            MORDOR_TEST_ASSERT_EQUAL(entry->filename(), path4);
            MORDOR_TEST_ASSERT_EQUAL(entry->linkname(), path1);
            MORDOR_TEST_ASSERT_EQUAL(entry->filetype(), TarEntry::SYMLINK);
            Stream::ptr stream = entry->stream();
            MORDOR_TEST_ASSERT(!stream);
        }
        MORDOR_TEST_ASSERT(!tar.getNextEntry());
        tar.close();
    }
}

MORDOR_UNITTEST_FIXTURE(TarTestFixture, Tar, brokenData)
{
    const char* data = "this is temp file for tar test";
    MemoryStream temp((Buffer(data)));
    std::string path = m_testFolder + "tartest";
    std::string tarfile = m_testFolder + "test.tar";
    Stream::ptr stream(new FileStream(tarfile, FileStream::WRITE, FileStream::OVERWRITE_OR_CREATE));
    Tar tar(stream);
    {
        TarEntry& entry = tar.addFile();
        entry.filename(path);
        entry.size(strlen(data));
        entry.filetype(TarEntry::REGULAR);
        transferStream(temp, entry.stream(), entry.size() - 1);
        temp.close();
    }
    MORDOR_TEST_ASSERT_EXCEPTION(tar.addFile(), UnexpectedTarSizeException);
}

MORDOR_UNITTEST_FIXTURE(TarTestFixture, Tar, incompleteHeader)
{
    std::string tarfile = m_testFolder + "test.tar";
    Stream::ptr stream(new FileStream(tarfile, FileStream::WRITE, FileStream::OVERWRITE_OR_CREATE));
    Tar tar(stream);
    {
        TarEntry& entry = tar.addFile();
        entry.filetype(TarEntry::REGULAR);
        // filename/size not set
    }
    MORDOR_TEST_ASSERT_EXCEPTION(tar.addFile(), IncompleteTarHeaderException);
}

#endif

