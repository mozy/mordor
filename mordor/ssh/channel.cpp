// Copyright (c) 2013 - Cody Cutrer

#include "channel.h"

#include "mordor/exception.h"
#include "mordor/log.h"
#include "session.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:ssh:channel");

namespace SSH {

Channel::Channel(Session *session, LIBSSH2_CHANNEL *channel)
    : m_session(session),
      m_channel(channel)
{}

Channel::~Channel()
{
    libssh2_channel_free(m_channel);
}

size_t
Channel::write(const void *buffer, size_t length)
{
    int rc;
    while ((rc = libssh2_channel_write(m_channel, (const char *)buffer, length)) ==
        LIBSSH2_ERROR_EAGAIN) {
        m_session->wait();
    }
    if (rc < 0) {
        MORDOR_THROW_EXCEPTION(std::runtime_error("write failed"));
    }
    return (size_t)rc;
}

void
Channel::close(CloseType type)
{
    MORDOR_LOG_VERBOSE(g_log) << this << " sending EOF";
    int rc;
    while ((rc = libssh2_channel_send_eof(m_channel)) ==
        LIBSSH2_ERROR_EAGAIN) {
        m_session->wait();
    }
    if (rc) {
        MORDOR_THROW_EXCEPTION(std::runtime_error("unable to send eof"));
    }
    MORDOR_LOG_VERBOSE(g_log) << this << " waiting for EOF";
    while ((rc = libssh2_channel_wait_eof(m_channel)) ==
        LIBSSH2_ERROR_EAGAIN) {
        m_session->wait();
    }
    if (rc) {
        MORDOR_THROW_EXCEPTION(std::runtime_error("unable to wait for eof"));
    }
    MORDOR_LOG_VERBOSE(g_log) << this << " waiting for close";
    while ((rc = libssh2_channel_wait_closed(m_channel)) ==
        LIBSSH2_ERROR_EAGAIN) {
        m_session->wait();
    }
    if (rc) {
        MORDOR_THROW_EXCEPTION(std::runtime_error("unable to wait for channel close"));
    }
}

}}

