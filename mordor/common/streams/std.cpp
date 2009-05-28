// Copyright (c) 2009 - Decho Corp.

#include "std.h"

#include "common/exception.h"

StdinStream::StdinStream()
{
#ifdef WINDOWS
    HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdIn == INVALID_HANDLE_VALUE) {
        throwExceptionFromLastError();
    }
    if (hStdIn == NULL) {
        throwExceptionFromLastError(ERROR_FILE_NOT_FOUND);
    }
    init(hStdIn, false);
#else
    init(STDIN_FILENO, false);
#endif
}

StdoutStream::StdoutStream()
{
#ifdef WINDOWS
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdOut == INVALID_HANDLE_VALUE) {
        throwExceptionFromLastError();
    }
    if (hStdOut == NULL) {
        throwExceptionFromLastError(ERROR_FILE_NOT_FOUND);
    }
    init(hStdOut, false);
#else
    init(STDOUT_FILENO, false);
#endif
}

StderrStream::StderrStream()
{
#ifdef WINDOWS
    HANDLE hStdErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hStdErr == INVALID_HANDLE_VALUE) {
        throwExceptionFromLastError();
    }
    if (hStdErr == NULL) {
        throwExceptionFromLastError(ERROR_FILE_NOT_FOUND);
    }
    init(hStdErr, false);
#else
    init(STDERR_FILENO, false);
#endif
}
