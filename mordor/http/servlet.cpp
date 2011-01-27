// Copyright (c) 2010 - Mozy, Inc.

#include "servlet.h"

#include "mordor/assert.h"
#include "server.h"

namespace Mordor {
namespace HTTP {

Servlet::ptr
ServletDispatcher::getServlet(const URI &uri)
{
    MORDOR_ASSERT(!uri.authority.userinfoDefined());
    Servlet::ptr result;
    if (m_servlets.empty())
        return result;
    URI copy(uri);
    copy.normalize();
    ServletHostMap::iterator it = m_servlets.find(copy.authority);
    if (it != m_servlets.end()) {
        result = getServlet(it->second, copy.path);
        if (result)
            return result;
    }
    if (copy.authority.hostDefined()) {
        it = m_servlets.find(URI::Authority());
        if (it != m_servlets.end())
            result = getServlet(it->second, copy.path);
    }
    return result;
}

void
ServletDispatcher::request(ServerRequest::ptr request)
{
    URI uri = request->request().requestLine.uri;
    if (!request->request().request.host.empty())
        uri.authority = request->request().request.host;
    Servlet::ptr servlet = getServlet(uri);
    if (servlet)
        servlet->request(request);
    else
        respondError(request, NOT_FOUND);
}

Servlet::ptr
ServletDispatcher::getServlet(ServletPathMap &vhost, URI::Path &path)
{
    Servlet::ptr result;
    ServletPathMap::iterator it;

    while (!path.segments.empty()) {
        it = vhost.find(path);
        if (it == vhost.end()) {
            if (!path.segments.back().empty()) {
                std::string empty;
                path.segments.back().swap(empty);
            } else {
                path.segments.pop_back();
            }
            continue;
        }
        const Servlet::ptr *servletPtr = boost::get<Servlet::ptr>(&it->second);
        if (servletPtr)
            result = *servletPtr;
        else
            result.reset(boost::get<boost::function<Servlet *()> >(
                it->second)());
        break;
    }
    return result;
}

void
ServletDispatcher::registerServlet(const URI &uri,
    const ServletOrCreator &servlet)
{
    MORDOR_ASSERT(!uri.authority.userinfoDefined());
    MORDOR_ASSERT(!uri.queryDefined());
    MORDOR_ASSERT(!uri.fragmentDefined());
    URI copy(uri);
    copy.normalize();
    ServletPathMap &vhost = m_servlets[copy.authority];
    MORDOR_ASSERT(vhost.find(copy.path) == vhost.end());
    vhost[copy.path] = servlet;
}

}}
