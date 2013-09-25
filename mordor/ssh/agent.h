#ifndef __MORDOR_SSH_AGENT_H__
#define __MORDOR_SSH_AGENT_H__
// Copyright (c) 2013 - Cody Cutrer

#include <libssh2.h>

#include "mordor/fibersynchronization.h"

namespace Mordor {

class IOManager;

namespace SSH {

class Agent
{
public:
    Agent(IOManager *ioManager = NULL);
    ~Agent();

    void connect();
    void disconnect();

public: // internal
    void wait();

public: // internal
    LIBSSH2_AGENT *m_agent;
    LIBSSH2_SESSION *m_session;
    FiberMutex m_mutex;

private:
    IOManager *m_ioManager;
};

}}

#endif

