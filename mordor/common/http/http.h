#ifndef __HTTP_H__
#define __HTTP_H__
// Copyright (c) 2009 - Decho Corp.

#include <string.h>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

#include "mordor/common/uri.h"
#include "mordor/common/version.h"

#ifdef WINDOWS
#define stricmp _stricmp
#define strnicmp _strnicmp
#else
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

class HTTPException : public std::runtime_error
{
public:
    HTTPException() : std::runtime_error("") {}
};

namespace HTTP
{
    class IncompleteMessageHeaderException : public HTTPException
    { };

#ifdef DELETE
#undef DELETE
#endif
    enum Method
    {
        GET,
        HEAD,
        POST,
        PUT,
        DELETE,
        CONNECT,
        OPTIONS,
        TRACE
    };
    extern const char *methods[];

    enum Status
    {
        INVALID                          = 0,

        CONTINUE                         = 100,
        SWITCHING_PROTOCOL               = 101,
        
        OK                               = 200,
        CREATED                          = 201,
        ACCEPTED                         = 202,
        NON_AUTHORITATIVE_INFORMATION    = 203,
        NO_CONTENT                       = 204,
        RESET_CONTENT                    = 205,
        PARTIAL_CONTENT                  = 206,

        MULTIPLE_CHOICES                 = 300,
        MOVED_PERMANENTLY                = 301,
        FOUND                            = 302,
        SEE_OTHER                        = 303,
        NOT_MODIFIED                     = 304,
        USE_PROXY                        = 305,
        // UNUSED                        = 306,
        TEMPORARY_REDIRECT               = 307,

        BAD_REQUEST                      = 400,
        UNAUTHORIZED                     = 401,
        PAYMENT_REQUIRED                 = 402,
        FORBIDDEN                        = 403,
        NOT_FOUND                        = 404,
        METHOD_NOT_ALLOWED               = 405,
        NOT_ACCEPTABLE                   = 406,
        PROXY_AUTHENTICATION_REQUIRED    = 407,
        REQUEST_TIMEOUT                  = 408,
        CONFLICT                         = 409,
        GONE                             = 410,
        LENGTH_REQUIRED                  = 411,
        PRECONDITION_FAILED              = 412,
        REQUEST_ENTITY_TOO_LARGE         = 413,
        REQUEST_URI_TOO_LONG             = 414,
        UNSUPPORTED_MEDIA_TYPE           = 415,
        REQUESTED_RANGE_NOT_SATISFIABLE  = 416,
        EXPECTATION_FAILED               = 417,

        INTERNAL_SERVER_ERROR            = 500,
        NOT_IMPLEMENTED                  = 501,
        BAD_GATEWAY                      = 502,
        SERVICE_UNAVAILABLE              = 503,
        GATEWAY_TIMEOUT                  = 504,
        HTTP_VERSION_NOT_SUPPORTED       = 505
    };
    const char *reason(Status s);

    std::string quote(const std::string &str);

#ifdef major
#undef major
#endif
#ifdef minor
#undef minor
#endif

    struct Version
    {
        Version() : major(~0), minor(~0) {}
        Version(unsigned char m, unsigned char n) : major(m), minor(n) {}

        unsigned char major;
        unsigned char minor;

        bool operator==(const Version& rhs) const
        {
            return major == rhs.major && minor == rhs.minor;
        }
        bool operator!=(const Version& rhs) const
        {
            return !(*this == rhs);
        }
        bool operator<(const Version& rhs) const
        {
            if (major < rhs.major) return true; return minor < rhs.minor;
        }
        bool operator<=(const Version& rhs) const
        {
            if (major > rhs.major) return false;
            if (major < rhs.major) return true; return minor <= rhs.minor;
        }
        bool operator>(const Version& rhs) const
        {
            if (major > rhs.major) return true; return minor > rhs.minor;
        }
        bool operator>=(const Version& rhs) const
        {
            if (major < rhs.major) return false;
            if (major > rhs.major) return true; return minor >= rhs.minor;
        }
    };

    struct caseinsensitiveless
    {
        bool operator()(const std::string& lhs, const std::string& rhs) const
        {
            return stricmp(lhs.c_str(), rhs.c_str()) < 0;
        }
    };
    typedef std::set<std::string, caseinsensitiveless> StringSet;
    typedef std::map<std::string, std::string, caseinsensitiveless> StringMap;

    struct ValueWithParameters
    {
        std::string value;
        StringMap parameters;
    };

    typedef std::vector<ValueWithParameters> ParameterizedList;

    struct KeyValueWithParameters
    {
        std::string key;
        std::string value;
        StringMap parameters;
    };

    typedef std::vector<KeyValueWithParameters> ParameterizedKeyValueList;

    struct MediaType
    {
        std::string type;
        std::string subtype;
        StringMap parameters;
    };

    typedef std::vector<std::pair<unsigned long long, unsigned long long> > RangeSet;
    struct ContentRange
    {
        ContentRange() : first(~0ULL), last(~0ULL), instance(~0ULL) {}

        unsigned long long first;
        unsigned long long last;
        unsigned long long instance;
    };

    struct AcceptValueWithParameters
    {
        AcceptValueWithParameters() : qvalue(~0u) {}
        AcceptValueWithParameters(const std::string &v, unsigned int q = ~0u)
            : value(v), qvalue(q)
        {}

        std::string value;
        StringMap parameters;
        unsigned int qvalue;
        StringMap acceptParams;

        bool operator== (const AcceptValueWithParameters &rhs) const;
        bool operator!= (const AcceptValueWithParameters &rhs) const
        { return !(*this == rhs); }
    };

    typedef std::vector<AcceptValueWithParameters> AcceptList;

    struct RequestLine
    {
        RequestLine() : method(GET) {}

        Method method;
        URI uri;
        Version ver;        
    };

    struct StatusLine
    {
        StatusLine() : status(OK) {}

        Status status;
        std::string reason;
        Version ver;
    };

    struct GeneralHeaders
    {
        StringSet connection;
        ParameterizedList transferEncoding;
        StringSet trailer;
    };

    struct RequestHeaders
    {
        ValueWithParameters authorization;
        ParameterizedKeyValueList expect;
        std::string host;
        ValueWithParameters proxyAuthorization;
        RangeSet range;
        AcceptList te;
    };

    struct ResponseHeaders
    {
        StringSet acceptRanges;
        URI location;
        ParameterizedList proxyAuthenticate;
        ParameterizedList wwwAuthenticate;
    };

    struct EntityHeaders
    {
        EntityHeaders() : contentLength(~0ull) {}

        std::vector<std::string> contentEncoding;
        unsigned long long contentLength;
        ContentRange contentRange;
        MediaType contentType;
        StringMap extension;
    };

    struct Request
    {
        RequestLine requestLine;
        GeneralHeaders general;
        RequestHeaders request;
        EntityHeaders entity;

        std::string toString() const;
    };

    struct Response
    {
        StatusLine status;
        GeneralHeaders general;
        ResponseHeaders response;
        EntityHeaders entity;

        std::string toString() const;
    };

    bool isAcceptable(const AcceptList &list, const AcceptValueWithParameters &value, bool defaultMissing = false);
    bool isPreferred(const AcceptList &list, const AcceptValueWithParameters &lhs, const AcceptValueWithParameters &rhs);
    const AcceptValueWithParameters *preferred(const AcceptList &accept, const AcceptList &available);
};

std::ostream& operator<<(std::ostream& os, HTTP::Method m);
std::ostream& operator<<(std::ostream& os, HTTP::Status s);
std::ostream& operator<<(std::ostream& os, HTTP::Version v);
std::ostream& operator<<(std::ostream& os, const HTTP::ValueWithParameters &v);
std::ostream& operator<<(std::ostream& os, const HTTP::ParameterizedList &l);
std::ostream& operator<<(std::ostream& os, const HTTP::KeyValueWithParameters &v);
std::ostream& operator<<(std::ostream& os, const HTTP::ParameterizedKeyValueList &v);
std::ostream& operator<<(std::ostream& os, const HTTP::MediaType &m);
std::ostream& operator<<(std::ostream& os, const HTTP::ContentRange &m);
std::ostream& operator<<(std::ostream& os, const HTTP::AcceptValueWithParameters &v);
std::ostream& operator<<(std::ostream& os, const HTTP::AcceptList &l);
std::ostream& operator<<(std::ostream& os, const HTTP::RequestLine &r);
std::ostream& operator<<(std::ostream& os, const HTTP::StatusLine &s);
std::ostream& operator<<(std::ostream& os, const HTTP::GeneralHeaders &g);
std::ostream& operator<<(std::ostream& os, const HTTP::RequestHeaders &r);
std::ostream& operator<<(std::ostream& os, const HTTP::ResponseHeaders &r);
std::ostream& operator<<(std::ostream& os, const HTTP::EntityHeaders &e);
std::ostream& operator<<(std::ostream& os, const HTTP::Request &r);
std::ostream& operator<<(std::ostream& os, const HTTP::Response &r);

#endif
