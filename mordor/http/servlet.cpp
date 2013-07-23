// Copyright (c) 2010 - Mozy, Inc.

#include "servlet.h"

#include "mordor/assert.h"
#include "server.h"

namespace Mordor {
namespace HTTP {

Servlet::ptr
ServletDispatcher::getServletPtr(ServletDispatcher::ServletOrCreator &creator)
{
    Servlet::ptr servletPtr = boost::get<Servlet::ptr>(creator);
    if (servletPtr)
        return servletPtr;
    else
        return Servlet::ptr(boost::get<boost::function<Servlet *()> >(creator)());
}

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
        // fall back to no authority defined scenario
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
ServletDispatcher::getServletWildcard(ServletWildcardPathMap &vhost, const URI::Path &path)
{
    // reverse order because '*' < [a-zA-Z]
    for (ServletWildcardPathMap::reverse_iterator it = vhost.rbegin();
        it != vhost.rend(); ++it) {
        if (wildcardPathMatch(it->first, path))
            return getServletPtr(it->second);
    }
    return Servlet::ptr();
}

Servlet::ptr
ServletDispatcher::getServlet(ServletPathMapPair &vhosts, const URI::Path &path)
{
    URI::Path copy(path);
    while (!copy.segments.empty()) {
        // search from non-wildcard path
        ServletPathMap::iterator it = vhosts.first.find(copy);
        if (it != vhosts.first.end())
            return getServletPtr(it->second);
        if (m_enableWildcard) {
            Servlet::ptr result = getServletWildcard(vhosts.second, copy);
            if (result)
                return result;
        }
        // can't find any match, shorten the path
        if (!copy.segments.back().empty()) {
            copy.segments.back().clear();
        } else {
            copy.segments.pop_back();
        }
    }
    return Servlet::ptr();
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
    ServletPathMap * vhost = NULL;
    if (m_enableWildcard && isWildcardPath(copy.path))
        vhost = &(m_servlets[copy.authority].second);
    else
        vhost = &(m_servlets[copy.authority].first);
    MORDOR_ASSERT(vhost->find(copy.path) == vhost->end());
    (*vhost)[copy.path] = servlet;
}

bool
ServletDispatcher::wildcardPathMatch(const URI::Path &wildPath, const URI::Path &path)
{
    if (path.segments.size() != wildPath.segments.size())
        return false;

    for (size_t i = 0; i < path.segments.size(); ++i) {
        if (wildPath.segments[i] == "*")
            continue;
        if (wildPath.segments[i] != path.segments[i])
            return false;
    }
    return true;
}

bool
ServletDispatcher::isWildcardPath(const URI::Path &path)
{
    for (std::vector<std::string>::const_iterator it=path.segments.begin();
        it!=path.segments.end(); ++it) {
        if (*it == "*") return true;
    }
    return false;
}

}}
