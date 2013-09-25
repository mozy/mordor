#ifndef __MORDOR_SSH_CHANNEL_H__
#define __MORDOR_SSH_CHANNEL_H__
// Copyright (c) 2013 - Cody Cutrer

#include <boost/shared_ptr.hpp>
#include <libssh2.h>

#include "mordor/streams/stream.h"

namespace Mordor {
namespace SSH {

class Session;

class Channel : public Stream
{
public:
    typedef boost::shared_ptr<Channel> ptr;

public: // internal
    Channel(Session *session, LIBSSH2_CHANNEL *channel);
    ~Channel();

    bool supportsWrite() { return true; }

    size_t write(const void *buffer, size_t length);

    void close(CloseType type = BOTH);
private:
    Session *m_session;
    LIBSSH2_CHANNEL *m_channel;
};

}}

#endif
