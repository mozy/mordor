// Copyright (c) 2009 - Decho Corp.

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
    WorkerPool mainpool, pool(2, false);
    pool.switchTo();
    const char* hyphen = "-";
    if (argc == 1) {
        argc = 2;
        argv = &hyphen;
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
    mainpool.switchTo();
    pool.stop();
}
