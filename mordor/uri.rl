// Copyright (c) 2009 - Mozy, Inc.
/* To compile to .cpp:
   ragel uri.rl -G2 -o uri.cpp
*/

#include "mordor/pch.h"

#include "mordor/uri.h"

#include <sstream>

#include "mordor/ragel.h"
#include "mordor/string.h"
#include "mordor/version.h"

namespace Mordor {

static const std::string unreserved("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~");
static const std::string sub_delims("!$&'()*+,;=");
static const std::string scheme("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-.");
static const std::string userinfo("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~" "!$&'()*+,;=" ":");
static const std::string host("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~" "!$&'()*+,;=" ":");
static const std::string pchar("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~" "!$&'()*+,;=" ":@");
static const std::string path("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~" "!$&'()*+,;=" ":@" "/");
static const std::string segment_nc("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~" "!$&'()*+,;=" "@");
static const std::string query("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~" "!$&'()*+,;=" ":@" "/?");
static const std::string queryString("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~" "!$'()*+," ":@" "/?");

static std::string escape(const std::string& str, const std::string& allowedChars, bool spaceAsPlus = false)
{
    const char *hexdigits = "0123456789ABCDEF";
    std::string result(str);

    const char *c = str.c_str();
    const char *end = c + str.length();
    bool differed = false;
    while(c < end)
    {
        if (allowedChars.find(*c) == std::string::npos) {
            if (!differed) {
                result.resize(c - str.c_str());
                differed = true;
            }
            if (*c == ' ' && spaceAsPlus) {
                result.append(1, '+');
            } else {
                result.append(1, '%');
                result.append(1, hexdigits[(unsigned char)*c >> 4]);
                result.append(1, hexdigits[*c & 0xf]);
            }
        } else {
            if (differed) {
                result.append(1, *c);
            }
        }
        ++c;
    }

    if (differed) {
        MORDOR_ASSERT(result.length() >= str.length());
    } else {
        MORDOR_ASSERT(result == str);
    }
    return result;
}

std::string unescape(const std::string& str, bool spaceAsPlus = false)
{
    std::string result = str;

    const char *c = str.c_str();
    const char *end = c + str.length();
    bool differed = false;
    while (c < end)
    {
        if (*c == '%') {
            MORDOR_ASSERT(c + 2 < end);
            if (!differed) {
                result.resize(c - str.c_str());
                differed = true;
            }
            char decoded;
            ++c;
            if (*c >= 'a' && *c <= 'f')
                decoded = (*c - 'a' + 10) << 4;
            else if (*c >= 'A' && *c <= 'F')
                decoded = (*c - 'A' + 10) << 4;
            else {
                MORDOR_ASSERT(*c >= '0' && *c <='9');
                decoded = (*c - '0') << 4;
            }
            ++c;
            if (*c >= 'a' && *c <= 'f')
                decoded |= *c - 'a' + 10;
            else if (*c >= 'A' && *c <= 'F')
                decoded |= *c - 'A' + 10;
            else {
                MORDOR_ASSERT(*c >= '0' && *c <='9');
                decoded |= *c - '0';
            }
            result.append(1, decoded);
        } else if (*c == '+' && spaceAsPlus) {
            if (!differed) {
                result.resize(c - str.c_str());
                differed = true;
            }
            result.append(1, ' ');
        } else if (differed) {
            result.append(1, *c);
        }
        ++c;
    }
    return result;
}

std::string
URI::encode(const std::string &str, CharacterClass charClass)
{
    switch (charClass) {
        case UNRESERVED:
            return escape(str, unreserved, false);
        case QUERYSTRING:
            return escape(str, Mordor::queryString, true);
        default:
            MORDOR_NOTREACHED();
    }
}

std::string
URI::decode(const std::string &str, CharacterClass charClass)
{
    switch (charClass) {
        case UNRESERVED:
            return unescape(str, false);
        case QUERYSTRING:
            return unescape(str, true);
        default:
            MORDOR_NOTREACHED();
    }
}

%%{
    # See RFC 3986: http://www.ietf.org/rfc/rfc3986.txt

    machine uri_parser;

    gen_delims = ":" | "/" | "?" | "#" | "[" | "]" | "@";
    sub_delims = "!" | "$" | "&" | "'" | "(" | ")" | "*" | "+" | "," | ";" | "=";
    reserved = gen_delims | sub_delims;
    unreserved = alpha | digit | "-" | "." | "_" | "~";
    pct_encoded = "%" xdigit xdigit;

    action marku { mark = fpc; }
    action save_scheme
    {
        m_uri->scheme(unescape(std::string(mark, fpc - mark)));
        mark = NULL;
    }

    scheme = (alpha | digit | "+" | "-" | ".")+ >marku %save_scheme;

    action save_port
    {
        if (fpc == mark)
            m_uri->authority.port(-1);
        else
            m_uri->authority.port(atoi(mark));
        mark = NULL;
    }
    action save_userinfo
    {
        m_uri->authority.userinfo(unescape(std::string(mark, fpc - mark)));
        mark = NULL;
    }
    action save_host
    {
        if (mark != NULL) {
            m_uri->authority.host(unescape(std::string(mark, fpc - mark)));
            mark = NULL;
        }
    }

    userinfo = (unreserved | pct_encoded | sub_delims | ":")*;
    dec_octet = digit | [1-9] digit | "1" digit{2} | 2 [0-4] digit | "25" [0-5];
    IPv4address = dec_octet "." dec_octet "." dec_octet "." dec_octet;
    h16 = xdigit{1,4};
    ls32 = (h16 ":" h16) | IPv4address;
    IPv6address = (                         (h16 ":"){6} ls32) |
                  (                    "::" (h16 ":"){5} ls32) |
                  ((             h16)? "::" (h16 ":"){4} ls32) |
                  (((h16 ":"){1} h16)? "::" (h16 ":"){3} ls32) |
                  (((h16 ":"){2} h16)? "::" (h16 ":"){2} ls32) |
                  (((h16 ":"){3} h16)? "::" (h16 ":"){1} ls32) |
                  (((h16 ":"){4} h16)? "::"              ls32) |
                  (((h16 ":"){5} h16)? "::"              h16 ) |
                  (((h16 ":"){6} h16)? "::"                  );
    IPvFuture = "v" xdigit+ "." (unreserved | sub_delims | ":")+;
    IP_literal = "[" (IPv6address | IPvFuture) "]";
    reg_name = (unreserved | pct_encoded | sub_delims)*;
    host = IP_literal | IPv4address | reg_name;
    port = digit*;

    authority = ( (userinfo %save_userinfo "@")? host >marku %save_host (":" port >marku %save_port)? ) >marku;

    action save_segment
    {
        m_path->segments.push_back(unescape(std::string(mark, fpc - mark)));
        mark = NULL;
    }

    pchar = unreserved | pct_encoded | sub_delims | ":" | "@";
    segment = pchar* >marku %save_segment;
    segment_nz = pchar+ >marku %save_segment;
    segment_nz_nc = (pchar - ":")+ >marku %save_segment;

    action set_absolute
    {
        m_path->type = URI::Path::ABSOLUTE;
    }
    action set_relative
    {
        m_path->type = URI::Path::RELATIVE;
    }

    path_abempty = ("/" segment >set_absolute)*;
    path_absolute = "/" (segment_nz ("/" segment)*)?  >set_absolute;
    path_noscheme = segment_nz_nc >set_relative ("/" segment)*;
    path_rootless = segment_nz >set_relative ("/" segment)*;
    path_empty = "" %set_relative;
    path = (path_abempty | path_absolute | path_noscheme | path_rootless | path_empty);

    action save_query
    {
        m_uri->m_query = std::string(mark, fpc - mark);
        m_uri->m_queryDefined = true;
        mark = NULL;
    }
    action save_fragment
    {
        m_uri->fragment(unescape(std::string(mark, fpc - mark)));
        mark = NULL;
    }

    query = (pchar | "/" | "?")* >marku %save_query;
    fragment = (pchar | "/" | "?")* >marku %save_fragment;

    hier_part = ("//" authority path_abempty) | path_absolute | path_rootless | path_empty;

    relative_part = ("//" authority path_abempty) | path_absolute | path_noscheme | path_empty;
    relative_ref = relative_part ( "?" query )? ( "#" fragment )?;

    absolute_URI = scheme ":" hier_part ( "?" query )? ;
    # Obsolete, but referenced from HTTP, so we translate
    relative_URI = relative_part ( "?" query )?;

    URI = scheme ":" hier_part ( "?" query )? ( "#" fragment )?;
    URI_reference = URI | relative_ref;
}%%

%%{
        machine uri_parser_proper;
        include uri_parser;
        main := URI_reference;
        write data;
}%%

class URIParser : public RagelParser
{
public:
    URIParser(URI& uri)
    {
        m_uri = &uri;
        m_path = &m_uri->path;
    }

    void init()
    {
        RagelParser::init();
        %% write init;
    }

protected:
    void exec()
    {
#ifdef MSVC
#pragma warning(push)
#pragma warning(disable : 4244)
#endif
        %% write exec;
#ifdef MSVC
#pragma warning(pop)
#endif
    }

public:
    bool complete() const
    {
        return false;
    }

    bool final() const
    {
        return cs >= uri_parser_proper_first_final;
    }

    bool error() const
    {
        return cs == uri_parser_proper_error;
    }

private:
    URI *m_uri;
    URI::Path *m_path;
};

%%{
    machine uri_path_parser;
    include uri_parser;
    main := path;
    write data;
}%%
class URIPathParser : public RagelParser
{
public:
    URIPathParser(URI::Path& path)
    {
        m_path = &path;
    }

    void init()
    {
        RagelParser::init();
        %% write init;
    }

protected:
    void exec()
    {
#ifdef MSVC
#pragma warning(push)
#pragma warning(disable : 4244)
#endif
        %% write exec;
#ifdef MSVC
#pragma warning(pop)
#endif
    }

public:
    bool complete() const
    {
        return false;
    }

    bool final() const
    {
        return cs >= uri_path_parser_first_final;
    }

    bool error() const
    {
        return cs == uri_path_parser_error;
    }

private:
    URI::Path *m_path;
};

URI::URI()
{
    reset();
}

URI::URI(const std::string& uri)
{
    reset();
    *this = uri;
}

URI::URI(const char *uri)
{
    reset();
    *this = uri;
}

URI::URI(const Buffer &uri)
{
    reset();
    *this = uri;
}

URI&
URI::operator=(const std::string& uri)
{
    URIParser parser(*this);
    parser.run(uri);
    if (parser.error() || !parser.final())
        throw std::invalid_argument("uri");
    return *this;
}

URI&
URI::operator=(const Buffer &uri)
{
    URIParser parser(*this);
    parser.run(uri);
    if (parser.error() || !parser.final())
        throw std::invalid_argument("uri");
    return *this;
}

void
URI::reset()
{
    schemeDefined(false);
    authority.hostDefined(false);
    path.type = Path::RELATIVE;
    path.segments.clear();
    queryDefined(false);
    fragmentDefined(false);
}

URI::Authority::Authority()
{
    userinfoDefined(false);
    hostDefined(false);
    portDefined(false);
}

void
URI::Authority::normalize(const std::string& defaultHost, bool emptyHostValid,
    int defaultPort, bool emptyPortValid)
{
    for(size_t i = 0; i < m_host.length(); ++i)
        m_host[i] = tolower(m_host[i]);
    if (m_port == defaultPort)
        m_port = -1;
    if (m_port == -1 && !emptyPortValid)
        m_portDefined = false;
    if (m_host == defaultHost)
        m_host.clear();
    if (m_host.empty() && !emptyHostValid && !m_userinfoDefined && !m_portDefined)
        m_hostDefined = false;
}

std::string
URI::Authority::toString() const
{
    std::ostringstream os;
    os << *this;
    return os.str();
}


std::string
URI::Path::toString() const
{
    std::ostringstream os;
    os << *this;
    return os.str();
}

static int boolcmp(bool lhs, bool rhs)
{
    if (!lhs && rhs)
        return -1;
    if (lhs && !rhs)
        return 1;
    return 0;
}

int
URI::Authority::cmp(const Authority &rhs) const
{
    int x = boolcmp(m_hostDefined, rhs.m_hostDefined);
    if (x != 0) return x;
    x = strcmp(m_host.c_str(), rhs.m_host.c_str());
    if (x != 0) return x;
    x = boolcmp(m_portDefined, rhs.m_portDefined);
    if (x != 0) return x;
    x = m_port - rhs.m_port;
    if (x != 0) return x;
    x = boolcmp(m_userinfoDefined, rhs.m_userinfoDefined);
    if (x != 0) return x;
    return strcmp(m_userinfo.c_str(), rhs.m_userinfo.c_str());
}

bool
URI::Authority::operator==(const Authority &rhs) const
{
    return cmp(rhs) == 0;
}

std::ostream&
operator<<(std::ostream& os, const URI::Authority& authority)
{
    MORDOR_ASSERT(authority.hostDefined());
    if (authority.userinfoDefined()) {
        os << escape(authority.userinfo(), userinfo) << "@";
    }
    os << escape(authority.host(), host);
    if (authority.portDefined()) {
        os << ":";
        if (authority.port() > 0) {
            os << authority.port();
        }
    }
    return os;
}

URI::Path::Path()
{
    type = RELATIVE;
}

URI::Path::Path(const char *path)
{
    *this = path;
}

URI::Path::Path(const std::string& path)
{
    *this = path;
}

URI::Path&
URI::Path::operator=(const std::string& path)
{
    type = RELATIVE;
	segments.clear();
    URIPathParser parser(*this);
    parser.run(path);
    if (parser.error() || !parser.final())
        throw std::invalid_argument("uri");
    return *this;
}

void
URI::Path::removeDotComponents()
{
    for(size_t i = 0; i < segments.size(); ++i) {
        if (segments[i] == ".") {
            if (i + 1 == segments.size()) {
                segments[i].clear();
                continue;
            } else {
                segments.erase(segments.begin() + i);
                --i;
                continue;
            }
        }
        if (segments[i] == "..") {
            if (i == 0) {
                segments.erase(segments.begin());
                --i;
                continue;
            }
            if (i + 1 == segments.size()) {
                segments.resize(segments.size() - 1);
                segments.back().clear();
                --i;
                continue;
            }
            segments.erase(segments.begin() + i - 1, segments.begin() + i + 1);
            i -= 2;
            continue;
        }
    }
}

void
URI::Path::normalize(bool emptyPathValid)
{
    removeDotComponents();
    if (segments.empty() && !emptyPathValid)
        type = ABSOLUTE;
}

void
URI::Path::merge(const Path& rhs)
{
    MORDOR_ASSERT(rhs.type == RELATIVE);
    if (!segments.empty()) {
        segments.pop_back();
        segments.insert(segments.end(), rhs.segments.begin(), rhs.segments.end());
    } else {
        segments = rhs.segments;
    }
}

URI::Path::path_serializer
URI::Path::serialize(bool schemeless) const
{
    path_serializer result;
    result.p = this;
    result.schemeless = schemeless;
    return result;
}

std::ostream&
operator<<(std::ostream& os, const URI::Path::path_serializer &p)
{
    if (p.p->segments.empty() && p.p->type == URI::Path::ABSOLUTE) {
        return os << "/";
    }
    for (size_t i = 0; i < p.p->segments.size(); ++i) {
        if (i != 0 || p.p->type == URI::Path::ABSOLUTE) {
            os << "/";
        }
        if (i == 0 && p.p->type == URI::Path::RELATIVE && p.schemeless) {
            os << escape(p.p->segments[i], segment_nc);
        } else {
            os << escape(p.p->segments[i], pchar);
        }
    }
    return os;
}

std::ostream&
operator<<(std::ostream& os, const URI::Path& path)
{
    return os << path.serialize();
}

int
URI::Path::cmp(const Path &rhs) const
{
    if (type == ABSOLUTE && rhs.type == RELATIVE)
        return -1;
    if (type == RELATIVE && rhs.type == ABSOLUTE)
        return 1;
    std::vector<std::string>::const_iterator itl, itr;
    itl = segments.begin(); itr = rhs.segments.begin();
    while (true) {
        if (itl == segments.end() && itr != rhs.segments.end())
            return -1;
        if (itl != segments.end() && itr == rhs.segments.end())
            return 1;
        if (itl == segments.end() && itr == rhs.segments.end())
            return 0;
        int x = strcmp(itl->c_str(), itr->c_str());
        if (x != 0) return x;
        ++itl; ++itr;
    }
}

bool
URI::Path::operator==(const Path &rhs) const
{
    return type == rhs.type && segments == rhs.segments;
}

void
URI::normalize()
{
    for (size_t i = 0; i < m_scheme.size(); ++i)
        m_scheme[i] = tolower(m_scheme[i]);

    if (m_scheme == "http" || m_scheme == "https") {
        authority.normalize("", false, m_scheme.size() == 4 ? 80 : 443, false);
        path.normalize();
    } else if (m_scheme == "file") {
        authority.normalize("localhost", true);
        path.normalize();
    } else {
        authority.normalize();
        path.normalize();
    }
}

std::string
URI::query() const
{
    MORDOR_ASSERT(m_queryDefined);
    return unescape(m_query);
}

void
URI::query(const std::string &q)
{
    m_queryDefined = true;
    m_query = escape(q, Mordor::query);
}

std::string
URI::toString() const
{
    std::ostringstream os;
    os << *this;
    return os.str();
}

std::ostream&
operator<<(std::ostream& os, const URI& uri)
{
    if (uri.schemeDefined()) {
        os << escape(uri.scheme(), scheme) << ":";
    }

    if (uri.authority.hostDefined()) {
        os << "//" << uri.authority;
    }

    // Has scheme, but no authority, must ensure that an absolute path
    // doesn't begin with an empty segment (or could be mistaken for authority)
    if (uri.schemeDefined() && !uri.authority.hostDefined() &&
        uri.path.type == URI::Path::ABSOLUTE &&
        uri.path.segments.size() > 0 && uri.path.segments.front().empty()) {
        os << "/";
    }
    os << uri.path.serialize(!uri.schemeDefined());

    if (uri.queryDefined()) {
        os << "?" << escape(uri.query(), query);
    }

    if (uri.fragmentDefined()) {
        os << "#" << escape(uri.fragment(), query);
    }
    return os;
}

URI
URI::transform(const URI& base, const URI& relative)
{
    MORDOR_ASSERT(base.schemeDefined());

    URI target;
    if (relative.schemeDefined()) {
        target.scheme(relative.scheme());
        target.authority = relative.authority;
        target.path = relative.path;
        target.path.removeDotComponents();
        target.m_query = relative.m_query;
        target.m_queryDefined = relative.m_queryDefined;
    } else {
        if (relative.authority.hostDefined()) {
            target.authority = relative.authority;
            target.path = relative.path;
            target.path.removeDotComponents();
            target.m_query = relative.m_query;
            target.m_queryDefined = relative.m_queryDefined;
        } else {
            if (relative.path.isEmpty()) {
                target.path = base.path;
                if (relative.queryDefined()) {
                    target.query(relative.query());
                } else {
                    target.m_query = base.m_query;
                    target.m_queryDefined = base.m_queryDefined;
                }
            } else {
                if (relative.path.type == Path::ABSOLUTE) {
                    target.path = relative.path;
                } else {
                    target.path = base.path;
                    target.path.merge(relative.path);
                    if (!base.authority.hostDefined())
                        target.path.type = Path::ABSOLUTE;
                }
                target.path.removeDotComponents();
                target.m_query = relative.m_query;
                target.m_queryDefined = relative.m_queryDefined;
            }
            target.authority = base.authority;
        }
        target.scheme(base.scheme());
    }
    target.m_fragment = relative.m_fragment;
    target.m_fragmentDefined = relative.m_fragmentDefined;
    return target;
}

int
URI::cmp(const URI &rhs) const
{
    int x = boolcmp(m_schemeDefined, rhs.m_schemeDefined);
    if (x != 0) return x;
    x = strcmp(m_scheme.c_str(), rhs.m_scheme.c_str());
    if (x != 0) return x;
    x = authority.cmp(rhs.authority);
    if (x != 0) return x;
    x = path.cmp(rhs.path);
    if (x != 0) return x;
    x = boolcmp(m_queryDefined, rhs.m_queryDefined);
    if (x != 0) return x;
    x = strcmp(m_query.c_str(), rhs.m_query.c_str());
    if (x != 0) return x;
    x = boolcmp(m_fragmentDefined, rhs.m_fragmentDefined);
    if (x != 0) return x;
    return strcmp(m_fragment.c_str(), rhs.m_fragment.c_str());
}

bool
URI::operator<(const URI &rhs) const
{
    return cmp(rhs) < 0;
}

bool
URI::operator==(const URI &rhs) const
{
    return cmp(rhs) == 0;
}

%%{
    machine querystring_parser;

    action mark { mark = fpc; }
    action saveKey {
        m_iterator = m_qs.insert(std::make_pair(
            unescape(std::string(mark, fpc - mark), true), std::string()));
        mark = NULL;
    }
    action saveValue {
        MORDOR_ASSERT(m_iterator != m_qs.end());
        m_iterator->second = unescape(std::string(mark, fpc - mark), true);
        m_iterator = m_qs.end();
        mark = NULL;
    }

    sub_delims = "!" | "$" | "&" | "'" | "(" | ")" | "*" | "+" | "," | ";";
    unreserved = alpha | digit | "-" | "." | "_" | "~";
    pct_encoded = "%" xdigit xdigit;
    pchar = unreserved | pct_encoded | sub_delims | ":" | "@";
    querychar = (pchar | "/" | "?") -- '&' -- ';';
    key = querychar+;
    value = (querychar | '=')+;
    keyValue = key >mark %saveKey ('=' value >mark %saveValue)?;
    main := keyValue? ( ('&' | ';') keyValue? )*;
    write data;
}%%

class QueryStringParser : public RagelParser
{
public:
    QueryStringParser(URI::QueryString &qs)
    : m_qs(qs),
      m_iterator(m_qs.end())
    {}


    void init()
    {
        RagelParser::init();
        %% write init;
    }

    void exec()
    {
#ifdef MSVC
#pragma warning(push)
#pragma warning(disable : 4244)
#endif
        %% write exec;
#ifdef MSVC
#pragma warning(pop)
#endif
    }

    bool complete() const { return false; }
    bool final() const
    {
        return cs >= querystring_parser_first_final;
    }

    bool error() const
    {
        return cs == querystring_parser_error;
    }

private:
    URI::QueryString &m_qs;
    URI::QueryString::iterator m_iterator;
};

URI::QueryString &
URI::QueryString::operator =(const std::string &string)
{
    clear();
    QueryStringParser parser(*this);
    parser.run(string);
    if (!parser.final() || parser.error())
        MORDOR_THROW_EXCEPTION(std::invalid_argument("Invalid QueryString"));
    return *this;
}

URI::QueryString &
URI::QueryString::operator =(Stream &stream)
{
    clear();
    QueryStringParser parser(*this);
    parser.run(stream);
    if (!parser.final() || parser.error())
        MORDOR_THROW_EXCEPTION(std::invalid_argument("Invalid QueryString"));
    return *this;
}

std::string
URI::QueryString::toString() const
{
    std::ostringstream os;
    for (const_iterator it = begin();
        it != end();
        ++it) {
        if (it != begin()) {
            os << '&';
        }
        os << escape(it->first, Mordor::queryString, true);
        if (!it->second.empty())
            os << '=' << escape(it->second, Mordor::queryString, true);
    }
    return os.str();
}

}
