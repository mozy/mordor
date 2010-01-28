// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "mordor/log.h"
#include "mordor/test/test.h"

using namespace Mordor;

class TestLogSink : public LogSink
{
public:
    typedef boost::shared_ptr<TestLogSink> ptr;

    void log(const std::string &logger,
             boost::posix_time::ptime now, unsigned long long elapsed,
             tid_t thread, void *fiber,
             Log::Level level, const std::string &str,
             const char* file, int line)
    {
        m_str = str;
    }

    std::string m_str;
};

static void testLogger(const std::vector<Logger::ptr> &loggers)
{
    if(loggers.empty())
        return;
    std::vector<TestLogSink::ptr> sinks;
    sinks.resize(loggers.size());
    for(size_t i = 0; i < loggers.size(); ++i) {
        sinks[i].reset(new TestLogSink());
        loggers[i]->addSink(sinks[i]);
    }
    for (size_t i = 0; i < loggers.size(); i++) {
        MORDOR_LOG_INFO(loggers[i]) << "Hello world";
        for (size_t j = 0; j <= i; ++j) {
            MORDOR_TEST_ASSERT_EQUAL(sinks[j]->m_str, "Hello world");
            sinks[j]->m_str.clear();
        }
        for (size_t j = i + 1; j < loggers.size(); ++j)
            MORDOR_TEST_ASSERT(sinks[j]->m_str.empty());
    }
    for (size_t i = 0; i < loggers.size(); i++)
        loggers[i]->clearSinks();
}

MORDOR_UNITTEST(Log, two_asc)
{
    std::vector<Logger::ptr> vec;
    vec.push_back(Log::lookup("twoasc"));
    vec.push_back(Log::lookup("twoasc:child"));
    testLogger(vec);
}

MORDOR_UNITTEST(Log, two_des){
    std::vector<Logger::ptr> vec;
    Logger::ptr a = Log::lookup("twodes:child");
    Logger::ptr b = Log::lookup("twodes");
    vec.push_back(b);
    vec.push_back(a);
    testLogger(vec);
}

MORDOR_UNITTEST(Log, three_asc)
{
    std::vector<Logger::ptr> vec;
    vec.push_back(Log::lookup("threeasc"));
    vec.push_back(Log::lookup("threeasc:child1"));
    Log::lookup("threeasc:child1:child2");
    vec.push_back(Log::lookup("threeasc:child1:child2"));
    testLogger(vec);
}

MORDOR_UNITTEST(Log, three_des1){
    std::vector<Logger::ptr> vec;
    Logger::ptr a = Log::lookup("threedes1:child1:child2");
    Logger::ptr b = Log::lookup("threedes1:child1");
    Logger::ptr c = Log::lookup("threedes1");
    vec.push_back(c);
    vec.push_back(b);
    vec.push_back(a);
    testLogger(vec);
}

MORDOR_UNITTEST(Log, three_des2){
    std::vector<Logger::ptr> vec;
    Logger::ptr b = Log::lookup("threedes2:child1");
    Logger::ptr a = Log::lookup("threedes2:child1:child2");
    Logger::ptr c = Log::lookup("threedes2");
    vec.push_back(c);
    vec.push_back(b);
    vec.push_back(a);
    testLogger(vec);
}

MORDOR_UNITTEST(Log, multi_children){
    std::vector<Logger::ptr> vec;
    Logger::ptr c1 = Log::lookup("mc:mc1:::::c1::");
    Logger::ptr c2 = Log::lookup("mc:mc1:c2");
    Logger::ptr g = Log::lookup("mc");
    Logger::ptr p = Log::lookup("mc:mc1");
    Logger::ptr cc = Log::lookup("mc:mc1:c1:cc1");

    vec.push_back(g);
    vec.push_back(p);
    vec.push_back(c1);
    vec.push_back(cc);
    testLogger(vec);
    vec.clear();
    vec.push_back(p);
    vec.push_back(c2);
    testLogger(vec);
}
