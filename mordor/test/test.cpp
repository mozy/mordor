// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "test.h"

#include <iostream>

#include "mordor/config.h"

#ifdef WINDOWS
#include <windows.h>
#elif defined (LINUX)
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#elif defined (OSX)
#include <sys/sysctl.h>
#endif

using namespace Mordor;
using namespace Mordor::Test;

static TestSuites *g_allTests;
#ifdef LINUX
static bool g_traced;
#endif

static ConfigVar<bool>::ptr g_protect = Config::lookup(
    "test.protect", false,
    "Protect test while running under a debugger");

namespace {
static struct Initializer {
#ifdef LINUX
    Initializer()
    {
        char buffer[1024];
        snprintf(buffer, 1024, "/proc/%d/status", getpid());
        int fd = open(buffer, O_RDONLY);
        if (fd >= 0) {
            int rc = read(fd, buffer, 1024);
            if (rc > 0) {
                const char *tracerPidStr = strstr(buffer, "TracerPid:");
                if (tracerPidStr) {
                    int tracingPid = atoi(tracerPidStr + 13);
                    if (tracingPid != 0) {
                        g_traced = true;
                    }
                }
            }
            close(fd);
        }
    }
#endif
    ~Initializer()
    {
        if (g_allTests)
            delete g_allTests;
    }
} g_init;
}

void
Test::registerTest(const std::string &suite, const std::string &testName,
             TestDg test)
{
    if (!g_allTests)
        g_allTests = new TestSuites();
    (*g_allTests)[suite].second[testName] = test;
}

void
Test::registerSuiteInvariant(const std::string &suite, TestDg invariant)
{
    if (!g_allTests)
        g_allTests = new TestSuites();
    MORDOR_ASSERT((*g_allTests)[suite].first == NULL);
    (*g_allTests)[suite].first = invariant;
}

void
Test::assertion(const char *file, int line, const std::string &expr)
{
    throw Assertion(expr) << boost::throw_file(file) << boost::throw_line(line)
        << errinfo_backtrace(backtrace());
}

static bool
runTest(TestListener *listener, const std::string &suite,
             const std::string &testName, TestDg test)
{
    if (listener)
        listener->testStarted(suite, testName);
    
    bool protect = true;
#ifdef WINDOWS
    protect = !IsDebuggerPresent();
#elif defined (LINUX)
    protect = !g_traced;
#elif defined (OSX)
    int mib[4];
    kinfo_proc info;
    size_t size;
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = getpid();
    size = sizeof(kinfo_proc);
    info.kp_proc.p_flag = 0;
    sysctl(mib, 4, &info, &size, NULL, 0);
    protect = !(info.kp_proc.p_flag & P_TRACED);
#endif
    protect = protect || g_protect->val();
    if (protect) {
        try {
            test();
            if (listener)
                listener->testComplete(suite, testName);
        } catch (const Assertion &assertion) {
            if (listener)
                listener->testAsserted(suite, testName, assertion);
            return false;
        } catch (...) {
            if (listener)
                listener->testException(suite, testName);
            return false;
        }
    } else {
        test();
        if (listener)
            listener->testComplete(suite, testName);
    }
    return true;
}

static bool
runTests(const TestSuites *suites, TestListener *listener)
{
    bool result = true;
    if (!suites) suites = g_allTests;
    if (suites) {
        for (TestSuites::const_iterator it(suites->begin());
            it != suites->end();
            ++it) {
            for (TestSuite::second_type::const_iterator
                    it2(it->second.second.begin());
                it2 != it->second.second.end();
                ++it2) {
                if (it->second.first) {
                    result = result && runTest(listener, it->first,
                        "<invariant>", it->second.first);
                }
                result = runTest(listener, it->first, it2->first,
                    it2->second) && result;
            }
            if (it->second.first) {
                result = runTest(listener, it->first,
                    "<invariant>", it->second.first) && result;
            }
        }
    }
    if (listener)
        listener->testsComplete();
    return result;
}

const TestSuites &
Test::allTests()
{
    if (!g_allTests)
        g_allTests = new TestSuites();
    return *g_allTests;
}

TestSuites
Test::testsForArguments(int argc, const char **argv)
{
    TestSuites tests;
    const TestSuites &all = allTests();
    for (int i = 0; i < argc; ++i) {
        std::string suite = argv[i];
        std::string test;
        size_t offset = suite.find("::");
        if (offset != std::string::npos) {
            test = suite.substr(offset + 2);
            suite = suite.substr(0, offset);
        }
        TestSuites::const_iterator suiteIt = all.find(suite);
        if (suiteIt != all.end()) {
            if (test.empty()) {
                tests[suite] = suiteIt->second;
            } else {
                TestSuite::second_type::const_iterator testIt =
                    suiteIt->second.second.find(test);
                if (testIt != suiteIt->second.second.end()) {
                    tests[suite].first = suiteIt->second.first;
                    tests[suite].second[test] = testIt->second;
                }
            }                
        }
    }
    return tests;
}

bool
Test::runTests()
{
    return ::runTests(g_allTests, NULL);
}

bool
Test::runTests(const TestSuites &suites)
{
    return ::runTests(&suites, NULL);
}

bool
Test::runTests(TestListener &listener)
{
    return ::runTests(g_allTests, &listener);
}

bool
Test::runTests(const TestSuites &suites, TestListener &listener)
{
    return ::runTests(&suites, &listener);
}

template <>
void Test::assertEqual<const char *, const char *>(const char *file,
    int line, const char *lhs, const char *rhs, const char *lhsExpr,
    const char *rhsExpr)
{
    if (!(strcmp(lhs, rhs) == 0)) {
        assertComparison(file, line, lhs, rhs, lhsExpr, rhsExpr, "==");
    }
}

template <>
void Test::assertNotEqual<const char *, const char *>(const char *file,
    int line, const char *lhs, const char *rhs, const char *lhsExpr,
    const char *rhsExpr)
{
    if (!(strcmp(lhs, rhs) != 0)) {
        assertComparison(file, line, lhs, rhs, lhsExpr, rhsExpr, "!=");
    }
}

template <>
void Test::assertLessThan<const char *, const char *>(const char *file,
    int line, const char *lhs, const char *rhs, const char *lhsExpr,
    const char *rhsExpr)
{
    if (!(strcmp(lhs, rhs) < 0)) {
        assertComparison(file, line, lhs, rhs, lhsExpr, rhsExpr, "<");
    }
}

template <>
void Test::assertLessThanOrEqual<const char *, const char *>(const char *file,
    int line, const char *lhs, const char *rhs, const char *lhsExpr,
    const char *rhsExpr)
{
    if (!(strcmp(lhs, rhs) <= 0)) {
        assertComparison(file, line, lhs, rhs, lhsExpr, rhsExpr, "<=");
    }
}

template <>
void Test::assertGreaterThan<const char *, const char *>(const char *file,
    int line, const char *lhs, const char *rhs, const char *lhsExpr,
    const char *rhsExpr)
{
    if (!(strcmp(lhs, rhs) > 0)) {
        assertComparison(file, line, lhs, rhs, lhsExpr, rhsExpr, ">");
    }
}

template <>
void Test::assertGreaterThanOrEqual<const char *, const char *>(const char *file,
    int line, const char *lhs, const char *rhs, const char *lhsExpr,
    const char *rhsExpr)
{
    if (!(strcmp(lhs, rhs) == 0)) {
        assertComparison(file, line, lhs, rhs, lhsExpr, rhsExpr, ">=");
    }
}
