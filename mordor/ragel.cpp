// Copyright (c) 2009 - Mozy, Inc.

#include "ragel.h"

#include "assert.h"
#include "log.h"
#include "streams/buffer.h"
#include "streams/stream.h"
#include "string.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:ragel");

size_t
RagelParser::run(const void *buffer, size_t length)
{
    init();
    p = (const char *)buffer;
    pe = p + length;
    eof = pe;

    exec();

    MORDOR_ASSERT(!(final() && error()));
    return length - (pe - p);
}

size_t
RagelParser::run(const char *string)
{
    return run(string, strlen(string));
}

size_t
RagelParser::run(const std::string& string)
{
    return run(string.c_str(), string.size());
}

size_t
RagelParser::run(const Buffer& buffer)
{
    init();
    size_t total = 0;

    const std::vector<iovec> buffers = buffer.readBuffers();
    for (size_t i = 0; i < buffers.size(); ++i) {
        size_t consumed = run(buffers[i].iov_base, buffers[i].iov_len, false);
        total += consumed;
        if (consumed < buffers[i].iov_len) {
            MORDOR_ASSERT(final() || error());
            return total;
        }
        if (complete())
            break;
        if (error())
            return total;
    }
    run(NULL, 0, true);
    return total;
}

unsigned long long
RagelParser::run(Stream &stream)
{
    unsigned long long total = 0;
    init();
    Buffer buffer;
    bool inferredComplete = false;
    while (!error() && !inferredComplete) {
        // TODO: limit total amount read
        if (complete() || stream.read(buffer, 65536) == 0) {
            run(NULL, 0, true);
            break;
        } else {
            const std::vector<iovec> buffers = buffer.readBuffers();
            for (size_t i = 0; i < buffers.size(); ++i) {
                size_t consumed = run(buffers[i].iov_base, buffers[i].iov_len, false);
                total += consumed;
                buffer.consume(consumed);
                if (consumed < buffers[i].iov_len) {
                    MORDOR_ASSERT(final() || error());
                    inferredComplete = true;
                    break;
                }
                if (error() || complete())
                    break;
            }
        }
    }
    if (stream.supportsUnread())
        stream.unread(buffer, buffer.readAvailable());
    return total;
}

void
RagelParser::init()
{
    mark = NULL;
    m_fullString.clear();
}

size_t
RagelParser::run(const void *buffer, size_t length, bool isEof)
{
    MORDOR_ASSERT(!error());

    // Remember and reset marks in case fullString gets moved
    if (earliestPointer()) {
        const char *oldString = m_fullString.c_str();
        m_fullString.append((const char *)buffer, length);
        if (m_fullString.c_str() != oldString)
            adjustPointers(m_fullString.c_str() - oldString);
        p = m_fullString.c_str();
        pe = p + m_fullString.length();
        p = pe - length;
    } else {
        p = (const char *)buffer;
        pe = p + length;
    }

    eof = isEof ? pe : NULL;

    MORDOR_LOG_DEBUG(g_log) << charslice(p, pe - p);
    exec();

    const char *earliest = earliestPointer();
    MORDOR_ASSERT(earliest <= pe);
    if (!earliest) {
        m_fullString.clear();
    } else {
        if (m_fullString.empty()) {
            MORDOR_ASSERT(earliest >= buffer);
            m_fullString.append(earliest, pe - earliest);
            adjustPointers(m_fullString.c_str() - earliest);
        } else if (earliest == m_fullString.c_str()) {
        } else {
            MORDOR_ASSERT(earliest > m_fullString.c_str());
            m_fullString = m_fullString.substr(earliest - m_fullString.c_str());
            adjustPointers(m_fullString.c_str() - earliest);
        }
    }

    return p - (pe - length);
}

const char *
RagelParser::earliestPointer() const
{
    return mark;
}

void
RagelParser::adjustPointers(ptrdiff_t offset)
{
    if (mark)
        mark += offset;
}

void
RagelParserWithStack::prepush()
{
    if (stack.empty())
        stack.resize(1);
    if (top >= stack.size())
        stack.resize(stack.size() * 2);
}

void
RagelParserWithStack::postpop()
{
    if (top <= stack.size() / 4)
        stack.resize(stack.size() / 2);
}

}
