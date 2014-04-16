// Copyright (c) 2011 - Mozy, Inc.

#include "mordor/config.h"
#include "mordor/test/test.h"

#ifdef HAVE_CONFIG_H
#include "autoconfig.h"
#endif

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

MORDOR_UNITTEST(Config, configVarNameRules)
{
    // valid name
    MORDOR_TEST_ASSERT(Config::lookup("validname1.test", 0));
    MORDOR_TEST_ASSERT(Config::lookup("a1.2x", 0));
    // This is a little corner case that still valid
    // Currently I don't think we are trying to use this specific case,
    // but it could be still useful, for example, some env var can be
    // named as A_2.
    MORDOR_TEST_ASSERT(Config::lookup("a.2", 0));

    MORDOR_TEST_ASSERT_EXCEPTION(Config::lookup("1a", 0), std::invalid_argument);
    MORDOR_TEST_ASSERT_EXCEPTION(Config::lookup("Aa", 0), std::invalid_argument);
    MORDOR_TEST_ASSERT_EXCEPTION(Config::lookup("a_", 0), std::invalid_argument);
    MORDOR_TEST_ASSERT_EXCEPTION(Config::lookup("a.", 0), std::invalid_argument);
    MORDOR_TEST_ASSERT_EXCEPTION(Config::lookup("a_b", 0), std::invalid_argument);
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

MORDOR_UNITTEST(Config, configVarNameFromJSONValid)
{
    std::ostringstream ss;
    ss << "s3:\n"
       << "    host: 192.168.1.1\n";
    JSON::Value json = YAML::parse(ss.str());
    MORDOR_TEST_ASSERT(!json["s3"]["host"].isBlank());
    Config::loadFromJSON(json);
    MORDOR_TEST_ASSERT(Config::lookup("s3.host"));
    MORDOR_TEST_ASSERT_EQUAL(Config::lookup("s3.host")->toString(), "192.168.1.1");
}

MORDOR_UNITTEST(Config, configVarNameFromJSONInvalid)
{
    // dot (.) can't be used as ConfigVar name in loadFromJSON case
    std::ostringstream ss("s3.host: 192.168.1.1\n");
    JSON::Value json = YAML::parse(ss.str());
    MORDOR_TEST_ASSERT(json.find("s3.host") != json.end());
    Config::loadFromJSON(json);
    MORDOR_TEST_ASSERT(!Config::lookup("s3.host"));
}
#endif

