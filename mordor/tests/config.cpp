// Copyright (c) 2011 - Mozy, Inc.

#include "mordor/config.h"
#include "mordor/test/test.h"

#ifdef HAVE_LIBYAML
#include "mordor/yaml.h"
#endif

using namespace Mordor;
using namespace Mordor::Test;

static ConfigVar<int>::ptr g_testVar1 = Config::lookup(
    "config.test", 0, "Config var used by unit test");

MORDOR_UNITTEST(Config, loadFromCommandLineNull)
{
    int argc = 0;
    char **argv = NULL;
    Config::loadFromCommandLine(argc, argv);
    MORDOR_TEST_ASSERT_EQUAL(argc, 0);
    MORDOR_TEST_ASSERT(argv == NULL);
}

MORDOR_UNITTEST(Config, loadFromCommandLineEmpty)
{
    int argc = 1;
    std::string args[] = { "--config.test=1" };
    char *argv[1];
    for (int i = 0; i < argc; ++i)
        argv[i] = const_cast<char *>(args[i].c_str());
    g_testVar1->val(0);
    Config::loadFromCommandLine(argc, argv);
    // Didn't do anything, even though it looks like a config argument
    MORDOR_TEST_ASSERT_EQUAL(argc, 1);
    MORDOR_TEST_ASSERT_EQUAL((const char *)argv[0], "--config.test=1");
    MORDOR_TEST_ASSERT_EQUAL(g_testVar1->val(), 0);
}

MORDOR_UNITTEST(Config, loadFromCommandLineSimpleEquals)
{
    int argc = 2;
    std::string args[] = { "program",
                           "--config.test=1" };
    char *argv[2];
    for (int i = 0; i < argc; ++i)
        argv[i] = const_cast<char *>(args[i].c_str());
    g_testVar1->val(0);
    Config::loadFromCommandLine(argc, argv);
    MORDOR_TEST_ASSERT_EQUAL(argc, 1);
    MORDOR_TEST_ASSERT_EQUAL((const char *)argv[0], "program");
    MORDOR_TEST_ASSERT_EQUAL(g_testVar1->val(), 1);
}

MORDOR_UNITTEST(Config, loadFromCommandLineSimple)
{
    int argc = 3;
    std::string args[] = { "program",
                           "--config.test",
                           "1" };
    char *argv[3];
    for (int i = 0; i < argc; ++i)
        argv[i] = const_cast<char *>(args[i].c_str());
    g_testVar1->val(0);
    Config::loadFromCommandLine(argc, argv);
    MORDOR_TEST_ASSERT_EQUAL(argc, 1);
    MORDOR_TEST_ASSERT_EQUAL((const char *)argv[0], "program");
    MORDOR_TEST_ASSERT_EQUAL(g_testVar1->val(), 1);
}

MORDOR_UNITTEST(Config, loadFromCommandLineNoConfigVars)
{
    int argc = 3;
    std::string args[] = { "program",
                           "--notaconfigvar",
                           "norami" };
    char *argv[3];
    for (int i = 0; i < argc; ++i)
        argv[i] = const_cast<char *>(args[i].c_str());
    Config::loadFromCommandLine(argc, argv);
    MORDOR_TEST_ASSERT_EQUAL(argc, 3);
    MORDOR_TEST_ASSERT_EQUAL((const char *)argv[0], "program");
    MORDOR_TEST_ASSERT_EQUAL((const char *)argv[1], "--notaconfigvar");
    MORDOR_TEST_ASSERT_EQUAL((const char *)argv[2], "norami");
}

MORDOR_UNITTEST(Config, loadFromCommandLineStripConfigVars)
{
    int argc = 4;
    std::string args[] = { "program",
                           "--notaconfigvar",
                           "--config.test=1",
                           "norami" };
    char *argv[4];
    for (int i = 0; i < argc; ++i)
        argv[i] = const_cast<char *>(args[i].c_str());
    g_testVar1->val(0);
    Config::loadFromCommandLine(argc, argv);
    MORDOR_TEST_ASSERT_EQUAL(argc, 3);
    MORDOR_TEST_ASSERT_EQUAL((const char *)argv[0], "program");
    MORDOR_TEST_ASSERT_EQUAL((const char *)argv[1], "--notaconfigvar");
    MORDOR_TEST_ASSERT_EQUAL((const char *)argv[2], "norami");
    MORDOR_TEST_ASSERT_EQUAL(g_testVar1->val(), 1);
}

MORDOR_UNITTEST(Config, loadFromCommandLineStripConfigVars2)
{
    int argc = 5;
    std::string args[] = { "program",
                           "--notaconfigvar",
                           "--config.test",
                           "1",
                           "norami" };
    char *argv[5];
    for (int i = 0; i < argc; ++i)
        argv[i] = const_cast<char *>(args[i].c_str());
    g_testVar1->val(0);
    Config::loadFromCommandLine(argc, argv);
    MORDOR_TEST_ASSERT_EQUAL(argc, 3);
    MORDOR_TEST_ASSERT_EQUAL((const char *)argv[0], "program");
    MORDOR_TEST_ASSERT_EQUAL((const char *)argv[1], "--notaconfigvar");
    MORDOR_TEST_ASSERT_EQUAL((const char *)argv[2], "norami");
    MORDOR_TEST_ASSERT_EQUAL(g_testVar1->val(), 1);
}

MORDOR_UNITTEST(Config, loadFromCommandLineMissingArg)
{
    int argc = 2;
    std::string args[] = { "program",
                           "--config.test" };
    char *argv[2];
    for (int i = 0; i < argc; ++i)
        argv[i] = const_cast<char *>(args[i].c_str());
    MORDOR_TEST_ASSERT_EXCEPTION(Config::loadFromCommandLine(argc, argv),
        std::invalid_argument);
}

MORDOR_UNITTEST(Config, loadFromCommandLineBadArg)
{
    int argc = 2;
    std::string args[] = { "program",
                           "--config.test=bad" };
    char *argv[2];
    for (int i = 0; i < argc; ++i)
        argv[i] = const_cast<char *>(args[i].c_str());
    MORDOR_TEST_ASSERT_EXCEPTION(Config::loadFromCommandLine(argc, argv),
        std::invalid_argument);
}

MORDOR_UNITTEST(Config, loadFromCommandLineDuplicates)
{
    int argc = 4;
    std::string args[] = { "program",
                           "--config.test=1",
                           "--config.test=2",
                           "--config.test=3" };
    char *argv[4];
    for (int i = 0; i < argc; ++i)
        argv[i] = const_cast<char *>(args[i].c_str());
    g_testVar1->val(0);
    Config::loadFromCommandLine(argc, argv);
    MORDOR_TEST_ASSERT_EQUAL(argc, 1);
    MORDOR_TEST_ASSERT_EQUAL((const char *)argv[0], "program");
    MORDOR_TEST_ASSERT_EQUAL(g_testVar1->val(), 3);
}

MORDOR_UNITTEST(Config, loadFromCommandLineNoConfigVarsAfterDashDash)
{
    int argc = 4;
    std::string args[] = { "program",
                           "--config.test=1",
                           "--",
                           "--config.test=2" };
    char *argv[4];
    for (int i = 0; i < argc; ++i)
        argv[i] = const_cast<char *>(args[i].c_str());
    g_testVar1->val(0);
    Config::loadFromCommandLine(argc, argv);
    MORDOR_TEST_ASSERT_EQUAL(argc, 3);
    MORDOR_TEST_ASSERT_EQUAL((const char *)argv[0], "program");
    MORDOR_TEST_ASSERT_EQUAL((const char *)argv[1], "--");
    MORDOR_TEST_ASSERT_EQUAL((const char *)argv[2], "--config.test=2");
    MORDOR_TEST_ASSERT_EQUAL(g_testVar1->val(), 1);
}

#ifdef HAVE_LIBYAML
MORDOR_UNITTEST(Config, loadFromJSON)
{
    ConfigVar<int>::ptr port = Config::lookup("http.port", 8080, "");
    ConfigVar<int>::ptr quantity = Config::lookup("quantity", 123456, "");

    std::ostringstream ss;
    ss << "http:\n"
       << "    host: 192.168.0.1\n"
       << "    port: 80\n"
       << "quantity: 654321\n"
       << "price: 800.34";

    Config::loadFromJSON(YAML::parse(ss.str()));

    MORDOR_TEST_ASSERT_EQUAL(port->val(), 80);
    MORDOR_TEST_ASSERT_EQUAL(quantity->val(), 654321);

    MORDOR_TEST_ASSERT_EQUAL(Config::lookup("http.host")->toString(), "192.168.0.1");
    MORDOR_TEST_ASSERT_EQUAL(Config::lookup("price")->toString(), "800.34");
}
#endif

