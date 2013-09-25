// Copyright (c) 2013 - Cody Cutrer

#include "tee.h"

#include "mordor/assert.h"
#include "mordor/log.h"
#include "mordor/parallel.h"
#include "mordor/streams/buffer.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:streams:tee");

TeeStream::TeeStream(std::vector<Stream::ptr> outputs, int parallelism, bool own)
    : m_outputs(outputs),
      m_parallelism(parallelism),
      m_own(own)
{
    if (parallelism == -2)
        m_parallelism = outputs.size();
#ifndef NDEBUG
    for (std::vector<Stream::ptr>::const_iterator it(outputs.begin());
         it != outputs.end();
         ++it) {
        MORDOR_ASSERT((*it)->supportsWrite());
    }
#endif
}

void
TeeStream::close(CloseType type)
{
    if (ownsOutputs() && (type & Stream::WRITE)) {
        parallel_foreach(m_outputs.begin(), m_outputs.end(),
            boost::bind(&Stream::close, _1, type), m_parallelism);
    }
}

size_t
TeeStream::write(const Buffer &buffer, size_t length)
{
    parallel_foreach(m_outputs.begin(), m_outputs.end(),
        boost::bind(&TeeStream::doWrites, _1, buffer, length), m_parallelism);
    return length;
}

void
TeeStream::flush(bool flushOutputs)
{
    if (!flushOutputs)
        return;
    parallel_foreach(m_outputs.begin(), m_outputs.end(),
        boost::bind(&Stream::flush, _1, true), m_parallelism);
}

void
TeeStream::doWrites(Stream::ptr output, const Buffer &buffer, size_t length)
{
    Buffer copy;
    copy.copyIn(buffer, length);
    while (copy.readAvailable()) {
        copy.consume(output->write(copy, copy.readAvailable()));
    }
}

}
