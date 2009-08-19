// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "stdoutlistener.h"

#include <iostream>

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
                             const std::string &message)
{
    std::cerr << message;
}

void
StdoutListener::testException(const std::string &suite, const std::string &test,
                              const std::exception &ex)
{
    std::cerr << "Unexpected exception " << typeid(ex).name() << ":" << std::endl
                << "" << ex.what() << std::endl;
}

void
StdoutListener::testUnknownException(const std::string &suite,
                                        const std::string &test)
{
    std::cerr << "Unexpected unknown exception" << std::endl;
}

void
StdoutListener::testsComplete()
{
    std::cout << "Tests complete.  " << m_success << "/" << m_tests
        << " passed." << std::endl;
}
