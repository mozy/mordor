// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include <iostream>

#include "mordor/common/config.h"
#include "mordor/common/version.h"
#include "mordor/test/test.h"
#include "mordor/test/stdoutlistener.h"

using namespace Mordor;
using namespace Mordor::Test;

#ifdef WINDOWS
#include <direct.h>
#define chdir _chdir
#endif

int main(int argc, const char **argv)
{
    Config::loadFromEnvironment();
    std::string newDirectory = argv[0];
#ifdef WINDOWS
    newDirectory = newDirectory.substr(0, newDirectory.rfind('\\'));
#else
    newDirectory = newDirectory.substr(0, newDirectory.rfind('/'));
#endif
    chdir(newDirectory.c_str());

    StdoutListener listener;
    if (argc > 1) {
        return runTests(testsForArguments(argc - 1, argv + 1), listener)
            ? 0 : 1;
    } else {
        return runTests(listener) ? 0 : 1;
    }
}
