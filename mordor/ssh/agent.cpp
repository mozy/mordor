// Copyright (c) 2013 - Cody Cutrer

#include "agent.h"

#include <libssh2.h>

#include "mordor/exception.h"

namespace Mordor {
namespace SSH {

Agent::Agent(IOManager *ioManager)
    : m_ioManager(ioManager)
{
    m_session = libssh2_session_init();
    if (!m_session) {
        MORDOR_THROW_EXCEPTION(std::runtime_error("Unable to create dummy session for ssh agent"));
    }
    m_agent = libssh2_agent_init(m_session);
    if (!m_agent) {
        libssh2_session_free(m_session);
        MORDOR_THROW_EXCEPTION(std::runtime_error("Unable to create ssh agent"));
    }
}

Agent::~Agent()
{
    disconnect();
    *(LIBSSH2_SESSION **)m_agent = m_session;
    libssh2_agent_free(m_agent);
    libssh2_session_free(m_session);
}

void
Agent::connect()
{
    // TODO: nonblocking
    if (libssh2_agent_connect(m_agent)) {
        MORDOR_THROW_EXCEPTION(std::runtime_error("Unable to connect to agent"));
    }
    if (libssh2_agent_list_identities(m_agent)) {
        MORDOR_THROW_EXCEPTION(std::runtime_error("Unable to list agent identities"));
    }
}

void
Agent::disconnect()
{
    *(LIBSSH2_SESSION **)m_agent = m_session;
    libssh2_agent_disconnect(m_agent);
}

}}
