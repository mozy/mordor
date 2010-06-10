#ifndef __MORDOR_TEST_ANT_XML_LISTENER_H__
#define __MORDOR_TEST_ANT_XML_LISTENER_H__

#include <boost/shared_ptr.hpp>

#include "test.h"

namespace Mordor {
namespace Test {

class AntXMLLogSink;

class AntXMLListener : public TestListener
{
public:
    AntXMLListener(const std::string &directory);
    ~AntXMLListener();

    void testStarted(const std::string &suite,
        const std::string &test);
    void testComplete(const std::string &suite,
        const std::string &test);
    void testAsserted(const std::string &suite,
        const std::string &test, const Assertion &message);
    void testException(const std::string &suite,
        const std::string &test);
    void testsComplete();

private:
    struct TestInfo
    {
        TestInfo() : start(~0ull), end(~0ull) {}
        unsigned long long start;
        unsigned long long end;
        std::string exceptionMessage;
        std::string exceptionType;
        std::string exceptionDetails;
    };

    struct TestSuiteInfo
    {
        TestSuiteInfo() : start(~0ull), end(~0ull), errors(0), failures(0) {}
        unsigned long long start;
        unsigned long long end;
        size_t errors;
        size_t failures;
        std::map<std::string, TestInfo> tests;
        boost::shared_ptr<std::ostringstream> out;
        boost::shared_ptr<std::ostringstream> err;
    };

private:
    boost::shared_ptr<AntXMLLogSink> m_logSink;
    std::string m_directory;
    std::map<std::string, TestSuiteInfo > m_testSuites;
};

}}

#endif
