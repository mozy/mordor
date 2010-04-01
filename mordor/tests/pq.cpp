// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include <iostream>

#include "mordor/config.h"
#include "mordor/pq.h"
#include "mordor/version.h"
#include "mordor/statistics.h"
#include "mordor/streams/memory.h"
#include "mordor/streams/transfer.h"
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
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
            << " <connection string>"
            << std::endl;
        return 1;
    }
    g_goodConnString = argv[1];
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
    if (argc > 2) {
        result = runTests(testsForArguments(argc - 2, argv + 2), listener);
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

MORDOR_UNITTEST(PQ, invalidConnStringBlocking)
{
    MORDOR_TEST_ASSERT_EXCEPTION(Connection conn("garbage"), ConnectionException);
}
MORDOR_UNITTEST(PQ, invalidConnStringAsync)
{
    IOManager ioManager;
    MORDOR_TEST_ASSERT_EXCEPTION(Connection conn("garbage", &ioManager), ConnectionException);
}

MORDOR_UNITTEST(PQ, invalidConnString2Blocking)
{
    MORDOR_TEST_ASSERT_EXCEPTION(Connection conn("garbage="), ConnectionException);
}
MORDOR_UNITTEST(PQ, invalidConnString2Async)
{
    IOManager ioManager;
    MORDOR_TEST_ASSERT_EXCEPTION(Connection conn("garbage=", &ioManager), ConnectionException);
}

MORDOR_UNITTEST(PQ, invalidConnString3Blocking)
{
    MORDOR_TEST_ASSERT_EXCEPTION(Connection conn("host=garbage"), ConnectionException);
}
MORDOR_UNITTEST(PQ, invalidConnString3Async)
{
    IOManager ioManager;
    MORDOR_TEST_ASSERT_EXCEPTION(Connection conn("host=garbage", &ioManager), ConnectionException);
}

MORDOR_UNITTEST(PQ, badConnStringBlocking)
{
    MORDOR_TEST_ASSERT_EXCEPTION(Connection conn(g_badConnString), ConnectionException);
}
MORDOR_UNITTEST(PQ, badConnStringAsync)
{
    IOManager ioManager;
    MORDOR_TEST_ASSERT_EXCEPTION(Connection conn(g_badConnString, &ioManager), ConnectionException);
}

void queryAfterDisconnect(IOManager *ioManager = NULL)
{
    Connection conn(g_goodConnString, ioManager);

    close(PQsocket(conn.conn()));
    MORDOR_TEST_ASSERT_EXCEPTION(conn.execute("SELECT 1"), ConnectionException);
    conn.reset();
    Result result = conn.execute("SELECT 1");
    MORDOR_TEST_ASSERT_EQUAL(result.rows(), 1u);
    MORDOR_TEST_ASSERT_EQUAL(result.columns(), 1u);
    MORDOR_TEST_ASSERT_EQUAL(result.get<int>(0, 0), 1);
}

MORDOR_UNITTEST(PQ, queryAfterDisconnectBlocking)
{ queryAfterDisconnect(); }
MORDOR_UNITTEST(PQ, queryAfterDisconnectAsync)
{ IOManager ioManager; queryAfterDisconnect(&ioManager); }

void fillUsers(Connection &conn)
{
    conn.execute("CREATE TEMPORARY TABLE users (id INTEGER, name TEXT, height SMALLINT, awesome BOOLEAN, company TEXT, gender CHAR, efficiency REAL, crazy DOUBLE PRECISION, sometime TIMESTAMP)");
    conn.execute("INSERT INTO users VALUES (1, 'cody', 72, true, 'Mozy', 'M', .9, .75, '2009-05-19 15:53:45.123456')");
    conn.execute("INSERT INTO users VALUES (2, 'brian', 70, false, NULL, 'M', .9, .25, NULL)");
}

template <class ParamType, class ExpectedType>
void queryForParam(const std::string &query, ParamType param, size_t expectedCount,
    ExpectedType expected,
    const std::string &queryName = std::string(), IOManager *ioManager = NULL)
{
    Connection conn(g_goodConnString, ioManager);
    fillUsers(conn);
    PreparedStatement stmt = conn.prepare(query, queryName);
    Result result = stmt.execute(param);
    MORDOR_TEST_ASSERT_EQUAL(result.rows(), expectedCount);
    MORDOR_TEST_ASSERT_EQUAL(result.columns(), 1u);
    MORDOR_TEST_ASSERT_EQUAL(result.get<ExpectedType>(0, 0), expected);
}

MORDOR_UNITTEST(PQ, queryForIntBlocking)
{ queryForParam("SELECT name FROM users WHERE id=$1", 2, 1u, "brian"); }
MORDOR_UNITTEST(PQ, queryForIntAsync)
{ IOManager ioManager; queryForParam("SELECT name FROM users WHERE id=$1", 2, 1u, "brian", std::string(), &ioManager); }
MORDOR_UNITTEST(PQ, queryForIntPreparedBlocking)
{ queryForParam("SELECT name FROM users WHERE id=$1::integer", 2, 1u, "brian", "constant"); }
MORDOR_UNITTEST(PQ, queryForIntPreparedAsync)
{ IOManager ioManager; queryForParam("SELECT name FROM users WHERE id=$1::integer", 2, 1u, "brian", "constant", &ioManager); }

MORDOR_UNITTEST(PQ, queryForStringBlocking)
{ queryForParam("SELECT id FROM users WHERE name=$1", "brian", 1u, 2); }
MORDOR_UNITTEST(PQ, queryForStringAsync)
{ IOManager ioManager; queryForParam("SELECT id FROM users WHERE name=$1", "brian", 1u, 2, std::string(), &ioManager); }
MORDOR_UNITTEST(PQ, queryForStringPreparedBlocking)
{ queryForParam("SELECT id FROM users WHERE name=$1::text", "brian", 1u, 2, "constant"); }
MORDOR_UNITTEST(PQ, queryForStringPreparedAsync)
{ IOManager ioManager; queryForParam("SELECT id FROM users WHERE name=$1::text", "brian", 1u, 2, "constant", &ioManager); }

MORDOR_UNITTEST(PQ, queryForSmallIntBlocking)
{ queryForParam("SELECT id FROM users WHERE height=$1", (short)70, 1u, 2); }
MORDOR_UNITTEST(PQ, queryForSmallIntAsync)
{ IOManager ioManager; queryForParam("SELECT id FROM users WHERE height=$1", (short)70, 1u, 2, std::string(), &ioManager); }
MORDOR_UNITTEST(PQ, queryForSmallIntPreparedBlocking)
{ queryForParam("SELECT id FROM users WHERE height=$1::smallint", (short)70, 1u, 2, "constant"); }
MORDOR_UNITTEST(PQ, queryForSmallIntPreparedAsync)
{ IOManager ioManager; queryForParam("SELECT id FROM users WHERE height=$1::smallint", (short)70, 1u, 2, "constant", &ioManager); }

MORDOR_UNITTEST(PQ, queryForBooleanBlocking)
{ queryForParam("SELECT id FROM users WHERE awesome=$1", false, 1u, 2); }
MORDOR_UNITTEST(PQ, queryForBooleanAsync)
{ IOManager ioManager; queryForParam("SELECT id FROM users WHERE awesome=$1", false, 1u, 2, std::string(), &ioManager); }
MORDOR_UNITTEST(PQ, queryForBooleanPreparedBlocking)
{ queryForParam("SELECT id FROM users WHERE awesome=$1::boolean", false, 1u, 2, "constant"); }
MORDOR_UNITTEST(PQ, queryForBooleanPreparedAsync)
{ IOManager ioManager; queryForParam("SELECT id FROM users WHERE awesome=$1::boolean", false, 1u, 2, "constant", &ioManager); }

MORDOR_UNITTEST(PQ, queryForCharBlocking)
{ queryForParam("SELECT id FROM users WHERE gender=$1", 'M', 2u, 1); }
MORDOR_UNITTEST(PQ, queryForCharAsync)
{ IOManager ioManager; queryForParam("SELECT id FROM users WHERE gender=$1", 'M', 2u, 1, std::string(), &ioManager); }
MORDOR_UNITTEST(PQ, queryForCharPreparedBlocking)
{ queryForParam("SELECT id FROM users WHERE gender=$1::CHAR", 'M', 2u, 1, "constant"); }
MORDOR_UNITTEST(PQ, queryForCharPreparedAsync)
{ IOManager ioManager; queryForParam("SELECT id FROM users WHERE gender=$1::CHAR", 'M', 2u, 1, "constant", &ioManager); }

MORDOR_UNITTEST(PQ, queryForFloatBlocking)
{ queryForParam("SELECT efficiency FROM users WHERE efficiency=$1", .9f, 2u, .9f); }
MORDOR_UNITTEST(PQ, queryForFloatAsync)
{ IOManager ioManager; queryForParam("SELECT efficiency FROM users WHERE efficiency=$1", .9f, 2u, .9f, std::string(), &ioManager); }
MORDOR_UNITTEST(PQ, queryForFloatPreparedBlocking)
{ queryForParam("SELECT efficiency FROM users WHERE efficiency=$1::REAL", .9f, 2u, .9f, "constant"); }
MORDOR_UNITTEST(PQ, queryForFloatPreparedAsync)
{ IOManager ioManager; queryForParam("SELECT efficiency FROM users WHERE efficiency=$1::REAL", .9f, 2u, .9f, "constant", &ioManager); }

MORDOR_UNITTEST(PQ, queryForDoubleBlocking)
{ queryForParam("SELECT crazy FROM users WHERE crazy=$1", .75, 1u, .75); }
MORDOR_UNITTEST(PQ, queryForDoubleAsync)
{ IOManager ioManager; queryForParam("SELECT crazy FROM users WHERE crazy=$1", .75, 1u, .75, std::string(), &ioManager); }
MORDOR_UNITTEST(PQ, queryForDoublePreparedBlocking)
{ queryForParam("SELECT crazy FROM users WHERE crazy=$1::DOUBLE PRECISION", .75, 1u, .75, "constant"); }
MORDOR_UNITTEST(PQ, queryForDoublePreparedAsync)
{ IOManager ioManager; queryForParam("SELECT crazy FROM users WHERE crazy=$1::DOUBLE PRECISION", .75, 1u, .75, "constant", &ioManager); }

static const boost::posix_time::ptime thetime(
    boost::gregorian::date(2009, 05, 19),
    boost::posix_time::hours(15) + boost::posix_time::minutes(53) +
    boost::posix_time::seconds(45) + boost::posix_time::microseconds(123456));

static const boost::posix_time::ptime nulltime;

MORDOR_UNITTEST(PQ, queryForTimestampBlocking)
{ queryForParam("SELECT sometime FROM users WHERE sometime=$1", thetime, 1u, thetime); }
MORDOR_UNITTEST(PQ, queryForTimestampAsync)
{ IOManager ioManager; queryForParam("SELECT sometime FROM users WHERE sometime=$1", thetime, 1u, thetime, std::string(), &ioManager); }
MORDOR_UNITTEST(PQ, queryForTimestampPreparedBlocking)
{ queryForParam("SELECT sometime FROM users WHERE sometime=$1::TIMESTAMP", thetime, 1u, thetime, "constant"); }
MORDOR_UNITTEST(PQ, queryForTimestampPreparedAsync)
{ IOManager ioManager; queryForParam("SELECT sometime FROM users WHERE sometime=$1::TIMESTAMP", thetime, 1u, thetime, "constant", &ioManager); }
MORDOR_UNITTEST(PQ, queryForNullTimestampBlocking)
{ queryForParam("SELECT sometime FROM users WHERE sometime IS NULL OR sometime=$1", nulltime, 1u, nulltime); }
MORDOR_UNITTEST(PQ, queryForNullTimestampAsync)
{ IOManager ioManager; queryForParam("SELECT sometime FROM users WHERE sometime IS NULL OR sometime=$1", nulltime, 1u, nulltime, std::string(), &ioManager); }
MORDOR_UNITTEST(PQ, queryForNullTimestampPreparedBlocking)
{ queryForParam("SELECT sometime FROM users WHERE sometime IS NULL OR sometime=$1", nulltime, 1u, nulltime, "constant"); }
MORDOR_UNITTEST(PQ, queryForNullTimestampPreparedAsync)
{ IOManager ioManager; queryForParam("SELECT sometime FROM users WHERE sometime IS NULL OR sometime=$1", nulltime, 1u, nulltime, "constant", &ioManager); }

MORDOR_UNITTEST(PQ, transactionCommits)
{
    Connection conn(g_goodConnString);
    fillUsers(conn);
    Transaction t(conn);
    conn.execute("UPDATE users SET name='tom' WHERE id=1");
    t.commit();
    Result result = conn.execute("SELECT name FROM users WHERE id=1");
    MORDOR_TEST_ASSERT_EQUAL(result.rows(), 1u);
    MORDOR_TEST_ASSERT_EQUAL(result.columns(), 1u);
    MORDOR_TEST_ASSERT_EQUAL(result.get<const char *>(0, 0), "tom");
}

MORDOR_UNITTEST(PQ, transactionRollsback)
{
    Connection conn(g_goodConnString);
    fillUsers(conn);
    Transaction t(conn);
    conn.execute("UPDATE users SET name='tom' WHERE id=1");
    t.rollback();
    Result result = conn.execute("SELECT name FROM users WHERE id=1");
    MORDOR_TEST_ASSERT_EQUAL(result.rows(), 1u);
    MORDOR_TEST_ASSERT_EQUAL(result.columns(), 1u);
    MORDOR_TEST_ASSERT_EQUAL(result.get<const char *>(0, 0), "cody");
}

MORDOR_UNITTEST(PQ, transactionRollsbackAutomatically)
{
    Connection conn(g_goodConnString);
    fillUsers(conn);
    {
        Transaction t(conn);
        conn.execute("UPDATE users SET name='tom' WHERE id=1");
    }
    Result result = conn.execute("SELECT name FROM users WHERE id=1");
    MORDOR_TEST_ASSERT_EQUAL(result.rows(), 1u);
    MORDOR_TEST_ASSERT_EQUAL(result.columns(), 1u);
    MORDOR_TEST_ASSERT_EQUAL(result.get<const char *>(0, 0), "cody");
}

static void copyIn(IOManager *ioManager = NULL)
{
    Connection conn(g_goodConnString, ioManager);
    conn.execute("CREATE TEMP TABLE stuff (id INTEGER, name TEXT)");
    Stream::ptr stream = conn.copyIn("stuff").csv()();
    stream->write("1,cody\n");
    stream->write("2,tom\n");
    stream->write("3,brian\n");
    stream->write("4,jeremy\n");
    stream->write("5,zach\n");
    stream->write("6,paul\n");
    stream->write("7,alen\n");
    stream->write("8,jt\n");
    stream->write("9,jon\n");
    stream->write("10,jacob\n");
    stream->close();
    Result result = conn.execute("SELECT COUNT(*) FROM stuff");
    MORDOR_TEST_ASSERT_EQUAL(result.rows(), 1u);
    MORDOR_TEST_ASSERT_EQUAL(result.columns(), 1u);
    MORDOR_TEST_ASSERT_EQUAL(result.get<long long>(0, 0), 10);
    result = conn.execute("SELECT SUM(id) FROM stuff");
    MORDOR_TEST_ASSERT_EQUAL(result.rows(), 1u);
    MORDOR_TEST_ASSERT_EQUAL(result.columns(), 1u);
    MORDOR_TEST_ASSERT_EQUAL(result.get<long long>(0, 0), 55);
}

MORDOR_UNITTEST(PQ, copyInBlocking)
{ copyIn(); }

MORDOR_UNITTEST(PQ, copyInAsync)
{ IOManager ioManager; copyIn(&ioManager); }

static void copyOut(IOManager *ioManager = NULL)
{
    Connection conn(g_goodConnString, ioManager);
    conn.execute("CREATE TEMP TABLE country (code TEXT, name TEXT)");
    PreparedStatement stmt = conn.prepare("INSERT INTO country VALUES($1, $2)",
        "insertcountry");
    Transaction transaction(conn);
    stmt.execute("AF", "AFGHANISTAN");
    stmt.execute("AL", "ALBANIA");
    stmt.execute("DZ", "ALGERIA");
    stmt.execute("ZM", "ZAMBIA");
    stmt.execute("ZW", "ZIMBABWE");

    Stream::ptr stream = conn.copyOut("country").csv().delimiter('|')();
    MemoryStream output;
    transferStream(stream, output);
    MORDOR_ASSERT(output.buffer() ==
        "AF|AFGHANISTAN\n"
        "AL|ALBANIA\n"
        "DZ|ALGERIA\n"
        "ZM|ZAMBIA\n"
        "ZW|ZIMBABWE\n");
}

MORDOR_UNITTEST(PQ, copyOutBlocking)
{ copyOut(); }

MORDOR_UNITTEST(PQ, copyOutAsync)
{ IOManager ioManager; copyOut(&ioManager); }
