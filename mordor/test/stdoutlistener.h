#ifndef __STDOUT_LISTENER_H__
#define __STDOUT_LISTENER_H__

#include "test.h"

class StdoutListener : public TestListener
{
public:
    StdoutListener();

    void testStarted(const std::string &suite,
        const std::string &test);
    void testComplete(const std::string &suite,
        const std::string &test);
    void testAsserted(const std::string &suite,
        const std::string &test, const std::string &message);
    void testException(const std::string &suite,
        const std::string &test, const std::exception &ex);
    void testUnknownException(const std::string &suite,
        const std::string &test);
    void testsComplete();

private:
    size_t m_tests;
    size_t m_success;
};

#endif
