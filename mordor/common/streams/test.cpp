// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "test.h"

void
TestStream::close(CloseType type)
{
    if (m_onClose)
        m_onClose(type);
    if (ownsParent())
        parent()->close(type);
}

size_t
TestStream::read(Buffer &b, size_t len)
{
    len = std::min(len, m_maxReadSize);
    if (m_onRead) {
        if (m_onReadBytes == 0) {
            m_onRead();
        }
    }
    if (m_onRead && m_onReadBytes > 0)
        len = (size_t)std::min<long long>(len, m_onReadBytes);
    size_t result = parent()->read(b, len);
    if (m_onRead && m_onReadBytes > 0)
        m_onReadBytes -= result;
    return result;
}

size_t
TestStream::write(const Buffer &b, size_t len)
{
    len = std::min(len, m_maxWriteSize);
    if (m_onWrite) {
        if (m_onWriteBytes == 0) {
            m_onWrite();
        }
    }
    if (m_onWrite && m_onWriteBytes > 0)
        len = (size_t)std::min<long long>(len, m_onWriteBytes);
    size_t result = parent()->write(b, len);
    if (m_onWrite && m_onWriteBytes > 0)
        m_onWriteBytes -= result;
    return result;
}

void
TestStream::flush()
{
    if (m_onFlush)
        m_onFlush();
    parent()->flush();
}
