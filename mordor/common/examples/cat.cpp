// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include <iostream>

#include <boost/shared_ptr.hpp>

#include "mordor/common/config.h"
#include "mordor/common/fiber.h"
#include "mordor/common/scheduler.h"
#include "mordor/common/streams/file.h"
#include "mordor/common/streams/std.h"
#include "mordor/common/streams/transfer.h"

using namespace Mordor;

int main(int argc, const char *argv[])
{
    Config::loadFromEnvironment();
    StdoutStream stdoutStream;
    Fiber::ptr mainfiber(new Fiber());
    WorkerPool pool(2);
    try {
        if (argc == 1) {
            argc = 2;
            const char *hyphen[] = { "", "-" };
            argv = hyphen;
        }
        for (int i = 1; i < argc; ++i) {
            boost::shared_ptr<Stream> inStream;
            std::string arg(argv[i]);
            if (arg == "-") {
                inStream.reset(new StdinStream());
            } else {
                inStream.reset(new FileStream(arg, FileStream::READ));
            }
            transferStream(inStream, stdoutStream);
        }
    } catch (std::exception& ex) {
        std::cerr << "Caught " << typeid(ex).name( ) << ": "
                  << ex.what( ) << std::endl;
    }
    pool.stop();
    return 0;
}
