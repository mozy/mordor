// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/predef.h"

#include "test.h"

#include <iostream>

#include <boost/regex.hpp>

#include "mordor/config.h"
#include "mordor/sleep.h"

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

static ConfigVar<bool>::ptr g_protect = Config::lookup(
    "test.protect", false,
    "Protect test while running under a debugger");
static ConfigVar<bool>::ptr g_wait = Config::lookup(
    "test.waitfordebugger", false,
    "Wait for a debugger to attach before running tests");

#ifdef WINDOWS
#elif defined(LINUX)
static bool IsDebuggerPresent()
{
    bool result = false;
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
                    result = true;
                }
            }
        }
        close(fd);
    }
    return result;
}
#elif defined(OSX)
static bool IsDebuggerPresent()
{
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
    return !!(info.kp_proc.p_flag & P_TRACED);
}
#else
static bool IsDebuggerPresent()
{
    return false;
}
#endif

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
Test::assertion(const char *file, int line, const char *function,
                const std::string &expr)
{
    throw boost::enable_current_exception(Assertion(expr))
        << boost::throw_file(file) << boost::throw_line(line)
        << boost::throw_function(function)
        << errinfo_backtrace(backtrace());
}

static bool
runTest(TestListener *listener, const std::string &suite,
             const std::string &testName, TestDg test)
{
    if (listener)
        listener->testStarted(suite, testName);

    bool protect = !IsDebuggerPresent();
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
    Assertion::throwOnAssertion = true;
    if (g_wait->val()) {
        while (!IsDebuggerPresent())
            sleep(10000ull);
#ifdef WINDOWS
        DebugBreak();
#endif
    }
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
        boost::regex regex("^" + std::string(argv[i]) + "$");
        for (TestSuites::const_iterator j(all.begin());
            j != all.end();
            ++j) {
            if (boost::regex_match(j->first, regex)) {
                tests[j->first] = j->second;
            } else {
                for (std::map<std::string, TestDg>::const_iterator k(j->second.second.begin());
                    k != j->second.second.end();
                    ++k) {
                    if (boost::regex_match(j->first + "::" + k->first, regex)) {
                        tests[j->first].first = j->second.first;
                        tests[j->first].second[k->first] = k->second;
                    }
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
    int line, const char *function, const char *lhs, const char *rhs,
    const char *lhsExpr, const char *rhsExpr)
{
    if (!(strcmp(lhs, rhs) == 0)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            "==");
    }
}

template <>
void Test::assertNotEqual<const char *, const char *>(const char *file,
    int line, const char *lhs, const char *function, const char *rhs,
    const char *lhsExpr, const char *rhsExpr)
{
    if (!(strcmp(lhs, rhs) != 0)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            "!=");
    }
}

template <>
void Test::assertLessThan<const char *, const char *>(const char *file,
    int line, const char *function, const char *lhs, const char *rhs,
    const char *lhsExpr, const char *rhsExpr)
{
    if (!(strcmp(lhs, rhs) < 0)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            "<");
    }
}

template <>
void Test::assertLessThanOrEqual<const char *, const char *>(const char *file,
    int line, const char *function, const char *lhs, const char *rhs,
    const char *lhsExpr, const char *rhsExpr)
{
    if (!(strcmp(lhs, rhs) <= 0)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            "<=");
    }
}

template <>
void Test::assertGreaterThan<const char *, const char *>(const char *file,
    int line, const char *function, const char *lhs, const char *rhs,
    const char *lhsExpr, const char *rhsExpr)
{
    if (!(strcmp(lhs, rhs) > 0)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            ">");
    }
}

template <>
void Test::assertGreaterThanOrEqual<const char *, const char *>(const char *file,
    int line, const char *function, const char *lhs, const char *rhs,
    const char *lhsExpr, const char *rhsExpr)
{
    if (!(strcmp(lhs, rhs) == 0)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            ">=");
    }
}
