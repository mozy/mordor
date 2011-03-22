// Copyright (c) 2009 - Mozy, Inc.

#include "temp.h"

#include "mordor/config.h"
#include "mordor/string.h"

namespace Mordor {

static ConfigVar<std::string>::ptr g_tempDir = Config::lookup(
    "mordor.tempdir",
    std::string(""),
    "Temporary directory (blank for system default)");

TempStream::TempStream(const std::string &prefix, bool deleteOnClose,
                       IOManager *ioManager, Scheduler *scheduler)
{
    std::string tempdir;
    bool absolutePath =
#ifdef WINDOWS
        (prefix.size() >= 2 && (prefix[1] == ':' || prefix[1] == '\\')) ||
        (!prefix.empty() && prefix[0] == '\\');
#else
        !prefix.empty() && prefix[0] == '/';
#endif
    if (!absolutePath)
        tempdir = g_tempDir->val();
#ifdef WINDOWS
    std::wstring wtempdir = toUtf16(tempdir);
    if (!absolutePath && wtempdir.empty()) {
        wtempdir.resize(MAX_PATH);
        DWORD len = GetTempPathW(MAX_PATH, &wtempdir[0]);
        if (len == 0)
            wtempdir = L".";
        else
            wtempdir.resize(len);
    }
    std::wstring prefixW = toUtf16(prefix);
    size_t backslash = prefixW.rfind(L'\\');
    if (backslash != std::wstring::npos) {
        wtempdir += prefixW.substr(0, backslash);
        prefixW = prefixW.substr(backslash + 1);
    }
    std::wstring tempfile;
    tempfile.resize(MAX_PATH);
    UINT len = GetTempFileNameW(wtempdir.c_str(),
        prefixW.c_str(),
        0,
        &tempfile[0]);
    if (len == 0)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("GetTempFileNameW");
    tempfile.resize(len);
    init(tempfile, FileStream::READWRITE,
        (FileStream::CreateFlags)(FileStream::OPEN |
            (deleteOnClose ? FileStream::DELETE_ON_CLOSE : 0)),
        ioManager, scheduler);
#else
    if (!absolutePath && tempdir.empty())
        tempdir = "/tmp/" + prefix + "XXXXXX";
    else if (!absolutePath)
        tempdir += prefix + "XXXXXX";
    else
        tempdir = prefix + "XXXXXX";
    int fd = mkstemp(&tempdir[0]);
    if (fd < 0)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("mkstemp");
    init(fd, ioManager, scheduler);
    if (deleteOnClose) {
        int rc = unlink(tempdir.c_str());
        if (rc != 0)
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("unlink");
    }
    m_path = tempdir;
#endif
}

}
