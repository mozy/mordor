#ifndef __PIPE_STREAM_H__
#define __PIPE_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "stream.h"

std::pair<Stream::ptr, Stream::ptr> pipeStream(size_t bufferSize = ~0);

#endif
