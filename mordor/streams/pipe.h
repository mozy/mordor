#ifndef __MORDOR_PIPE_STREAM_H__
#define __MORDOR_PIPE_STREAM_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <utility>

#include <boost/shared_ptr.hpp>

namespace Mordor {

class Stream;

std::pair<boost::shared_ptr<Stream>, boost::shared_ptr<Stream> >
    pipeStream(size_t bufferSize = ~0);

}

#endif
