// Copyright (c) 2010 - Mozy, Inc.

#include "mordor/predef.h"

#include "antxmllistener.h"

#include <iostream>

#include <boost/date_time/posix_time/posix_time_io.hpp>

#include "mordor/config.h"
#include "mordor/log.h"
#include "mordor/streams/file.h"
#include "mordor/string.h"
#include "mordor/timer.h"

namespace Mordor {
namespace Test {

class AntXMLLogSink : public LogSink
{
public:
    typedef boost::shared_ptr<AntXMLLogSink> ptr;

    AntXMLLogSink() : m_out(NULL), m_err(NULL)
    {}

    void log(const std::string &logger,
             boost::posix_time::ptime now, unsigned long long elapsed,
             tid_t thread, void *fiber,
             Log::Level level, const std::string &str,
             const char* file, int line)
    {
        std::ostringstream *os = NULL;
        std::ostringstream localOS;
        switch (level) {
            case Log::FATAL:
            case Log::ERROR:
                os = m_err;
                break;
            default:
                os = m_out;
        }
        if (os) {
            localOS << now << " " << elapsed << " " << level << " " << thread
                    << " " << fiber << " " << logger << " " << file << ":"
                    << line << " " << str << std::endl;
            boost::mutex::scoped_lock lock(m_mutex);
            *os << localOS.str();
        }
    }

    boost::mutex m_mutex;
    std::ostringstream *m_out, *m_err;
};

AntXMLListener::AntXMLListener(const std::string &directory)
: m_directory(directory)
{
    m_logSink.reset(new AntXMLLogSink());
    Log::root()->addSink(m_logSink);
}

AntXMLListener::~AntXMLListener()
{
    if (m_logSink)
        Log::root()->removeSink(m_logSink);
}

void
AntXMLListener::testStarted(const std::string &suite, const std::string &test)
{
    TestSuiteInfo &suiteInfo = m_testSuites[suite];
    if (!suiteInfo.out) {
        suiteInfo.out.reset(new std::ostringstream());
        suiteInfo.err.reset(new std::ostringstream());
    }
    m_logSink->m_out = suiteInfo.out.get();
    m_logSink->m_err = suiteInfo.err.get();
    if (test != "<invariant>") {
        TestInfo &testInfo = suiteInfo.tests[test];
        testInfo.start = TimerManager::now();
        if (suiteInfo.start == ~0ull)
            suiteInfo.start = testInfo.start;
    }
}

void
AntXMLListener::testComplete(const std::string &suite, const std::string &test)
{
    if (test != "<invariant>") {
        TestSuiteInfo &suiteInfo = m_testSuites[suite];
        TestInfo &testInfo = suiteInfo.tests[test];
        suiteInfo.end = testInfo.end = TimerManager::now();
    }
}

void
AntXMLListener::testSkipped(const std::string &suite, const std::string &test)
{
    if (test != "<invariant>") {
        TestSuiteInfo &suiteInfo = m_testSuites[suite];
        suiteInfo.tests.erase(test);
        suiteInfo.end = TimerManager::now();
    }
}

void
AntXMLListener::testAsserted(const std::string &suite, const std::string &test,
                             const Assertion &assertion)
{
    if (test != "<invariant>") {
        TestSuiteInfo &suiteInfo = m_testSuites[suite];
        TestInfo &testInfo = suiteInfo.tests[test];
        ++suiteInfo.failures;
        suiteInfo.end = testInfo.end = TimerManager::now();
        testInfo.exceptionType = "Assertion";
        testInfo.exceptionMessage = assertion.what();
        replace(testInfo.exceptionMessage, "&", "&amp;");
        replace(testInfo.exceptionMessage, "\"", "&quot;");
        testInfo.exceptionDetails = boost::current_exception_diagnostic_information();
    }
}

void
AntXMLListener::testException(const std::string &suite, const std::string &test)
{
    if (test != "<invariant>") {
        TestSuiteInfo &suiteInfo = m_testSuites[suite];
        TestInfo &testInfo = suiteInfo.tests[test];
        ++suiteInfo.errors;
        suiteInfo.end = testInfo.end = TimerManager::now();
        try {
            throw;
        } catch (std::exception &ex) {
            testInfo.exceptionMessage = ex.what();
            testInfo.exceptionType = typeid(ex).name();
        } catch (boost::exception &ex) {
            testInfo.exceptionType = typeid(ex).name();
        } catch (...) {
        }
        testInfo.exceptionMessage = boost::current_exception_diagnostic_information();
    }
}

static void listProperties(std::ostringstream *os, ConfigVarBase::ptr var)
{
    *os << "    <property name=\"" << var->name() << "\" value=\""
        << var->toString() << "\" />" << std::endl;
}

static std::string sanitize(std::string string, bool cdata = true)
{
    replace(string, '&', "&amp;");
    replace(string, '<', "&lt;");
    if (!cdata)
        replace(string, '\"', "&quot;");
    return string;
}

void
AntXMLListener::testsComplete()
{
    for (std::map<std::string, TestSuiteInfo>::const_iterator it = m_testSuites.begin();
        it != m_testSuites.end();
        ++it) {
        try {
            FileStream file(m_directory + "TEST-" + it->first + ".xml",
                FileStream::WRITE, FileStream::OVERWRITE_OR_CREATE);
            std::ostringstream os;
            os << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>" << std::endl
                << "<testsuite errors=\"" << it->second.errors
                << "\" failures=\"" << it->second.failures << "\" name=\""
                << it->first << "\" tests=\"" << it->second.tests.size()
                << "\" time=\""
                << (double)(it->second.end - it->second.start) / 1000000ull
                << "\">" << std::endl
                << "  <properties>" << std::endl;
            Config::visit(boost::bind(&listProperties, &os, _1));
            os << "  </properties>" << std::endl;
            for (std::map<std::string, TestInfo>::const_iterator it2 = it->second.tests.begin();
                it2 != it->second.tests.end();
                ++it2) {
                os << "  <testcase name=\"" << it2->first << "\" time=\""
                    << (double)(it2->second.end - it2->second.start) / 1000000ull
                    << "\"";
                if (!it2->second.exceptionMessage.empty()) {
                    os << ">" << std::endl
                        << "    <failure message=\""
                        << sanitize(it2->second.exceptionMessage, false) << "\" type=\""
                        << sanitize(it2->second.exceptionType, false) << "\"><![CDATA["
                        << sanitize(it2->second.exceptionDetails) << "]]></failure>"
                        << std::endl << "  </testcase>" << std::endl;
                } else {
                    os << " />" << std::endl;
                }
            }
            os << "  <system-out><![CDATA[" << sanitize(it->second.out->str())
                << "]]></system-out>" << std::endl
                << "  <system-err><![CDATA[" << sanitize(it->second.err->str())
                << "]]></system-err>" << std::endl
                << "</testsuite>" << std::endl;
            std::string xml = os.str();
            size_t written = 0;
            while (written < xml.size())
                written += file.write(xml.c_str() + written,
                    xml.size() - written);
        } catch (...) {
            std::cerr << boost::current_exception_diagnostic_information();
        }
    }
}

}}
