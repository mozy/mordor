// Copyright (c) 2009 - Decho Corp.

#include <iostream>

#include <boost/shared_ptr.hpp>

#include "common/fiber.h"
#include "common/scheduler.h"
#include "common/streams/file.h"
#include "common/streams/std.h"
#include "common/streams/transfer.h"

void main(int argc, const char *argv[])
{
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
            transferStream(inStream.get(), &stdoutStream);
        }
    } catch (std::exception& ex) {
        std::cerr << "Caught " << typeid(ex).name( ) << ": "
                  << ex.what( ) << std::endl;
    }
    pool.stop();
}
