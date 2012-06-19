#ifndef __MORDOR_PIPE_STREAM_H__
#define __MORDOR_PIPE_STREAM_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <utility>

#include <boost/shared_ptr.hpp>

#ifdef WINDOWS
#include "handle.h"
#else
#include "fd.h"
#endif

namespace Mordor {

class IOManager;

/// Create a user-space only, full-duplex anonymous pipe
std::pair<Stream::ptr, Stream::ptr> pipeStream(size_t bufferSize = ~0);

/// Create a kernel-level, half-duplex anonymous pipe
///
/// The Streams created by this function will have a file handle, and are
/// suitable for usage with native OS APIs
std::pair<NativeStream::ptr, NativeStream::ptr>
    anonymousPipe(IOManager *ioManager = NULL);

}

#endif
