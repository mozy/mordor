#ifndef __MORDOR_HTTP_SERVLETS_CONFIG_H__
#define __MORDOR_HTTP_SERVLETS_CONFIG_H__
// Copyright (c) 2010 - Mozy, Inc.

#include <boost/shared_ptr.hpp>

#include "mordor/http/servlet.h"

namespace Mordor {
namespace HTTP {
class ServerRequest;
namespace Servlets {

class Config : public Servlet
{
public:
    enum Access
    {
        READONLY,
        READWRITE
    };

public:
    Config(Access access)
        : m_access(access)
    {}

    void request(boost::shared_ptr<ServerRequest> request)
    { return this->request(request, m_access); }
    void request(boost::shared_ptr<ServerRequest> request, Access access);

private:
    Access m_access;
};

}}}

#endif
