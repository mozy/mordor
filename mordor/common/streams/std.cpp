// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "std.h"

#include "mordor/common/exception.h"

StdinStream::StdinStream()
{
#ifdef WINDOWS
    HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdIn == INVALID_HANDLE_VALUE) {
        throwExceptionFromLastError("GetStdHandle");
    }
    if (hStdIn == NULL) {
        throwExceptionFromLastError(ERROR_FILE_NOT_FOUND, "GetStdHandle");
    }
    init(hStdIn, false);
#else
    init(STDIN_FILENO, false);
#endif
}

StdinStream::StdinStream(IOManager &ioManager)
{
#ifdef WINDOWS
    HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdIn == INVALID_HANDLE_VALUE) {
        throwExceptionFromLastError("GetStdHandle");
    }
    if (hStdIn == NULL) {
        throwExceptionFromLastError(ERROR_FILE_NOT_FOUND, "GetStdHandle");
    }
    init(&ioManager, hStdIn, false);
#else
    init(&ioManager, STDIN_FILENO, false);
#endif
}

StdoutStream::StdoutStream()
{
#ifdef WINDOWS
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdOut == INVALID_HANDLE_VALUE) {
        throwExceptionFromLastError("GetStdHandle");
    }
    if (hStdOut == NULL) {
        throwExceptionFromLastError(ERROR_FILE_NOT_FOUND, "GetStdHandle");
    }
    init(hStdOut, false);
#else
    init(STDOUT_FILENO, false);
#endif
}

StdoutStream::StdoutStream(IOManager &ioManager)
{
#ifdef WINDOWS
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdOut == INVALID_HANDLE_VALUE) {
        throwExceptionFromLastError("GetStdHandle");
    }
    if (hStdOut == NULL) {
        throwExceptionFromLastError(ERROR_FILE_NOT_FOUND, "GetStdHandle");
    }
    init(&ioManager, hStdOut, false);
#else
    init(&ioManager, STDOUT_FILENO, false);
#endif
}

StderrStream::StderrStream()
{
#ifdef WINDOWS
    HANDLE hStdErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hStdErr == INVALID_HANDLE_VALUE) {
        throwExceptionFromLastError("GetStdHandle");
    }
    if (hStdErr == NULL) {
        throwExceptionFromLastError(ERROR_FILE_NOT_FOUND, "GetStdHandle");
    }
    init(hStdErr, false);
#else
    init(STDERR_FILENO, false);
#endif
}

StderrStream::StderrStream(IOManager &ioManager)
{
#ifdef WINDOWS
    HANDLE hStdErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hStdErr == INVALID_HANDLE_VALUE) {
        throwExceptionFromLastError("GetStdHandle");
    }
    if (hStdErr == NULL) {
        throwExceptionFromLastError(ERROR_FILE_NOT_FOUND, "GetStdHandle");
    }
    init(&ioManager, hStdErr, false);
#else
    init(&ioManager, STDERR_FILENO, false);
#endif
}
