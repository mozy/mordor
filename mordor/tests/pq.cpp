// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include <iostream>

#include "mordor/config.h"
#include "mordor/pq.h"
#include "mordor/version.h"
#include "mordor/statistics.h"
#include "mordor/test/test.h"
#include "mordor/test/stdoutlistener.h"

using namespace Mordor;
using namespace Mordor::PQ;
using namespace Mordor::Test;

#ifdef WINDOWS
#include <direct.h>
#define chdir _chdir
#endif

std::string g_goodConnString;
std::string g_badConnString;

int main(int argc, const char **argv)
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
            << " <good connection string> <bad connection string>"
            << std::endl;
        return 1;
    }
    g_goodConnString = argv[1];
    g_badConnString = argv[2];
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
    if (argc > 3) {
        result = runTests(testsForArguments(argc - 3, argv + 3), listener);
    } else {
        result = runTests(listener);
    }
    std::cout << Statistics::dump();
    return result ? 0 : 1;
}

void constantQuery(const std::string &queryName = std::string(), IOManager *ioManager = NULL)
{
    Connection conn(g_goodConnString, ioManager);
    PreparedStatement stmt = conn.prepare("SELECT 1, 'mordor'", queryName);
    Result result = stmt.execute();
    MORDOR_TEST_ASSERT_EQUAL(result.rows(), 1u);
    MORDOR_TEST_ASSERT_EQUAL(result.columns(), 2u);
    MORDOR_TEST_ASSERT_EQUAL(result.get<int>(0, 0), 1);
    MORDOR_TEST_ASSERT_EQUAL(result.get<long long>(0, 0), 1);
    MORDOR_TEST_ASSERT_EQUAL(result.get<const char *>(0, 1), "mordor");
    MORDOR_TEST_ASSERT_EQUAL(result.get<std::string>(0, 1), "mordor");
}

MORDOR_UNITTEST(PQ, constantQueryBlocking)
{ constantQuery(); }
MORDOR_UNITTEST(PQ, constantQueryAsync)
{ IOManager ioManager; constantQuery(std::string(), &ioManager); }
MORDOR_UNITTEST(PQ, constantQueryPreparedBlocking)
{ constantQuery("constant"); }
MORDOR_UNITTEST(PQ, constantQueryPreparedAsync)
{ IOManager ioManager; constantQuery("constant", &ioManager); }
