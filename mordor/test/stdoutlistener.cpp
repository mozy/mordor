// Copyright (c) 2009 - Decho Corp.

#include "mordor/predef.h"

#include "stdoutlistener.h"

#include <iostream>

#include <boost/exception.hpp>

using namespace Mordor;
using namespace Mordor::Test;

StdoutListener::StdoutListener()
: m_tests(0),
  m_success(0)
{}

void
StdoutListener::testStarted(const std::string &suite, const std::string &test)
{
    if (test != "<invariant>")
        ++m_tests;
    std::cout << "Running " << suite << "::" << test << ": ";
    std::cout.flush();
}

void
StdoutListener::testComplete(const std::string &suite, const std::string &test)
{
    if (test != "<invariant>")
        ++m_success;
    std::cout << "OK" << std::endl;
}

void
StdoutListener::testAsserted(const std::string &suite, const std::string &test,
                             const Assertion &assertion)
{
    std::cerr << "Assertion: "
        << boost::current_exception_diagnostic_information() << std::endl;
}

void
StdoutListener::testException(const std::string &suite, const std::string &test)
{
    std::cerr << "Unexpected exception: "
        << boost::current_exception_diagnostic_information() << std::endl;
}

void
StdoutListener::testsComplete()
{
    std::cout << "Tests complete.  " << m_success << "/" << m_tests
        << " passed." << std::endl;
}
