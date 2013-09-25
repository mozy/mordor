#ifndef __MORDOR_SSH_SESSION_H__
#define __MORDOR_SSH_SESSION_H__
// Copyright (c) 2013 - Cody Cutrer

#include <string>
#include <boost/shared_ptr.hpp>
#include <libssh2.h>

namespace Mordor {
class IOManager;
class Socket;

namespace SSH {

class Agent;
class Channel;

class Session
{
public:
    Session(boost::shared_ptr<Socket> socket, IOManager *ioManager = NULL);
    ~Session();

    void handshake();
    bool authenticate(const std::string &username, Agent &agent);

    boost::shared_ptr<Channel> sendFile(const std::string &filename,
        unsigned long long size, int mode = 0644, time_t mtime = 0,
        time_t atime = 0);

public: // internal
    void wait();

private:
    LIBSSH2_SESSION *m_session;
    IOManager *m_ioManager;
    boost::shared_ptr<Socket> m_socket;
};

}}

#endif

