// Copyright (c) 2009 - Decho Corp.

#include "test.h"

#include <cassert>
#include <iostream>

#include "mordor/common/version.h"

#ifdef WINDOWS
#include <windows.h>
#endif

static TestSuites *g_allTests;

static struct CleanupAllTests {
    ~CleanupAllTests()
    {
        if (g_allTests)
            delete g_allTests;
    }
} g_cleanupAllTests;

void
registerTest(const std::string &suite, const std::string &testName,
             TestDg test)
{
    if (!g_allTests)
        g_allTests = new TestSuites();
    (*g_allTests)[suite].second[testName] = test;
}

void
registerSuiteInvariant(const std::string &suite, TestDg invariant)
{
    if (!g_allTests)
        g_allTests = new TestSuites();
    assert((*g_allTests)[suite].first == NULL);
    (*g_allTests)[suite].first = invariant;
}


class TestAssertion : public std::exception
{
public:
    TestAssertion(const char *file, int line, const std::string &msg)
        : m_file(file), m_line(line), m_msg(msg)
    {}
   ~TestAssertion() throw() {}

    const char *file() const { return m_file; }
    int line() const { return m_line; }
    const char *what() const throw() { return m_msg.c_str(); }

private:
    const char *m_file;
    int m_line;
    std::string m_msg;
};

void
assertion(const char *file, int line, const std::string &expr)
{
    throw TestAssertion(file, line, expr);
}

bool runTest(TestListener *listener, const std::string &suite,
             const std::string &testName, TestDg test)
{
    if (listener)
        listener->testStarted(suite, testName);
    
    bool protect = true;
#ifdef WINDOWS
    protect = !IsDebuggerPresent();
#endif
    if (protect) {
        try {
            test();
            if (listener)
                listener->testComplete(suite, testName);
        } catch (const TestAssertion &assertion) {
            std::ostringstream os;
            os << "Assertion failed (" << assertion.file() << ":"
               << assertion.line() << "):" << std::endl << assertion.what()
               << std::endl;
            if (listener)
                listener->testAsserted(suite, testName, os.str());
            return false;
        } catch (std::exception &ex) {
            if (listener)
                listener->testException(suite, testName, ex);
            return false;
        } catch (...) {
            if (listener)
                listener->testUnknownException(suite, testName);
            return false;
        }
    } else {
        test();
        if (listener)
            listener->testComplete(suite, testName);
    }
    return true;
}

bool
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
                result = result && runTest(listener, it->first, it2->first,
                    it2->second);
            }
            if (it->second.first) {
                result = result && runTest(listener, it->first,
                    "<invariant>", it->second.first);
            }
        }
    }
    if (listener)
        listener->testsComplete();
    return result;
}

const TestSuites &
allTests()
{
    if (!g_allTests)
        g_allTests = new TestSuites();
    return *g_allTests;
}

TestSuites
testsForArguments(int argc, char **argv)
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
runTests()
{
    return runTests(g_allTests, NULL);
}

bool
runTests(const TestSuites &suites)
{
    return runTests(&suites, NULL);
}

bool
runTests(TestListener &listener)
{
    return runTests(g_allTests, &listener);
}

bool
runTests(const TestSuites &suites, TestListener &listener)
{
    return runTests(&suites, &listener);
}
