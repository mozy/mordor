// Copyright (c) 2009 - Decho Corp.

#include "http.h"

#include <cassert>
#include <iostream>

static std::string quote(const std::string& str)
{
    if (str.empty())
        return "\"\"";

    if (str.find_first_of("!#$%&'*+-./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ^_`abcdefghijklmnopqrstuvwxyz|~") == std::string::npos) {
        return str;
    }

    std::string result;
    result.reserve(str.length() + 2);
    result.append(1, '"');

    size_t lastEscape = 0;
    size_t nextEscape = std::min(str.find('\\'), str.find('"'));
    while (nextEscape != std::string::npos) {
        result.append(str.substr(lastEscape, nextEscape - lastEscape));
        result.append(1, '\\');
        result.append(1, str[nextEscape]);
        lastEscape = nextEscape + 1;
        nextEscape = std::min(str.find('\\', lastEscape), str.find('"', lastEscape));
    }
    result.append(str.substr(lastEscape));
    result.append(1, '"');
    return result;
}

static std::ostream& operator<<(std::ostream& os, const HTTP::StringSet& set)
{
    for (HTTP::StringSet::const_iterator it(set.begin());
        it != set.end();
        ++it) {
        if (it != set.begin())
            os << ", ";
        os << *it;
    }
    return os;
}

static std::ostream& operator<<(std::ostream& os, const HTTP::StringMap& map)
{
    for (HTTP::StringMap::const_iterator it(map.begin());
        it != map.end();
        ++it) {
        os << ";" << it->first << "=" << quote(it->second);
    }
    return os;
}

const char *HTTP::methods[] = {
    "GET",
    "HEAD",
    "POST",
    "PUT",
    "DELETE",
    "CONNECT",
    "OPTIONS",
    "TRACE"
};

std::ostream& operator<<(std::ostream& os, HTTP::Method m)
{
    assert(m >= HTTP::GET && m <= HTTP::TRACE);
    return os << HTTP::methods[(size_t)m];
}

std::ostream& operator<<(std::ostream& os, HTTP::Version v)
{
    assert(v.major != ~0 && v.minor != ~0);
    return os << "HTTP/" << v.major << "." << v.minor;    
}

std::ostream& operator<<(std::ostream& os, const HTTP::ValueWithParameters &v)
{
    assert(!v.value.empty());
    return os << v.value << v.parameters;
}

std::ostream& operator<<(std::ostream& os, const HTTP::ParameterizedList &l)
{
    for (HTTP::ParameterizedList::const_iterator it(l.begin());
        it != l.end();
        ++it) {
        if (it != l.begin())
            os << ", ";
        os << it->value << it->parameters;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const HTTP::MediaType &m)
{
    assert(!m.type.empty());
    assert(!m.subtype.empty());
    return os << m.type << "/" << m.subtype << m.parameters;
}

std::ostream& operator<<(std::ostream& os, const HTTP::RequestLine &r)
{
    return os << r.method << " " << r.uri << " " << r.ver;
}

std::ostream& operator<<(std::ostream& os, const HTTP::StatusLine &s)
{
    assert(!s.reason.empty());
    return os << s.ver << " " << (int)s.status << " " << s.reason;
}

std::ostream& operator<<(std::ostream& os, const HTTP::GeneralHeaders &g)
{
    if (!g.connection.empty())
        os << "Connection: " << g.connection << "\r\n";
    if (!g.transferEncoding.empty())
        os << "Transfer-Encoding: " << g.transferEncoding << "\r\n";
    return os;
}

std::ostream& operator<<(std::ostream& os, const HTTP::RequestHeaders &r)
{
    if (!r.host.empty())
        os << "Host: " << r.host << "\r\n";
    return os;
}

std::ostream& operator<<(std::ostream& os, const HTTP::ResponseHeaders &r)
{
    if (r.location.isDefined())
        os << "Location: " << r.location << "\r\n";
    return os;
}

std::ostream& operator<<(std::ostream& os, const HTTP::EntityHeaders &e)
{
    if (e.contentLength != ~0u)
        os << "Content-Length: " << e.contentLength << "\r\n";
    if (!e.contentType.type.empty() && !e.contentType.subtype.empty())
        os << "Content-Type: " << e.contentType << "\r\n";
    for (HTTP::StringMap::const_iterator it(e.extension.begin());
        it != e.extension.end();
        ++it) {
        os << it->first << ": " << it->second << "\r\n";
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const HTTP::Request &r)
{
    return os << r.requestLine << "\r\n"
        << r.general
        << r.request
        << r.entity << "\r\n";
}

std::ostream& operator<<(std::ostream& os, const HTTP::Response &r)
{
    return os << r.status << "\r\n"
        << r.general
        << r.response
        << r.entity << "\r\n";
}
