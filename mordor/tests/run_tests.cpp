// Copyright (c) 2009 - Mozy, Inc.

#include <iostream>

#include "mordor/config.h"
#include "mordor/main.h"
#include "mordor/version.h"
#include "mordor/statistics.h"
#include "mordor/test/antxmllistener.h"
#include "mordor/test/compoundlistener.h"
#include "mordor/test/stdoutlistener.h"

using namespace Mordor;
using namespace Mordor::Test;

static ConfigVar<std::string>::ptr g_xmlDirectory = Config::lookup<std::string>(
    "test.antxml.directory", std::string(), "Location to put XML files");

MORDOR_MAIN(int argc, char *argv[])
{
    try {
        Config::loadFromCommandLine(argc, argv);
    } catch (std::invalid_argument &ex) {
        ConfigVarBase::ptr configVar = Config::lookup(ex.what());
        MORDOR_ASSERT(configVar);
        std::cerr << "Invalid value for " << typeid(*configVar).name()
            << ' ' << configVar->name() << ": " << configVar->description()
            << std::endl;
        return 1;
    }
    Config::loadFromEnvironment();

    CompoundListener listener;
    std::string xmlDirectory = g_xmlDirectory->val();
    listener.addListener(boost::shared_ptr<TestListener>(
        new StdoutListener()));
    if (!xmlDirectory.empty()) {
        if (xmlDirectory == ".")
            xmlDirectory.clear();
        listener.addListener(boost::shared_ptr<TestListener>(
            new AntXMLListener(xmlDirectory)));
    }
    bool result;
    if (argc > 1) {
        result = runTests(testsForArguments(argc - 1, argv + 1), listener);
    } else {
        result = runTests(listener);
    }
    std::cout << Statistics::dump();
    return result ? 0 : 1;
}
