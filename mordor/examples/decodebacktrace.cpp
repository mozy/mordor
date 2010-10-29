// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/predef.h"

#include <iostream>

#include <DbgHelp.h>

#include "mordor/config.h"
#include "mordor/main.h"
#include "mordor/streams/buffered.h"
#include "mordor/streams/std.h"

using namespace Mordor;

MORDOR_MAIN(int argc, char *argv[])
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <symbolpath> <binary>..." << std::endl
            << "    Look for backtrace addresses on stdin, and attempt to convert them to" << std::endl
            << "    symbols of the given binaries." << std::endl << std::endl;
        return 1;
    }

    std::vector<DWORD64> loadedModules;
    try {
        Config::loadFromEnvironment();
        // Re-init symbols (it got initted statically in exception.cpp) with
        // a symbol search path
        if (!SymCleanup(GetCurrentProcess()))
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("SymCleanup");
        if (!SymInitialize(GetCurrentProcess(), argv[1], FALSE))
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("SymInitialize");

        // Load up the specified modules in their default load location
        // Does *not* currently handle ASLR (basically, it would need to
        // do two passes through the log, and calculate the actual load address
        // of a module based on where the (limited) symbols that are in the
        // file ended up; or, use a minidump to aid the process)
        for (int i = 2; i < argc; ++i) {
            DWORD64 baseAddress =
                SymLoadModule64(GetCurrentProcess(), NULL, argv[i], NULL, 0, 0);
            if (baseAddress == 0) {
                if (GetLastError() == ERROR_SUCCESS)
                    continue;
                MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("SymLoadModule64");
            }
            loadedModules.push_back(baseAddress);
        }
        char buf[sizeof(SYMBOL_INFO) + MAX_SYM_NAME - 1];
        SYMBOL_INFO *symbol = (SYMBOL_INFO*)buf;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement64 = 0;

        Stream::ptr stream(new StdinStream());
        stream.reset(new BufferedStream(stream));
        StdoutStream output;
        std::string line;
        do {
            line = stream->getDelimited('\n', true);
            std::string copy(line);
            if (copy.size() >= 35 &&
                strncmp(copy.c_str(), "[struct Mordor::tag_backtrace *] = ", 35)
                == 0)
                copy = copy.substr(35);
            char *end;
            DWORD64 address = strtoull(copy.c_str(), &end, 16);
            if ( (end - copy.c_str()) == 8 || (end - copy.c_str()) == 16) {
                if (SymFromAddr(GetCurrentProcess(), address, &displacement64, symbol)) {
                    std::ostringstream os;
                    if (copy != line)
                        os << "[struct Mordor::tag_backtrace *] = ";
                    os << copy.substr(0, (end - copy.c_str())) << ": "
                        << symbol->Name << "+" << displacement64;
                    IMAGEHLP_LINE64 line;
                    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
                    DWORD displacement = 0;
                    if (SymGetLineFromAddr64(GetCurrentProcess(),
                        address, &displacement, &line)) {
                        std::cout << ": " << line.FileName << "("
                            << line.LineNumber << ")+" << displacement;
                    }
                    os << std::endl;
                    std::string newline = os.str();
                    output.write(newline.c_str(), newline.size());
                } else {
                    output.write(line.c_str(), line.size());
                }
            } else {
                output.write(line.c_str(), line.size());
            }
        } while (!line.empty() && line.back() == '\n');
    } catch (...) {
        std::cerr << boost::current_exception_diagnostic_information() << std::endl;
        for (size_t i = 0; i < loadedModules.size(); ++i)
            SymUnloadModule64(GetCurrentProcess(), loadedModules[i]);
        return 2;
    }
    for (size_t i = 0; i < loadedModules.size(); ++i)
        SymUnloadModule64(GetCurrentProcess(), loadedModules[i]);
    return 0;
}
