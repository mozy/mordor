#ifndef __MORDOR_PIPE_STREAM_H__
#define __MORDOR_PIPE_STREAM_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "stream.h"

namespace Mordor {

std::pair<Stream::ptr, Stream::ptr> pipeStream(size_t bufferSize = ~0);

}

#endif
