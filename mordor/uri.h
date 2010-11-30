#ifndef __MORDOR_URI_H__
#define __MORDOR_URI_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>

#include "assert.h"
#include "mordor/string.h"

namespace Mordor {

struct Buffer;
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
    friend std::ostream& operator<<(std::ostream& os, const Mordor::URI& uri);

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
    URI(const URI &uri);

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
        Authority(const char *path);
        Authority(const std::string& path);

        Authority& operator=(const std::string& authority);
        Authority& operator=(const char *authority) { return *this = std::string(authority); }

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

    /// Represents segments in the path
    ///
    /// A single, empty segment is invalid.  A leading empty segment indicates
    /// an absolute path; a trailing empty segment indicates a trailing slash.
    struct Path
    {
        friend struct URI;
    private:
        Path(const URI &uri);
        Path(const URI &uri, const Path &path);
    public:
        Path();
        Path(const char *path);
        Path(const std::string& path);
        Path(const Path &path);

        Path &operator=(const std::string &path);
        Path &operator=(const char *path) { return *this = std::string(path); }
        Path &operator=(const Path &path);

        bool isEmpty() const
        {
            return segments.empty() ||
                (segments.size() == 1 && segments.front().empty());
        }
        bool isAbsolute() const
        {
            return segments.size() > 1 && segments.front().empty();
        }
        bool isRelative() const
        {
            return !isAbsolute();
        }

        void makeAbsolute();
        void makeRelative();

        /// Append a single segment
        ///
        /// This will remove a trailing empty segment before appending, if
        /// necessary
        /// I.e., Path("/hi/").append("bob") would result in "/hi/bob" instead
        /// of "/hi//bob"
        /// Also, if this path is part of a URI, and the URI has an authority
        /// defined, and the path is empty, append will ensure the path becomes
        /// absolute.
        /// I.e., URI("http://localhost").path.append("bob") would result in
        /// "http://localhost/bob"
        void append(const std::string &segment);
        void removeDotComponents();
        void normalize(bool emptyPathValid = false);

        /// Concatenate rhs to this object, dropping least significant
        /// component of this object first
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

    private:
        const URI *m_uri;
    };
    Path path;

    class QueryString : public std::multimap<std::string, std::string, caseinsensitiveless>
    {
    public:
        QueryString() {}
        QueryString(const std::string &str) { *this = str; }
        QueryString(Stream &stream) { *this = stream; }
        QueryString(boost::shared_ptr<Stream> stream) { *this = *stream; }

        std::string toString() const;

        QueryString &operator =(const std::string &string);
        QueryString &operator =(Stream &stream);
        QueryString &operator =(boost::shared_ptr<Stream> stream) { return *this = *stream; }

        /// Convenience function for working with a non-multi-valued key
        ///
        /// This function will return a refence to the value corresponding to
        /// key.  If key does not yet exist, it is created.  If multiple values
        /// exist, all but the first are erased.
        std::string &operator[](const std::string &key);
        /// Convenience function for working with a non-multi-valued key
        ///
        /// This function will return a copy of the first value corresponding
        /// to key.  If key does not exist, an empy string is returned.
        std::string operator[](const std::string &key) const;
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
