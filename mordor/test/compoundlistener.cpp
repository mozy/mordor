// Copyright (c) 2010 - Mozy, Inc.

#include "mordor/predef.h"

#include "compoundlistener.h"

using namespace Mordor;
using namespace Mordor::Test;

void
CompoundListener::addListener(boost::shared_ptr<TestListener> listener)
{
    m_listeners.push_back(listener);
}

void
CompoundListener::testStarted(const std::string &suite, const std::string &test)
{
    for (std::vector<boost::shared_ptr<TestListener> >::const_iterator it =
        m_listeners.begin();
        it != m_listeners.end();
        ++it)
        (*it)->testStarted(suite, test);
}

void
CompoundListener::testComplete(const std::string &suite, const std::string &test)
{
    for (std::vector<boost::shared_ptr<TestListener> >::const_iterator it =
        m_listeners.begin();
        it != m_listeners.end();
        ++it)
        (*it)->testComplete(suite, test);
}

void
CompoundListener::testSkipped(const std::string &suite, const std::string &test)
{
    for (std::vector<boost::shared_ptr<TestListener> >::const_iterator it =
        m_listeners.begin();
        it != m_listeners.end();
        ++it)
        (*it)->testSkipped(suite, test);
}

void
CompoundListener::testAsserted(const std::string &suite, const std::string &test,
                             const Assertion &assertion)
{
    for (std::vector<boost::shared_ptr<TestListener> >::const_iterator it =
        m_listeners.begin();
        it != m_listeners.end();
        ++it)
        (*it)->testAsserted(suite, test, assertion);
}

void
CompoundListener::testException(const std::string &suite, const std::string &test)
{
    for (std::vector<boost::shared_ptr<TestListener> >::const_iterator it =
        m_listeners.begin();
        it != m_listeners.end();
        ++it)
        (*it)->testException(suite, test);
}

void
CompoundListener::testsComplete()
{
    for (std::vector<boost::shared_ptr<TestListener> >::const_iterator it =
        m_listeners.begin();
        it != m_listeners.end();
        ++it)
        (*it)->testsComplete();
}
