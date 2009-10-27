// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include <iostream>

#include "mordor/config.h"
#include "mordor/version.h"
#include "mordor/statistics.h"
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
    bool result;
    if (argc > 1) {
        result = runTests(testsForArguments(argc - 1, argv + 1), listener);
    } else {
        result = runTests(listener);
    }
    Statistics::dump(std::cout);
    return result ? 0 : 1;
}
