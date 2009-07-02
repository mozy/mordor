// Copyright (c) 2009 - Decho Corp.

#include "mordor/test/test.h"
#include "mordor/test/stdoutlistener.h"

int main(int argc, char **argv)
{
    StdoutListener listener;
    if (argc > 1) {
        return runTests(testsForArguments(argc - 1, argv + 1), listener)
            ? 0 : 1;
    } else {
        return runTests(listener) ? 0 : 1;
    }
}
