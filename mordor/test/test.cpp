// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/predef.h"

#include "test.h"

#include <iostream>

#include <boost/regex.hpp>

#include "mordor/config.h"
#include "mordor/sleep.h"
#include "mordor/timer.h"

#ifdef WINDOWS
#include <windows.h>
#elif defined (LINUX)
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#elif defined (OSX)
#include <sys/sysctl.h>
#endif

namespace Mordor {
namespace Test {

static ConfigVar<bool>::ptr g_protect = Config::lookup(
    "test.protect", false,
    "Protect test while running under a debugger");
static ConfigVar<bool>::ptr g_wait = Config::lookup(
    "test.waitfordebugger", false,
    "Wait for a debugger to attach before running tests");

TestSuites &allTests()
{
    static TestSuites s_allTests;
    return s_allTests;
}

void
registerTest(const std::string &suite, const std::string &testName,
             TestDg test)
{
    allTests()[suite].second[testName] = test;
}

void
registerSuiteInvariant(const std::string &suite, TestDg invariant)
{
    MORDOR_ASSERT(allTests()[suite].first == NULL);
    allTests()[suite].first = invariant;
}

TimeConstraint::TimeConstraint(unsigned long long us)
    : m_us(us),
      m_start(TimerManager::now())
{}

TimeConstraint::~TimeConstraint()
{
    MORDOR_TEST_ASSERT_LESS_THAN_OR_EQUAL(TimerManager::now() - m_start, m_us);
}

void
assertion(const char *file, int line, const char *function,
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

    bool protect = !isDebuggerAttached();
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
        while (!isDebuggerAttached())
            sleep(10000ull);
        debugBreak();
    }
    bool result = true;
    if (!suites) suites = &allTests();
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

TestSuites
testsForArguments(int argc, char **argv)
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
runTests()
{
    return runTests(&allTests(), NULL);
}

bool
runTests(const TestSuites &suites)
{
    return runTests(&suites, NULL);
}

bool
runTests(TestListener &listener)
{
    return runTests(&allTests(), &listener);
}

bool
runTests(const TestSuites &suites, TestListener &listener)
{
    return runTests(&suites, &listener);
}

template <>
void assertEqual<const char *, const char *>(const char *file,
    int line, const char *function, const char *lhs, const char *rhs,
    const char *lhsExpr, const char *rhsExpr)
{
    if (!(strcmp(lhs, rhs) == 0)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            "==");
    }
}

template <>
void assertNotEqual<const char *, const char *>(const char *file,
    int line, const char *function, const char *lhs, const char *rhs,
    const char *lhsExpr, const char *rhsExpr)
{
    if (!(strcmp(lhs, rhs) != 0)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            "!=");
    }
}

template <>
void assertLessThan<const char *, const char *>(const char *file,
    int line, const char *function, const char *lhs, const char *rhs,
    const char *lhsExpr, const char *rhsExpr)
{
    if (!(strcmp(lhs, rhs) < 0)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            "<");
    }
}

template <>
void assertLessThanOrEqual<const char *, const char *>(const char *file,
    int line, const char *function, const char *lhs, const char *rhs,
    const char *lhsExpr, const char *rhsExpr)
{
    if (!(strcmp(lhs, rhs) <= 0)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            "<=");
    }
}

template <>
void assertGreaterThan<const char *, const char *>(const char *file,
    int line, const char *function, const char *lhs, const char *rhs,
    const char *lhsExpr, const char *rhsExpr)
{
    if (!(strcmp(lhs, rhs) > 0)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            ">");
    }
}

template <>
void assertGreaterThanOrEqual<const char *, const char *>(const char *file,
    int line, const char *function, const char *lhs, const char *rhs,
    const char *lhsExpr, const char *rhsExpr)
{
    if (!(strcmp(lhs, rhs) == 0)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            ">=");
    }
}

#ifdef WINDOWS
template <>
void assertEqual<const wchar_t *, const wchar_t *>(const char *file,
    int line, const char *function, const wchar_t *lhs, const wchar_t *rhs,
    const char *lhsExpr, const char *rhsExpr)
{
    if (!(wcscmp(lhs, rhs) == 0)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            "==");
    }
}

template <>
void assertNotEqual<const wchar_t *, const wchar_t *>(const char *file,
    int line, const char *function, const wchar_t *lhs, const wchar_t *rhs,
    const char *lhsExpr, const char *rhsExpr)
{
    if (!(wcscmp(lhs, rhs) != 0)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            "!=");
    }
}

template <>
void assertLessThan<const wchar_t *, const wchar_t *>(const char *file,
    int line, const char *function, const wchar_t *lhs, const wchar_t *rhs,
    const char *lhsExpr, const char *rhsExpr)
{
    if (!(wcscmp(lhs, rhs) < 0)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            "<");
    }
}

template <>
void assertLessThanOrEqual<const wchar_t *, const wchar_t *>(const char *file,
    int line, const char *function, const wchar_t *lhs, const wchar_t *rhs,
    const char *lhsExpr, const char *rhsExpr)
{
    if (!(wcscmp(lhs, rhs) <= 0)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            "<=");
    }
}

template <>
void assertGreaterThan<const wchar_t *, const wchar_t *>(const char *file,
    int line, const char *function, const wchar_t *lhs, const wchar_t *rhs,
    const char *lhsExpr, const char *rhsExpr)
{
    if (!(wcscmp(lhs, rhs) > 0)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            ">");
    }
}

template <>
void assertGreaterThanOrEqual<const wchar_t *, const wchar_t *>(const char *file,
    int line, const char *function, const wchar_t *lhs, const wchar_t *rhs,
    const char *lhsExpr, const char *rhsExpr)
{
    if (!(wcscmp(lhs, rhs) == 0)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            ">=");
    }
}
#endif

}}
