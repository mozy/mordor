// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "http.h"

#include <boost/bind.hpp>

#include <algorithm>
#include <iostream>

#include "mordor/common/assert.h"

static boost::posix_time::time_facet rfc1123Facet_out("%a, %d %b %Y %H:%M:%S GMT",
        boost::posix_time::time_facet::period_formatter_type(),
        boost::posix_time::time_facet::special_values_formatter_type(),
        boost::posix_time::time_facet::date_gen_formatter_type(),
        1 /* starting refcount, so this never gets deleted */);

std::string
HTTP::quote(const std::string &str, bool alwaysQuote, bool comment)
{
    if (comment)
        alwaysQuote = true;
    if (str.empty())
        return comment ? "()" : "\"\"";

    if (!alwaysQuote && str.find_first_not_of("!#$%&'*+-./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ^_`abcdefghijklmnopqrstuvwxyz|~") == std::string::npos) {
        return str;
    }

    std::string result;
    result.reserve(str.length() + 2);
    result.append(1, comment ? '(' : '"');

    // qdtext = OCTET - CTL - '"' - '\\' | LWS
    // CR, LF, and HT are part of LWS, and are allowed
    // DEL is a CTL, and is not allowed
    // \ is the escape character, and must be escaped itself
    // " indicates end-of-string, and must be escaped
    static const char * escapedQuote =
        "\0\x01\x02\x03\x04\x05\x06\x07\x08\x0b\x0c\x0e\x0f"
        "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
        "\x7f\\\"";
    static const char * escapedComment =
        "\0\x01\x02\x03\x04\x05\x06\x07\x08\x0b\x0c\x0e\x0f"
        "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
        "\x7f\\()";
    const char *escaped = comment ? escapedComment : escapedQuote;
    size_t escapedCount = comment ? 33 : 32;
    size_t leftParens = 0, rightParens = 0;
    if (comment) {
        rightParens = std::count(str.begin(), str.end(), ')');
    }

    size_t lastEscape = 0;
    size_t nextEscape = str.find_first_of(escaped, 0, escapedCount);
    while (nextEscape != std::string::npos) {
        if (str[nextEscape] == '(' && rightParens > 0) {
            // Let this one through without quoting (there is at least one
            // more matching right paren coming up)
            ++leftParens;
            --rightParens;
            result.append(str.substr(lastEscape, nextEscape - lastEscape + 1));
        } else if (str[nextEscape] == ')' && leftParens > 0) {
            // Let this one through without quoting (it matches a previous
            // left paren)
            --leftParens;
            result.append(str.substr(lastEscape, nextEscape - lastEscape + 1));
        } else {
            // Unmatching right parens
            if (str[nextEscape] == ')') {
                ASSERT(rightParens > 0);
                --rightParens;
            }
            result.append(str.substr(lastEscape, nextEscape - lastEscape));
            result.append(1, '\\');
            result.append(1, str[nextEscape]);
        }
        lastEscape = nextEscape + 1;
        nextEscape = str.find_first_of(escaped, lastEscape, escapedCount);
    }
    result.append(str.substr(lastEscape));
    result.append(1, comment ? ')' : '"');
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

static std::ostream& operator<<(std::ostream& os, const std::vector<std::string>& list)
{
    for (std::vector<std::string>::const_iterator it(list.begin());
        it != list.end();
        ++it) {
        if (it != list.begin())
            os << ", ";
        os << *it;
    }
    return os;
}

struct serializeStringMapWithRequiredValue
{
    serializeStringMapWithRequiredValue(const HTTP::StringMap &m,
        const char *delim = ";", bool leadingDelimiter = true)
        : map(m),
          delimiter(delim),
          lead(leadingDelimiter)
    {}
    const HTTP::StringMap& map;
    const char *delimiter;
    bool lead;
};

struct serializeStringMapWithOptionalValue
{
    serializeStringMapWithOptionalValue(const HTTP::StringMap &m) : map(m) {}
    const HTTP::StringMap& map;
};

static std::ostream& operator<<(std::ostream& os, const serializeStringMapWithRequiredValue &map)
{
    for (HTTP::StringMap::const_iterator it(map.map.begin());
        it != map.map.end();
        ++it) {
        if (map.lead || it != map.map.begin())
            os << map.delimiter;
        os << it->first << "=" << HTTP::quote(it->second);
    }
    return os;
}

static std::ostream& operator<<(std::ostream& os, const serializeStringMapWithOptionalValue &map)
{
    for (HTTP::StringMap::const_iterator it(map.map.begin());
        it != map.map.end();
        ++it) {
        os << ";" << it->first;
        if (!it->second.empty())
            os << "=" << HTTP::quote(it->second);
    }
    return os;
}

static std::ostream& operator<<(std::ostream& os, const HTTP::RangeSet& set)
{
    ASSERT(!set.empty());
    os << "bytes=";
    for (HTTP::RangeSet::const_iterator it(set.begin());
        it != set.end();
        ++it) {
        if (it != set.begin())
            os << ", ";
        if (it->first != ~0ull)
            os << it->first;
        os << "-";
        if (it->second != ~0ull)
            os << it->second;
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

const char *HTTP::reason(Status s)
{
    switch (s) {
        case CONTINUE:
            return "Continue";
        case SWITCHING_PROTOCOL:
            return "Switching Protocols";

        case OK:
            return "OK";
        case CREATED:
            return "Created";
        case ACCEPTED:
            return "Accepted";
        case NON_AUTHORITATIVE_INFORMATION:
            return "Non-Authoritative Information";
        case NO_CONTENT:
            return "No Content";
        case RESET_CONTENT:
            return "Reset Content";
        case PARTIAL_CONTENT:
            return "Partial Content";

        case MULTIPLE_CHOICES:
            return "Multiple Choices";
        case MOVED_PERMANENTLY:
            return "Moved Permanently";
        case FOUND:
            return "Found";
        case SEE_OTHER:
            return "See Other";
        case NOT_MODIFIED:
            return "Not Modified";
        case USE_PROXY:
            return "Use Proxy";
        case TEMPORARY_REDIRECT:
            return "Temporary Redirect";

        case BAD_REQUEST:
            return "Bad Request";
        case UNAUTHORIZED:
            return "Unauthorized";
        case PAYMENT_REQUIRED:
            return "Payment Required";
        case FORBIDDEN:
            return "Forbidden";
        case NOT_FOUND:
            return "Not Found";
        case METHOD_NOT_ALLOWED:
            return "Method Not Allowed";
        case NOT_ACCEPTABLE:
            return "Not Acceptable";
        case PROXY_AUTHENTICATION_REQUIRED:
            return "Proxy Authentication Required";
        case REQUEST_TIMEOUT:
            return "Request Time-out";
        case CONFLICT:
            return "Conflict";
        case GONE:
            return "Gone";
        case LENGTH_REQUIRED:
            return "Length Required";
        case PRECONDITION_FAILED:
            return "Precondition Failed";
        case REQUEST_ENTITY_TOO_LARGE:
            return "Request Entity Too Large";
        case REQUEST_URI_TOO_LONG:
            return "Request-URI Too Long";
        case UNSUPPORTED_MEDIA_TYPE:
            return "Unsupported Media Type";
        case REQUESTED_RANGE_NOT_SATISFIABLE:
            return "Requested range not satisfiable";
        case EXPECTATION_FAILED:
            return "Expectation Failed";

        case INTERNAL_SERVER_ERROR:
            return "Internal Server Error";
        case NOT_IMPLEMENTED:
            return "Not Implemented";
        case BAD_GATEWAY:
            return "Bad Gateway";
        case SERVICE_UNAVAILABLE:
            return "Service Unavailable";
        case GATEWAY_TIMEOUT:
            return "Gateway Time-out";
        case HTTP_VERSION_NOT_SUPPORTED:
            return "HTTP Version not supported";

        default:
            return "<INVALID>";
    }
}

bool
HTTP::AcceptValueWithParameters::operator ==(const AcceptValueWithParameters &rhs) const
{
    return stricmp(value.c_str(), rhs.value.c_str()) == 0 &&
        parameters == rhs.parameters;
}

bool
HTTP::isAcceptable(const HTTP::ChallengeList &list, const std::string &scheme)
{
    for (HTTP::ChallengeList::const_iterator it = list.begin();
        it != list.end();
        ++it) {
        if (stricmp(it->scheme.c_str(), scheme.c_str()) == 0)
            return true;
    }
    return false;
}

bool
HTTP::isAcceptable(const HTTP::AcceptList &list, const AcceptValueWithParameters &value,
                   bool defaultMissing)
{
    for (HTTP::AcceptList::const_iterator it(list.begin());
        it != list.end();
        ++it) {
        if (*it == value) {
            return it->qvalue > 0;
        }
    }
    return defaultMissing;
}

bool
HTTP::isPreferred(const HTTP::AcceptList &list, const AcceptValueWithParameters &lhs,
                  const AcceptValueWithParameters &rhs)
{
    ASSERT(lhs != rhs);
    unsigned int lQvalue = ~0u, rQvalue = ~0u;
    for (HTTP::AcceptList::const_iterator it(list.begin());
        it != list.end();
        ++it) {
        if (*it == lhs) {
            lQvalue = it->qvalue;
            if (lQvalue == ~0u)
                lQvalue = 1000;
        } else if (*it == rhs) {
            rQvalue = it->qvalue;
            if (rQvalue == ~0u)
                rQvalue = 1000;
        }
        if (lQvalue != ~0u && rQvalue != ~0u)
            break;
    }
    if (lQvalue == ~0u)
        lQvalue = 0;
    if (rQvalue == ~0u)
        rQvalue = 0;
    return lQvalue > rQvalue;
}

const
HTTP::AcceptValueWithParameters *
HTTP::preferred(const HTTP::AcceptList &accept, const HTTP::AcceptList &available)
{
    ASSERT(!available.empty());
#ifdef _DEBUG
    // Assert that the available list is ordered
    for (HTTP::AcceptList::const_iterator it(available.begin());
        it != available.end();
        ++it) {
        ASSERT(it->qvalue <= 1000);
        HTTP::AcceptList::const_iterator next(it);
        ++next;
        if (next != available.end())
            ASSERT(it->qvalue >= next->qvalue);
    }
#endif
    HTTP::AcceptList::const_iterator availableIt(available.begin());
    while (availableIt != available.end()) {
        HTTP::AcceptList::const_iterator nextIt(availableIt);
        ++nextIt;
        while (nextIt != available.end() && nextIt->qvalue == availableIt->qvalue)
            ++nextIt;
        AcceptList preferred;
        for (;
            availableIt != nextIt;
            ++availableIt) {
            if (isAcceptable(accept, *availableIt))
                preferred.push_back(*availableIt);
        }
        if (!preferred.empty()) {
            std::stable_sort(preferred.begin(), preferred.end(), boost::bind(
                &isPreferred, boost::ref(accept), _1, _2));
            return &*std::find(available.begin(), nextIt, preferred.front());
        }
    }
    return NULL;
}

std::ostream& operator<<(std::ostream& os, HTTP::Method m)
{
    if (m < HTTP::GET || m > HTTP::TRACE)
        return os << "INVALID";
    return os << HTTP::methods[(size_t)m];
}

std::ostream& operator<<(std::ostream& os, HTTP::Status s)
{
    return os << (int)s;
}

std::ostream& operator<<(std::ostream& os, HTTP::Version v)
{
    if (v.major == (unsigned char)~0 || v.minor == (unsigned char)~0)
        return os << "HTTP/0.0";
    return os << "HTTP/" << (int)v.major << "." << (int)v.minor;
}

std::ostream& operator<<(std::ostream& os, const HTTP::ETag &e)
{
    if (e.unspecified)
        return os << "*";
    if (e.weak)
        os << "W/";
    return os << HTTP::quote(e.value, true);
}

std::ostream& operator<<(std::ostream& os, const HTTP::ETagSet &v)
{
    ASSERT(!v.empty());
    for (HTTP::ETagSet::const_iterator it = v.begin();
        it != v.end();
        ++it) {
        if (it != v.begin())
            os << ", ";
        ASSERT(!it->unspecified || v.size() == 1);
        os << *it;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const HTTP::Product &p)
{
    ASSERT(!p.product.empty());
    os << p.product;
    if (!p.version.empty())
        os << "/" << p.version;
    return os;
}

std::ostream& operator<<(std::ostream& os, const HTTP::ProductList &l)
{
    ASSERT(!l.empty());
    for (HTTP::ProductList::const_iterator it = l.begin();
        it != l.end();
        ++it) {
        if (it != l.begin())
            os << ", ";
        os << *it;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const HTTP::ProductAndCommentList &l)
{
    ASSERT(!l.empty());
    for (HTTP::ProductAndCommentList::const_iterator it = l.begin();
        it != l.end();
        ++it) {
        if (it != l.begin())
            os << " ";
        const HTTP::Product *product = boost::get<HTTP::Product>(&*it);
        if (product)
            os << *product;
        else
            os << HTTP::quote(boost::get<std::string>(*it), true, true);        
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const HTTP::ValueWithParameters &v)
{
    ASSERT(!v.value.empty());
    return os << v.value << serializeStringMapWithRequiredValue(v.parameters);
}

std::ostream& operator<<(std::ostream& os, const HTTP::ParameterizedList &l)
{
    for (HTTP::ParameterizedList::const_iterator it(l.begin());
        it != l.end();
        ++it) {
        if (it != l.begin())
            os << ", ";
        os << *it;
    }
    return os;
}

std::ostream &operator<<(std::ostream &os, const HTTP::AuthParams &a)
{
    ASSERT(a.base64.empty() || a.parameters.empty());
    os << a.scheme;
    if (!a.base64.empty())
        os << ' ' << a.base64;
    if (!a.parameters.empty())
        os << serializeStringMapWithRequiredValue(a.parameters, ", ", false);
    return os;
}

std::ostream &operator<<(std::ostream &os, const HTTP::ChallengeList &l)
{
    for (HTTP::ChallengeList::const_iterator it(l.begin());
        it != l.end();
        ++it) {
        if (it != l.begin())
            os << ", ";
        os << *it;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const HTTP::KeyValueWithParameters &v)
{
    ASSERT(!v.key.empty());
    os << v.key;
    if (!v.value.empty())
        os << "=" << HTTP::quote(v.value)
            << serializeStringMapWithOptionalValue(v.parameters);
    return os;
}

std::ostream& operator<<(std::ostream& os, const HTTP::ParameterizedKeyValueList &l)
{
    ASSERT(!l.empty());
    for (HTTP::ParameterizedKeyValueList::const_iterator it(l.begin());
        it != l.end();
        ++it) {
        if (it != l.begin())
            os << ", ";
        os << *it;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const HTTP::MediaType &m)
{
    ASSERT(!m.type.empty());
    ASSERT(!m.subtype.empty());
    return os << m.type << "/" << m.subtype << serializeStringMapWithRequiredValue(m.parameters);
}

std::ostream& operator<<(std::ostream& os, const HTTP::ContentRange &cr)
{
    os << "bytes ";
    if (cr.first == ~0ull || cr.last == ~0ull)
        os << "*/";
    else
        os << cr.first << "-" << cr.last << "/";        
    if (cr.instance == ~0ull)
        os << "*";
    else
        os << cr.instance;
    return os;
}

std::ostream& operator<<(std::ostream& os, const HTTP::AcceptValueWithParameters &v)
{
    ASSERT(!v.value.empty());
    os << v.value << serializeStringMapWithRequiredValue(v.parameters);
    if (v.qvalue != ~0u) {
        ASSERT(v.qvalue <= 1000);
        unsigned int qvalue = v.qvalue;
        unsigned int curPlace = 1000;
        while (curPlace > 0 && qvalue > 0) {
            if (curPlace == 100)
                os << ".";
            if (qvalue >= curPlace)
                os << "1";
            else
                os << "0";
            qvalue -= curPlace;
            curPlace /= 10;
        }
        os << serializeStringMapWithOptionalValue(v.acceptParams);
    } else {
        ASSERT(v.acceptParams.empty());
    }

    if (!v.value.empty())
        os << "=" << HTTP::quote(v.value)
            << serializeStringMapWithOptionalValue(v.parameters);
    return os;
}

std::ostream& operator<<(std::ostream& os, const HTTP::AcceptList &l)
{
    ASSERT(!l.empty());
    for (HTTP::AcceptList::const_iterator it(l.begin());
        it != l.end();
        ++it) {
        if (it != l.begin())
            os << ", ";
        os << *it;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const HTTP::RequestLine &r)
{
    if (!r.uri.isDefined())
        return os << r.method << " * " << r.ver;
    else
        return os << r.method << " " << r.uri << " " << r.ver;
}

std::ostream& operator<<(std::ostream& os, const HTTP::StatusLine &s)
{
    ASSERT(!s.reason.empty());
    return os << s.ver << " " << s.status << " " << s.reason;
}

std::ostream& operator<<(std::ostream& os, const HTTP::GeneralHeaders &g)
{
    os.imbue(std::locale(os.getloc(), &rfc1123Facet_out));
    if (!g.connection.empty())
        os << "Connection: " << g.connection << "\r\n";
    if (!g.date.is_not_a_date_time())
        os << "Date: " << g.date << "\r\n";
    if (!g.proxyConnection.empty())
        os << "Proxy-Connection: " << g.proxyConnection << "\r\n";
    if (!g.trailer.empty())
        os << "Trailer: " << g.trailer << "\r\n";
    if (!g.transferEncoding.empty())
        os << "Transfer-Encoding: " << g.transferEncoding << "\r\n";
    if (!g.upgrade.empty())
        os << "Upgrade: " << g.upgrade << "\r\n";
    return os;
}

std::ostream& operator<<(std::ostream& os, const HTTP::RequestHeaders &r)
{
    os.imbue(std::locale(os.getloc(), &rfc1123Facet_out));
    if (!r.authorization.scheme.empty())
        os << "Authorization: " << r.authorization << "\r\n";
    if (!r.expect.empty())
        os << "Expect: " << r.expect << "\r\n";
    if (!r.host.empty())
        os << "Host: " << r.host << "\r\n";
    if (!r.ifMatch.empty())
        os << "If-Match: " << r.ifMatch << "\r\n";
    if (!r.ifModifiedSince.is_not_a_date_time())
        os << "If-Modified-Since: " << r.ifModifiedSince << "\r\n";
    if (!r.ifNoneMatch.empty())
        os << "If-None-Match: " << r.ifNoneMatch << "\r\n";
    const HTTP::ETag *ifRangeEtag = boost::get<HTTP::ETag>(&r.ifRange);
    if (ifRangeEtag && !ifRangeEtag->unspecified)
        os << "If-Range: " << *ifRangeEtag << "\r\n";
    const boost::posix_time::ptime *ifRangeHttpDate = boost::get<boost::posix_time::ptime>(&r.ifRange);
    if (ifRangeHttpDate && !ifRangeHttpDate->is_not_a_date_time())
        os << "If-Range: " << *ifRangeHttpDate << "\r\n";
    if (!r.ifUnmodifiedSince.is_not_a_date_time())
        os << "If-Unmodified-Since: " << r.ifUnmodifiedSince << "\r\n";
    if (!r.proxyAuthorization.scheme.empty())
        os << "Proxy-Authorization: " << r.proxyAuthorization << "\r\n";
    if (!r.range.empty())
        os << "Range: " << r.range << "\r\n";
    if (r.referer.isDefined())
        os << "Referer: " << r.referer << "\r\n";
    if (!r.te.empty())
        os << "TE: " << r.te << "\r\n";
    if (!r.userAgent.empty())
        os << "User-Agent: " << r.userAgent << "\r\n";
    return os;
}

std::ostream& operator<<(std::ostream& os, const HTTP::ResponseHeaders &r)
{
    os.imbue(std::locale(os.getloc(), &rfc1123Facet_out));
    if (!r.acceptRanges.empty())
        os << "Accept-Ranges: " << r.acceptRanges << "\r\n";
    if (!r.eTag.unspecified)
        os << "ETag: " << r.eTag << "\r\n";
    if (r.location.isDefined())
        os << "Location: " << r.location << "\r\n";
    if (!r.proxyAuthenticate.empty())
        os << "Proxy-Authenticate: " << r.proxyAuthenticate << "\r\n";
    const boost::posix_time::ptime *retryAfterHttpDate = boost::get<boost::posix_time::ptime>(&r.retryAfter);
    if (retryAfterHttpDate && !retryAfterHttpDate->is_not_a_date_time())
        os << "Retry-After: " << *retryAfterHttpDate << "\r\n";
    const unsigned long long *retryAfterDeltaSeconds = boost::get<unsigned long long>(&r.retryAfter);
    if (retryAfterDeltaSeconds && *retryAfterDeltaSeconds != ~0ull)
        os << "Retry-After: " << *retryAfterDeltaSeconds << "\r\n";
    if (!r.server.empty())
        os << "Server: " << r.server << "\r\n";
    if (!r.wwwAuthenticate.empty())
        os << "WWW-Authenticate: " << r.wwwAuthenticate << "\r\n";
    return os;
}

std::ostream& operator<<(std::ostream& os, const HTTP::EntityHeaders &e)
{
    os.imbue(std::locale(os.getloc(), &rfc1123Facet_out));
    if (!e.contentEncoding.empty())
        os << "Content-Encoding: " << e.contentEncoding << "\r\n";
    if (e.contentLength != ~0ull)
        os << "Content-Length: " << e.contentLength << "\r\n";
    if (e.contentRange.first != ~0ull || e.contentRange.last != ~0ull || e.contentRange.instance != ~0ull)
        os << "Content-Range: " << e.contentRange << "\r\n";
    if (!e.contentType.type.empty() && !e.contentType.subtype.empty())
        os << "Content-Type: " << e.contentType << "\r\n";
    if (!e.expires.is_not_a_date_time())
        os << "Expires: " << e.expires << "\r\n";
    if (!e.lastModified.is_not_a_date_time())
        os << "Last-Modified: " << e.lastModified << "\r\n";
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
