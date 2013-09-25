// Copyright (c) 2013 - Cody Cutrer

#include "session.h"

#include <libssh2.h>

#include "agent.h"
#include "channel.h"
#include "mordor/exception.h"
#include "mordor/iomanager.h"
#include "mordor/log.h"
#include "mordor/socket.h"

namespace {

static struct Initializer {
    Initializer()
    {
        if (libssh2_init(0)) {
            MORDOR_THROW_EXCEPTION(std::runtime_error("unable to initialize libssh2"));
        }
    }
    ~Initializer()
    {
        libssh2_exit();
    }
} g_init;

}

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:ssh:session");

namespace SSH {

Session::Session(Socket::ptr socket, IOManager *ioManager)
    : m_ioManager(ioManager),
      m_socket(socket)
{
    m_session = libssh2_session_init();
    if (!m_session)
        MORDOR_THROW_EXCEPTION(std::runtime_error("failed"));
    if (m_ioManager)
        libssh2_session_set_blocking(m_session, 0);
}

Session::~Session()
{
    libssh2_session_free(m_session);
}

void
Session::handshake()
{
    int rc;
    while ((rc = libssh2_session_handshake(m_session, m_socket->socket())) ==
        LIBSSH2_ERROR_EAGAIN) {
        wait();
    }
    if (rc) {
        MORDOR_THROW_EXCEPTION(std::runtime_error("couldn't handshake"));
    }
    MORDOR_LOG_VERBOSE(g_log) << this << " SSH handshake with " << *m_socket->remoteAddress();
}

bool
Session::authenticate(const std::string &username, Agent &agent)
{
    struct libssh2_agent_publickey *identity = NULL;
    while (true) {
        int rc = libssh2_agent_get_identity(agent.m_agent, &identity, identity);
        if (rc == 1)
            return false;
        if (rc < 0) {
            MORDOR_THROW_EXCEPTION(std::runtime_error("couldn't get identity"));
        }
        FiberMutex::ScopedLock lock(agent.m_mutex);
        // Cheat, so we can share agents across sessions
        *(LIBSSH2_SESSION **)agent.m_agent = m_session;
        while ((rc = libssh2_agent_userauth(agent.m_agent, username.c_str(),
            identity)) == LIBSSH2_ERROR_EAGAIN) {
            // TODO: is it the agent that blocked, or the session?
            // agent.wait();
            wait();
        }
        if (rc == 0) {
            MORDOR_LOG_VERBOSE(g_log) << this << " authenticated as " <<
                username << " via SSH agent";
            return true;
        }
    }
}

Channel::ptr
Session::sendFile(const std::string &filename, unsigned long long size,
    int mode, time_t mtime, time_t atime)
{
    LIBSSH2_CHANNEL *channel;

     while ((channel = libssh2_scp_send64(m_session, filename.c_str(),
        mode & 0777, size, mtime, atime)) == NULL &&
        libssh2_session_last_errno(m_session) == LIBSSH2_ERROR_EAGAIN) {
        wait();
    }
    if (!channel) {
        MORDOR_THROW_EXCEPTION(std::runtime_error("couldn't start file transfer"));
    }
    Channel::ptr stream(new Channel(this, channel));
    MORDOR_LOG_VERBOSE(g_log) << this << " " << &*stream << " sending " << filename <<
        " (" << size << "b)";
    return stream;
}

void
Session::wait()
{
    int dir = libssh2_session_block_directions(m_session);
    MORDOR_LOG_DEBUG(g_log) << this << " waiting for " << dir;
    if (dir & LIBSSH2_SESSION_BLOCK_OUTBOUND) {
        m_ioManager->registerEvent(m_socket->socket(), IOManager::WRITE);
        Scheduler::yieldTo();
    } else if (dir & LIBSSH2_SESSION_BLOCK_INBOUND) {
        m_ioManager->registerEvent(m_socket->socket(), IOManager::READ);
        Scheduler::yieldTo();
    }
}

}}

