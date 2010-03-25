// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "ragel.h"

#include "assert.h"
#include "log.h"
#include "mordor/string.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:ragel");

size_t
RagelParser::run(const void *buf, size_t len)
{
    init();
    p = (const char *)buf;
    pe = p + len;
    eof = pe;

    exec();

    MORDOR_ASSERT(!(final() && error()));
    return len - (pe - p);
}

size_t
RagelParser::run(const char *str)
{
    return run(str, strlen(str));
}

size_t
RagelParser::run(const std::string& str)
{
    return run(str.c_str(), str.length());
}

size_t
RagelParser::run(const Buffer& b)
{
    init();
    size_t total = 0;

    const std::vector<iovec> bufs = b.readBuffers();
    for (size_t i = 0; i < bufs.size(); ++i) {
        size_t consumed = run(bufs[i].iov_base, bufs[i].iov_len, false);
        total += consumed;
        if (consumed < bufs[i].iov_len) {
            MORDOR_ASSERT(final() || error());
            return total;
        }
        if (error() || complete())
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
    Buffer b;
    bool inferredComplete = false;
    while (!error() && !complete() && !inferredComplete) {
        // TODO: limit total amount read
        size_t read = stream.read(b, 65536);
        if (read == 0) {
            run(NULL, 0, true);
            break;
        } else {
            const std::vector<iovec> bufs = b.readBuffers();
            for (size_t i = 0; i < bufs.size(); ++i) {
                size_t consumed = run(bufs[i].iov_base, bufs[i].iov_len, false);
                total += consumed;
                b.consume(consumed);
                if (consumed < bufs[i].iov_len) {
                    MORDOR_ASSERT(final() || error());
                    inferredComplete = true;
                    break;
                }
                if (error() || complete())
                    break;
            }
        }
    }
    if (stream.supportsUnread()) {
        stream.unread(b, b.readAvailable());
    }
    return total;
}

void
RagelParser::init()
{
    mark = NULL;
    m_fullString.clear();
}

size_t
RagelParser::run(const void *buf, size_t len, bool isEof)
{
    MORDOR_ASSERT(!error());

    // Remember and reset marks in case fullString gets moved
    if (earliestPointer()) {
        const char *oldString = m_fullString.c_str();
        m_fullString.append((const char *)buf, len);
        if (m_fullString.c_str() != oldString)
            adjustPointers(m_fullString.c_str() - oldString);
        p = m_fullString.c_str();
        pe = p + m_fullString.length();
        p = pe - len;
    } else {
        p = (const char *)buf;
        pe = p + len;
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
            MORDOR_ASSERT(earliest >= buf);
            m_fullString.append(earliest, pe - earliest);
            adjustPointers(m_fullString.c_str() - earliest);
        } else if (earliest == m_fullString.c_str()) {
        } else {
            MORDOR_ASSERT(earliest > m_fullString.c_str());
            const char *oldString = m_fullString.c_str();
            m_fullString = m_fullString.substr(earliest - m_fullString.c_str());
            adjustPointers(m_fullString.c_str() - earliest);
        }
    }

    return p - (pe - len);
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
