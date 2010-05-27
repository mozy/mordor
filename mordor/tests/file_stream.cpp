// Copyright (c) 2010 - Mozy, Inc.

#include "mordor/pch.h"

#include "mordor/streams/file.h"
#include "mordor/test/test.h"

using namespace Mordor;

#if 0 // WINDOWS
MORDOR_UNITTEST(FileStream, openSymlinkToDirectory)
{
    if (!CreateDirectoryW(L"symlinkdir", NULL))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateDirectoryW");
    if (!CreateSymbolicLinkW(L"symlink", L"symlinkdir", SYMBOLIC_LINK_FLAG_DIRECTORY)) {
        RemoveDirectoryW(L"symlinkdir");
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateSymbolicLinkW");
    }
    try {
        FileStream fstream(L"symlinktodir", FileStream::READ);
        Stream &stream(fstream);
        char buffer[6];
        stream.read(buffer, 6);
    } catch (...) {
        RemoveDirectoryW(L"symlinkdir");
        DeleteFileW(L"symlink");
        throw;
    }
    RemoveDirectoryW(L"symlinkdir");
    DeleteFileW(L"symlink");
}

MORDOR_UNITTEST(FileStream, openSymlinkToSelf)
{
    if (!CreateSymbolicLinkW(L"symlink", L"symlink", 0))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateSymbolicLinkW");
    try {
        MORDOR_TEST_ASSERT_EXCEPTION(FileStream stream(L"symlinktoself",
            FileStream::READ), UnresolvablePathException);
    } catch (...) {
        DeleteFileW(L"symlink");
        throw;
    }
    DeleteFileW(L"symlink");
}

MORDOR_UNITTEST(FileStream, openSymlinkToNothing)
{
    if (!CreateSymbolicLinkW(L"symlinkt", L"nothing", 0))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateSymbolicLinkW");
    try {
        MORDOR_TEST_ASSERT_EXCEPTION(FileStream stream(L"symlinktonothing",
            FileStream::READ), FileNotFoundException);
    } catch (...) {
        DeleteFileW(L"symlink");
        throw;
    }
    DeleteFileW(L"symlink");
}
#else

static std::string tempfilename()
{
    // Silly glibc, *forcing* me to not use mktemp, tempnam, or tmpnam
    std::string result("/tmp/mordorXXXXXX");
    int fd = mkstemp(&result[0]);
    if (fd < 0)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("mkstemp");
    unlink(result.c_str());
    close(fd);
    return result;
}

MORDOR_UNITTEST(FileStream, openSymlinkToDirectory)
{
    std::string dir("/tmp/mordorXXXXXX");
    if (!mkdtemp(&dir[0]))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("mkdtemp");
    std::string sym;
    try {
        sym = tempfilename();
    } catch(...) {
        rmdir(dir.c_str());
        throw;
    }
    if (symlink(dir.c_str(), sym.c_str())) {
        rmdir(dir.c_str());
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("symlink");
    }
    try {
        FileStream fstream(sym, FileStream::READ);
        Stream &stream(fstream);
        char buffer[6];
        stream.read(buffer, 6);
        MORDOR_NOTREACHED();
    } catch (IsDirectoryException &) {
    } catch (...) {
        rmdir(dir.c_str());
        unlink(sym.c_str());
        throw;
    }
    rmdir(dir.c_str());
    unlink(sym.c_str());
}

MORDOR_UNITTEST(FileStream, openSymlinkToSelf)
{
    std::string sym = tempfilename();
    if (symlink(sym.c_str(), sym.c_str()))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("symlink");
    try {
        MORDOR_TEST_ASSERT_EXCEPTION(FileStream stream(sym, FileStream::READ),
            TooManySymbolicLinksException);
    } catch (...) {
        unlink(sym.c_str());
        throw;
    }
    unlink(sym.c_str());
}

MORDOR_UNITTEST(FileStream, openSymlinkToNothing)
{
    std::string sym = tempfilename();
    std::string nothing = tempfilename();
    if (symlink(nothing.c_str(), sym.c_str()))
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("symlink");
    try {
        MORDOR_TEST_ASSERT_EXCEPTION(FileStream stream(sym, FileStream::READ),
            FileNotFoundException);
    } catch (...) {
        unlink(sym.c_str());
        throw;
    }
    unlink(sym.c_str());
}
#endif
