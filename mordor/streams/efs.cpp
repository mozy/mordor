// Copyright (c) 2009 - Mozy, Inc.

#include <boost/bind.hpp>

#include "efs.h"

#include "buffer.h"
#include "mordor/assert.h"
#include "mordor/fiber.h"
#include "mordor/string.h"

namespace Mordor {

EFSStream::EFSStream(void *context, bool read, bool ownContext)
        : m_context(context),
          m_read(read),
          m_own(ownContext),
          m_readBuffer(NULL),
          m_todo(0),
          m_pos(0),
          m_seekTarget(0)
{
    init();
}

EFSStream::EFSStream(const char *filename, bool read)
        : m_context(NULL),
          m_read(read),
          m_own(true),
          m_readBuffer(NULL),
          m_todo(0),
          m_pos(0),
          m_seekTarget(0)
{
    DWORD dwRet = OpenEncryptedFileRawW(toUtf16(filename).c_str(),
        read ? 0 : CREATE_FOR_IMPORT, &m_context);
    if (dwRet != ERROR_SUCCESS)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(dwRet, "OpenEncryptedFileRawW");
    init();
}

EFSStream::EFSStream(const wchar_t *filename, bool read)
        : m_context(NULL),
          m_read(read),
          m_own(true),
          m_readBuffer(NULL),
          m_todo(0),
          m_pos(0),
          m_seekTarget(0)
{
    DWORD dwRet = OpenEncryptedFileRawW(filename,
        read ? 0 : CREATE_FOR_IMPORT, &m_context);
    if (dwRet != ERROR_SUCCESS)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(dwRet, "OpenEncryptedFileRawW");
    init();
}

EFSStream::EFSStream(const std::string &filename, bool read)
        : m_context(NULL),
          m_read(read),
          m_own(true),
          m_readBuffer(NULL),
          m_todo(0),
          m_pos(0),
          m_seekTarget(0)
{
    DWORD dwRet = OpenEncryptedFileRawW(toUtf16(filename).c_str(),
        read ? 0 : CREATE_FOR_IMPORT, &m_context);
    if (dwRet != ERROR_SUCCESS)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(dwRet, "OpenEncryptedFileRawW");
    init();
}

EFSStream::EFSStream(const std::wstring &filename, bool read)
        : m_context(NULL),
          m_read(read),
          m_own(true),
          m_readBuffer(NULL),
          m_todo(0),
          m_pos(0),
          m_seekTarget(0)
{
    DWORD dwRet = OpenEncryptedFileRawW(filename.c_str(),
        read ? 0 : CREATE_FOR_IMPORT, &m_context);
    if (dwRet != ERROR_SUCCESS)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(dwRet, "OpenEncryptedFileRawW");
    init();
}

void
EFSStream::init()
{
    MORDOR_ASSERT(m_context);
    if (m_read)
        m_fiber = Fiber::ptr(new Fiber(boost::bind(&EFSStream::readFiber,
            this)));
    else
        m_fiber = Fiber::ptr(new Fiber(boost::bind(&EFSStream::writeFiber,
            this)));
}

EFSStream::~EFSStream()
{
    if (m_fiber && m_fiber->state() == Fiber::HOLD) {
        m_pos = -1;
        m_fiber->call();
    }
    if (m_context && m_own) {
        CloseEncryptedFileRaw(m_context);
        m_context = NULL;
    }
}

void
EFSStream::close(Stream::CloseType type)
{
    MORDOR_ASSERT(type == BOTH);
    if (!m_read && m_fiber && m_fiber->state() == Fiber::HOLD) {
        m_todo = 0;
        m_fiber->call();
    }
    if (m_fiber && m_fiber->state() == Fiber::HOLD) {
        m_pos = -1;
        m_fiber->call();
    }
    if (m_context && m_own) {
        CloseEncryptedFileRaw(m_context);
        m_context = NULL;
    }
}

size_t
EFSStream::read(Buffer &b, size_t len)
{
    MORDOR_ASSERT(m_read);
    if (m_fiber->state() == Fiber::TERM)
        return 0;
    b.reserve(len);
    m_readBuffer = &b;
    m_todo = len;
    m_fiber->call();
    m_readBuffer = NULL;
    m_pos += len - m_todo;
    return len - m_todo;
}

size_t
EFSStream::write(const Buffer &b, size_t len)
{
    MORDOR_ASSERT(!m_read);
    MORDOR_ASSERT(m_fiber->state() != Fiber::TERM);
    if (len == 0)
        return 0;
    // Deconstifying, but we really do treat it as const
    m_writeBuffer = &b;
    m_todo = len;
    m_fiber->call();
    m_writeBuffer = NULL;
    m_pos += len - m_todo;
    return len - m_todo;
}

long long
EFSStream::seek(long long offset, Anchor anchor)
{
    if (anchor == END)
        MORDOR_THROW_EXCEPTION(std::invalid_argument("anchor == END is not supported"));
    if (anchor == CURRENT) {
        offset = m_pos + offset;
        anchor = BEGIN;
    }
    MORDOR_ASSERT(anchor == BEGIN);
    if (offset < 0)
        MORDOR_THROW_EXCEPTION(std::invalid_argument("negative offset"));
    m_seekTarget = offset;
    if (m_seekTarget < m_pos) {
        if(m_fiber->state() != Fiber::TERM) {
            m_pos = -2;
            m_fiber->call();
            MORDOR_ASSERT(m_fiber->state() == Fiber::TERM);
        }
        m_fiber->reset();
        m_pos = 0;
        m_todo = 0;
    } else if (m_seekTarget == m_pos) {
        return m_pos;
    }
    m_fiber->call();
    return m_pos;
}

void
EFSStream::readFiber()
{
    MORDOR_ASSERT(m_read);
    DWORD dwRet = ReadEncryptedFileRaw(&EFSStream::ExportCallback, this, m_context);
    if (dwRet != ERROR_SUCCESS && dwRet != ERROR_CANCELLED)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(dwRet, "ReadEncryptedFileRaw");
}

DWORD WINAPI
EFSStream::ExportCallback(PBYTE pbData, PVOID pvCallbackContext, ULONG ulLength)
{
    EFSStream *self = (EFSStream *)pvCallbackContext;
    while (ulLength > 0) {
        if (self->m_pos == -1) {
            return ERROR_CANCELLED;
        } else if (self->m_pos == -2) {
            // it's dumb ... but if we seek'ed backward in the file,
            // so we need to start over ... ideally we'd say ERROR_CANCELED
            // here so we don't get any more callbacks. however, if we return
            // a failure code here, the EFS context will become invalid
            // and we will not be able to do ReadEncryptedFileRaw() again.
            // So we have to let Windows feed us the rest of the file even
            // though we don't want it.
            return ERROR_SUCCESS;
        } else if (self->m_todo == 0) {
            MORDOR_ASSERT(self->m_seekTarget >= self->m_pos);
            ULONG toAdvance =
                (ULONG)std::min<long long>(self->m_seekTarget - self->m_pos, ulLength);
            if (toAdvance == 0) {
                Fiber::yield();
            } else {
                ulLength -= toAdvance;
                pbData += toAdvance;
                self->m_pos += toAdvance;
            }
        } else {
            MORDOR_ASSERT(self->m_readBuffer);
            size_t toCopy = std::min<size_t>(self->m_todo, ulLength);
            self->m_readBuffer->copyIn(pbData, toCopy);
            ulLength -= (ULONG)toCopy;
            pbData += toCopy;
            self->m_todo -= toCopy;
            Fiber::yield();
        }
    }
    return ERROR_SUCCESS;
}

void
EFSStream::writeFiber()
{
    MORDOR_ASSERT(!m_read);
    DWORD dwRet = WriteEncryptedFileRaw(&EFSStream::ImportCallback, this, m_context);
    if (dwRet != ERROR_SUCCESS && dwRet != ERROR_CANCELLED)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(dwRet, "WriteEncryptedFileRaw");
}

DWORD WINAPI
EFSStream::ImportCallback(PBYTE pbData, PVOID pvCallbackContext, PULONG ulLength)
{
    EFSStream *self = (EFSStream *)pvCallbackContext;
    if (self->m_pos == -1) {
        return ERROR_CANCELLED;
    } else if (self->m_todo == 0) {
        *ulLength = 0;
        return ERROR_SUCCESS;
    } else {
        ULONG toCopy = (ULONG)std::min<size_t>(self->m_todo, *ulLength);
        if (toCopy > 0) {
            MORDOR_ASSERT(self->m_writeBuffer);
            self->m_writeBuffer->copyOut(pbData, toCopy);
        }
        *ulLength = toCopy;
        self->m_todo -= toCopy;
        Fiber::yield();
        return ERROR_SUCCESS;
    }
}

}
