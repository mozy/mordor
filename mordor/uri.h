#ifndef __MORDOR_URI_H__
#define __MORDOR_URI_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>

#include "assert.h"
#include "mordor/streams/buffer.h"
#include "mordor/string.h"
#include "predef.h"

namespace Mordor {

class Stream;

namespace HTTP
{
    class RequestParser;
    class ResponseParser;
}

struct URI
{
    friend class URIParser;
    friend class HTTP::RequestParser;
    friend class HTTP::ResponseParser;

    enum CharacterClass {
        UNRESERVED,
        QUERYSTRING
    };

    static std::string encode(const std::string &str,
        CharacterClass charClass = UNRESERVED);
    static std::string decode(const std::string &str,
        CharacterClass charClass = UNRESERVED);

    URI();
    URI(const std::string& uri);
    URI(const char *uri);
    URI(const Buffer &uri);

    URI& operator=(const std::string& uri);
    URI& operator=(const char *uri) { return *this = std::string(uri); }
    URI& operator=(const Buffer &uri);

    void reset();

    std::string scheme() const { MORDOR_ASSERT(m_schemeDefined); return m_scheme; }
    void scheme(const std::string& s) { m_schemeDefined = true; m_scheme = s; }
    bool schemeDefined() const { return m_schemeDefined; }
    void schemeDefined(bool d) { if (!d) m_scheme.clear(); m_schemeDefined = d; }

    struct Authority
    {
        Authority();

        std::string userinfo() const { MORDOR_ASSERT(m_userinfoDefined); return m_userinfo; }
        void userinfo(const std::string& ui) { m_userinfoDefined = true; m_hostDefined = true; m_userinfo = ui; }
        bool userinfoDefined() const { return m_userinfoDefined; }
        void userinfoDefined(bool d) { if (!d) m_userinfo.clear(); if (d) m_hostDefined = true; m_userinfoDefined = d; }

        std::string host() const { MORDOR_ASSERT(m_hostDefined); return m_host; }
        void host(const std::string& h) { m_hostDefined = true; m_host = h; }
        bool hostDefined() const { return m_hostDefined; }
        void hostDefined(bool d) { if (!d) { m_host.clear(); userinfoDefined(false); portDefined(false); } m_hostDefined = d; }

        int port() const { MORDOR_ASSERT(m_portDefined); return m_port; }
        void port(int p) { m_portDefined = true; m_hostDefined = true; m_port = p; }
        bool portDefined() const { return m_portDefined; }
        void portDefined(bool d) { if (!d) m_port = -1; if (d) m_hostDefined = true; m_portDefined = d; }

        void normalize(const std::string& defaultHost = "", bool emptyHostValid = false,
            int defaultPort = -1, bool emptyPortValid = false);

        std::string toString() const;

        int cmp(const Authority &rhs) const;
        bool operator==(const Authority &rhs) const;
        bool operator!=(const Authority &rhs) const
        { return !(*this == rhs); }

    private:
        std::string m_userinfo, m_host;
        int m_port;
        bool m_userinfoDefined, m_hostDefined, m_portDefined;
    };
    Authority authority;

    struct Path
    {
        enum Type {
            ABSOLUTE,
            RELATIVE
        };

        Path();
        Path(const char *path);
        Path(const std::string& path);

        Path& operator=(const std::string& path);
        Path& operator=(const char *path) { return *this = std::string(path); }

        Type type;

        bool isEmpty() const { return type == RELATIVE && segments.empty(); }

        void removeDotComponents();
        void normalize(bool emptyPathValid = false);

        // Concatenate rhs to this object, dropping least significant component
        // of this object first
        void merge(const Path& rhs);

        std::vector<std::string> segments;

        struct path_serializer
        {
            const Path *p;
            bool schemeless;
        };
        path_serializer serialize(bool schemeless = false) const;

        std::string toString() const;

        int cmp(const Path &rhs) const;
        bool operator==(const Path &rhs) const;
        bool operator!=(const Path &rhs) const
        { return !(*this == rhs); }
    };
    Path path;

    class QueryString : public std::multimap<std::string, std::string, caseinsensitiveless>
    {
    public:
        QueryString() {}
        QueryString(const std::string &str)
        {
            *this = str;
        }

        std::string toString() const;

        QueryString &operator =(const std::string &string);
        QueryString &operator =(Stream &stream);
        QueryString &operator =(boost::shared_ptr<Stream> stream) { return *this = *stream; }
    };

    std::string query() const;
    QueryString queryString() const { MORDOR_ASSERT(m_queryDefined); return QueryString(m_query); }
    void query(const std::string &q);
    void query(const QueryString &q) { m_queryDefined = true; m_query = q.toString(); }
    bool queryDefined() const { return m_queryDefined; }
    void queryDefined(bool d) { if (!d) m_query.clear(); m_queryDefined = d; }

    std::string fragment() const { MORDOR_ASSERT(m_fragmentDefined); return m_fragment; }
    void fragment(const std::string& f) { m_fragmentDefined = true; m_fragment = f; }
    bool fragmentDefined() const { return m_fragmentDefined; }
    void fragmentDefined(bool d) { if (!d) m_fragment.clear(); m_fragmentDefined = d; }

    bool isDefined() const { return m_schemeDefined || authority.hostDefined() ||
        !path.isEmpty() || m_queryDefined || m_fragmentDefined; }

    std::string toString() const;

    void normalize();

    static URI transform(const URI& base, const URI& relative);

    int cmp(const URI &rhs) const;
    bool operator<(const URI &rhs) const;
    bool operator==(const URI &rhs) const;
    bool operator!=(const URI &rhs) const
    { return !(*this == rhs); }

private:
    std::string m_scheme, m_query, m_fragment;
    bool m_schemeDefined, m_queryDefined, m_fragmentDefined;
};

std::ostream& operator<<(std::ostream& os, const Mordor::URI::Authority& authority);
std::ostream& operator<<(std::ostream& os, const Mordor::URI::Path& path);
std::ostream& operator<<(std::ostream& os, const Mordor::URI::Path::path_serializer& path);
std::ostream& operator<<(std::ostream& os, const Mordor::URI& uri);

}

#endif
