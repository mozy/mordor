// Copyright (c) 2009 - Decho Corp.

#include "ragel.h"

#include <cassert>

#include "common/streams/buffered.h"
#include "common/streams/stream.h"

void
RagelParser::run(const std::string& str)
{
    init();
    p = str.c_str();
    pe = p + str.length();
    eof = pe;

    exec();

    if (error()) {
        return;
    }
    if (p == pe) {
        return;
    } else {
        init();
    }
}

void
RagelParser::run(Stream &stream)
{
    init();
    Buffer b;
    while (!complete() && !error()) {
        // TODO: limit total amount read
        size_t read = stream.read(b, 65536);
        if (read == 0) {
            run(NULL, 0, true);
        } else {
            const std::vector<iovec> bufs = b.readBufs();
            for (size_t i = 0; i < bufs.size(); ++i) {
                size_t consumed = run((const char*)bufs[i].iov_base, bufs[i].iov_len, false);
                b.consume(consumed);
                if (error() || complete())
                    break;
            }
        }
    }
    BufferedStream *buffered = dynamic_cast<BufferedStream *>(&stream);
    if (buffered) {
        buffered->unread(b, b.readAvailable());
    }
}

void
RagelParser::init()
{
    mark = NULL;
    m_fullString.clear();
}

size_t
RagelParser::run(const char *buf, size_t len, bool isEof)
{
    assert(!complete());
    assert(!error());

    size_t markSpot = ~0;

    // Remember and reset mark in case fullString gets moved
    if (mark) {
        markSpot = mark - m_fullString.c_str();
    }

    m_fullString.append(buf, len);

    if (markSpot != ~0u) {
        mark = m_fullString.c_str() + markSpot;
    }

    p = m_fullString.c_str();
    pe = p + m_fullString.length();
    p = pe - len;
    if (isEof) {
        eof = pe;
    } else {
        eof = NULL;
    }

    exec();

    if (!mark) {
        m_fullString.clear();
    } else {
        markSpot = mark - m_fullString.c_str();
        m_fullString = m_fullString.substr(markSpot);
        mark = m_fullString.c_str();
    }

    return p - (pe - len);
}
