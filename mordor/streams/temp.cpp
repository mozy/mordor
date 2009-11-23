// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "temp.h"

#include "mordor/config.h"
#include "mordor/string.h"

namespace Mordor {

static ConfigVar<std::string>::ptr g_tempDir = Config::lookup(
    "mordor.tempdir",
    std::string(""),
    "Temporary directory (blank for system default)");

TempStream::TempStream(const std::string &prefix, IOManager *ioManager,
                       Scheduler *scheduler)
{
    std::string tempdir = g_tempDir->val();
#ifdef WINDOWS
    std::wstring wtempdir = toUtf16(tempdir);
    if (wtempdir.empty()) {
        wtempdir.resize(MAX_PATH);
        DWORD len = GetTempPathW(MAX_PATH, &wtempdir[0]);
        if (len == 0)
            wtempdir = L".";
        else
            wtempdir.resize(len);
    }
    std::wstring tempfile;
    tempfile.resize(MAX_PATH);
    UINT len = GetTempFileNameW(wtempdir.c_str(),
        toUtf16(prefix).c_str(),
        0,
        &tempfile[0]);
    if (len == 0)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("GetTempFileNameW");
    init(tempfile, FileStream::READWRITE,
        (FileStream::CreateFlags)(FileStream::OPEN | FileStream::DELETE_ON_CLOSE),
        ioManager, scheduler);
#else
    if (tempdir.empty())
        tempdir = "/tmp/" + prefix + "XXXXXX";
    else
        tempdir += prefix + "XXXXXX";
    int fd = mkstemp(&tempdir[0]);
    if (fd < 0)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("mkstemp");
    init(fd, ioManager, scheduler);
    int rc = unlink(tempdir.c_str());
    if (rc != 0)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("unlink");
#endif            
}

}
