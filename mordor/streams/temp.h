#ifndef __MORDOR_TEMP_STREAM_H__
#define __MORDOR_TEMP_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "file.h"

namespace Mordor {

#ifdef WINDOWS
typedef FileStream TempStreamBase;
#else
typedef FDStream TempStreamBase;
#endif

class TempStream : public TempStreamBase
{
public:
    TempStream(const std::string &prefix = "", IOManager *ioManager = NULL,
        Scheduler *scheduler = NULL);
};

}

#endif
