#ifndef __MORDOR_TEST_COMPOUND_LISTENER_H__
#define __MORDOR_TEST_COMPOUND_LISTENER_H__

#include <boost/shared_ptr.hpp>
#include <vector>

#include "test.h"

namespace Mordor {
namespace Test {

class CompoundListener : public TestListener
{
public:
    void addListener(boost::shared_ptr<TestListener> listener);

    void testStarted(const std::string &suite,
        const std::string &test);
    void testComplete(const std::string &suite,
        const std::string &test);
    void testSkipped(const std::string &suite,
        const std::string &tests);
    void testAsserted(const std::string &suite,
        const std::string &test, const Assertion &message);
    void testException(const std::string &suite,
        const std::string &test);
    void testsComplete();

private:
    std::vector<boost::shared_ptr<TestListener> > m_listeners;
};

}}

#endif
