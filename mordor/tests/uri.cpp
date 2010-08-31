// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/uri.h"
#include "mordor/streams/buffer.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

static void serializeAndParse(const char *uri, const char *expected = NULL)
{
    if (!expected) expected = uri;
    MORDOR_TEST_ASSERT_EQUAL(URI(uri).toString(), expected);
}

MORDOR_UNITTEST(URI, serializationAndParsing)
{
    MORDOR_TEST_ASSERT_EQUAL(URI::Path("/a/b/c/./../../g").toString(),
        "/a/b/c/./../../g");
    serializeAndParse("example://a/b/c/%7Bfoo%7D");
    serializeAndParse("eXAMPLE://a/./b/../b/%63/%7bfoo%7d", "eXAMPLE://a/./b/../b/c/%7Bfoo%7D");
    serializeAndParse("http://a/b/c/d;p?q");
    serializeAndParse("g:h");
    serializeAndParse("g");
    serializeAndParse("http://a/b/c/g");
    serializeAndParse("./g");
    serializeAndParse("g/");
    serializeAndParse("http://a/b/c/g/");
    serializeAndParse("/g");
    serializeAndParse("http://a/g");
    serializeAndParse("//g");
    serializeAndParse("http://g");
    serializeAndParse("?y");
    serializeAndParse("http://a/b/c/d;p?y");
    serializeAndParse("g?y");
    serializeAndParse("http://a/b/c/g?y");
    serializeAndParse("#s");
    serializeAndParse("http://a/b/c/d;p?q#s");
    serializeAndParse("g#s");
    serializeAndParse("http://a/b/c/g#s");
    serializeAndParse("g?y#s");
    serializeAndParse("http://a/b/c/g?y#s");
    serializeAndParse(";x");
    serializeAndParse("http://a/b/c/;x");
    serializeAndParse("g;x");
    serializeAndParse("http://a/b/c/g;x");
    serializeAndParse("g;x?y#s");
    serializeAndParse("http://a/b/c/g;x?y#s");
    serializeAndParse("");
    serializeAndParse("http://a/b/c/d;p?q");
    serializeAndParse(".");
    serializeAndParse("http://a/b/c/");
    serializeAndParse("./");
    serializeAndParse("..");
    serializeAndParse("http://a/b/");
    serializeAndParse("../");
    serializeAndParse("../g");
    serializeAndParse("http://a/b/g");
    serializeAndParse("../..");
    serializeAndParse("http://a/");
    serializeAndParse("../../");
    serializeAndParse("../../g");
    serializeAndParse("http://a/g");
    serializeAndParse("../../../g");
    serializeAndParse("../../../../g");
    serializeAndParse("/./g");
    serializeAndParse("/../g");
    serializeAndParse("g.");
    serializeAndParse("http://a/b/c/g.");
    serializeAndParse(".g");
    serializeAndParse("http://a/b/c/.g");
    serializeAndParse("g..");
    serializeAndParse("http://a/b/c/g..");
    serializeAndParse("..g");
    serializeAndParse("http://a/b/c/..g");
    serializeAndParse("./../g");
    serializeAndParse("./g/.");
    serializeAndParse("http://a/b/c/g/");
    serializeAndParse("g/./h");
    serializeAndParse("http://a/b/c/g/h");
    serializeAndParse("g/../h");
    serializeAndParse("http://a/b/c/h");
    serializeAndParse("g;x=1/./y");
    serializeAndParse("http://a/b/c/g;x=1/y");
    serializeAndParse("g;x=1/../y");
    serializeAndParse("http://a/b/c/y");
    serializeAndParse("g?y/./x");
    serializeAndParse("http://a/b/c/g?y/./x");
    serializeAndParse("g?y/../x");
    serializeAndParse("http://a/b/c/g?y/../x");
    serializeAndParse("g#s/./x");
    serializeAndParse("http://a/b/c/g#s/./x");
    serializeAndParse("g#s/../x");
    serializeAndParse("http://a/b/c/g#s/../x");
    serializeAndParse("http:g");
    serializeAndParse("http:/hi");
    serializeAndParse("http:////hi");
}

MORDOR_UNITTEST(URI, pathNormalization)
{
    URI::Path p("/a/b/c/./../../g");
    p.removeDotComponents();
    MORDOR_TEST_ASSERT_EQUAL(p, URI::Path("/a/g"));
}

MORDOR_UNITTEST(URI, normalization)
{
    URI lhs("example://a/b/c/%7Bfoo%7D");
    URI rhs("eXAMPLE://a/./b/../b/%63/%7bfoo%7d");

    lhs.normalize();
    rhs.normalize();

    MORDOR_TEST_ASSERT(lhs.isDefined());
    MORDOR_TEST_ASSERT(rhs.isDefined());
    MORDOR_TEST_ASSERT(lhs.schemeDefined());
    MORDOR_TEST_ASSERT(rhs.schemeDefined());
    MORDOR_TEST_ASSERT_EQUAL(lhs.scheme(), rhs.scheme());
    MORDOR_TEST_ASSERT(!lhs.authority.portDefined());
    MORDOR_TEST_ASSERT(!rhs.authority.portDefined());
    MORDOR_TEST_ASSERT(lhs.authority.hostDefined());
    MORDOR_TEST_ASSERT(rhs.authority.hostDefined());
    MORDOR_TEST_ASSERT_EQUAL(lhs.authority.host(), rhs.authority.host());
    MORDOR_TEST_ASSERT(!lhs.authority.userinfoDefined());
    MORDOR_TEST_ASSERT(!rhs.authority.userinfoDefined());
    MORDOR_TEST_ASSERT_EQUAL(lhs.authority, rhs.authority);
    MORDOR_TEST_ASSERT_EQUAL(lhs.path.segments, rhs.path.segments);
    MORDOR_TEST_ASSERT_EQUAL(lhs.path, rhs.path);
    MORDOR_TEST_ASSERT(!lhs.queryDefined());
    MORDOR_TEST_ASSERT(!rhs.queryDefined());
    MORDOR_TEST_ASSERT(!lhs.fragmentDefined());
    MORDOR_TEST_ASSERT(!rhs.fragmentDefined());
    MORDOR_TEST_ASSERT_EQUAL(lhs, rhs);
}

MORDOR_UNITTEST(URI, transform)
{
    URI base("http://a/b/c/d;p?q");
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("g:h")), URI("g:h"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("g")), URI("http://a/b/c/g"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("./g")), URI("http://a/b/c/g"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("g/")), URI("http://a/b/c/g/"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("/g")), URI("http://a/g"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("//g")), URI("http://g"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("?y")), URI("http://a/b/c/d;p?y"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("g?y")), URI("http://a/b/c/g?y"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("#s")), URI("http://a/b/c/d;p?q#s"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("g#s")), URI("http://a/b/c/g#s"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("g?y#s")), URI("http://a/b/c/g?y#s"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI(";x")), URI("http://a/b/c/;x"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("g;x")), URI("http://a/b/c/g;x"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("g;x?y#s")), URI("http://a/b/c/g;x?y#s"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("")), URI("http://a/b/c/d;p?q"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI(".")), URI("http://a/b/c/"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("./")), URI("http://a/b/c/"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("..")), URI("http://a/b/"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("../")), URI("http://a/b/"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("../g")), URI("http://a/b/g"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("../..")), URI("http://a/"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("../../")), URI("http://a/"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("../../g")), URI("http://a/g"));

    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("../../../g")), URI("http://a/g"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("../../../../g")), URI("http://a/g"));

    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("/./g")), URI("http://a/g"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("/../g")), URI("http://a/g"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("g.")), URI("http://a/b/c/g."));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI(".g")), URI("http://a/b/c/.g"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("g..")), URI("http://a/b/c/g.."));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("..g")), URI("http://a/b/c/..g"));

    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("./../g")), URI("http://a/b/g"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("./g/.")), URI("http://a/b/c/g/"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("g/./h")), URI("http://a/b/c/g/h"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("g/../h")), URI("http://a/b/c/h"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("g;x=1/./y")), URI("http://a/b/c/g;x=1/y"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("g;x=1/../y")), URI("http://a/b/c/y"));

    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("g?y/./x")), URI("http://a/b/c/g?y/./x"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("g?y/../x")), URI("http://a/b/c/g?y/../x"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("g#s/./x")), URI("http://a/b/c/g#s/./x"));
    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("g#s/../x")), URI("http://a/b/c/g#s/../x"));

    MORDOR_TEST_ASSERT_EQUAL(URI::transform(base, URI("http:g")), URI("http:g"));

    MORDOR_TEST_ASSERT_EQUAL(URI::transform("http:hi", "bob"), "http:bob");
    MORDOR_TEST_ASSERT_EQUAL(URI::transform("http://authority", "bob"), "http://authority/bob");
}

MORDOR_UNITTEST(URI, serializeCompleteOnBlockBoundary)
{
    Buffer b("http://abc/");
    b.copyIn("more");
    URI uri(b);
    MORDOR_TEST_ASSERT_EQUAL(URI(b), "http://abc/more");
}

MORDOR_UNITTEST(URI, bigBase64URI)
{
    serializeAndParse("/partialObjects/"
        "49ZtbkNPlEEi8T+sQLb5mh9zm1DcyaaRoyHUOC9sEfaKIgLh+eKZNUrqR+j3Iybhx321iz"
        "y3J+Mw7gZmIlVcZrP0qHNDxuEQHMUHLqxhXoXcN18+x4XedNLqc8KhnJtHLXndcKMJu5Cg"
        "xp2BI9NXDDDuBmYiVVxms/Soc0PG4RAcxQcurGFehSY0Wf0fG5eWquA0b0hozVjE4xxyAF"
        "TkSU39Hl3XcsUUMO4GZiJVXGaz9KhzQ8bhEBzFBy6sYV6F9718Fox0OiJ3PqBvo2gr352W"
        "vZBqmEeUV1n0CkcClc0w7gZmIlVcZrP0qHNDxuEQHMUHLqxhXoWapmDUfha0WO9SjTUn4F"
        "Jeht8Gjdy6mYpDqvUbB+3OoDDuBmYiVVxms/Soc0PG4RAcxQcurGFehcefjKkVeAR2HShU"
        "2UpBh5g/89ZP9czSJ8qKSKCPGyHWMO4GZiJVXGaz9KhzQ8bhEBzFBy6sYV6FAig0fJADqV"
        "eInu5RU/pgEXJlZ1MBce/F+rv7MI3g5jgw7gZmIlVcZrP0qHNDxuEQHMUHLqxhXoW4GIxe"
        "C1lnhkTtrAv3jhk17r3ZwL8Fq7CvpUHeAQl/JTDuBmYiVVxms/Soc0PG4RAcxQcurGFehc"
        "s4fMw9uBwTihHQAPFbcyDTjZtTMGlaovGaP6xe1H1TMO4GZiJVXGaz9KhzQ8bhEBzFBy6s"
        "YV6FFAhiH0dzP8E0IRZP+oxeL2JkfxiO5v8r7eWnYtMY8d4w7gZmIlVcZrP0qHNDxuEQHM"
        "UHLqxhXoUgoQ1pQreM2tYMR9QaJ7CsSOSJs+Qi5KIzV50DBUYLDjDuBmYiVVxms/Soc0PG"
        "4RAcxQcurGFehdeUg8nHldHqihIknc3OP/QRtBawAyEFY4p0RKlRxnA0MO4GZiJVXGaz9K"
        "hzQ8bhEBzFBy6sYV6FbRY5v48No3N72yRSA9JiYPhS/YTYcUFz");
}

MORDOR_UNITTEST(URI, queryString)
{
    URI uri("http://a/b?a&b");
    MORDOR_TEST_ASSERT(uri.queryDefined());
    MORDOR_TEST_ASSERT_EQUAL(uri.query(), "a&b");
    uri = "http://a/b?a%20b";
    MORDOR_TEST_ASSERT(uri.queryDefined());
    MORDOR_TEST_ASSERT_EQUAL(uri.query(), "a b");
    URI::QueryString qs = uri.queryString();

    MORDOR_TEST_ASSERT_EQUAL(qs.size(), 1u);
    MORDOR_TEST_ASSERT_EQUAL(qs.begin()->first, "a b");
    MORDOR_TEST_ASSERT_EQUAL(qs.toString(), "a+b");

    qs = "a%2Bb";
    MORDOR_TEST_ASSERT_EQUAL(qs.size(), 1u);
    MORDOR_TEST_ASSERT_EQUAL(qs.begin()->first, "a+b");
    MORDOR_TEST_ASSERT_EQUAL(qs.toString(), "a%2Bb");

    qs = "a&b;c";
    MORDOR_TEST_ASSERT_EQUAL(qs.size(), 3u);
    URI::QueryString::iterator it = qs.begin();
    MORDOR_TEST_ASSERT_EQUAL(it->first, "a");
    ++it;
    MORDOR_TEST_ASSERT_EQUAL(it->first, "b");
    ++it;
    MORDOR_TEST_ASSERT_EQUAL(it->first, "c");

    qs = "a+b";
    MORDOR_TEST_ASSERT_EQUAL(qs.size(), 1u);
    MORDOR_TEST_ASSERT_EQUAL(qs.begin()->first, "a b");
    MORDOR_TEST_ASSERT_EQUAL(qs.toString(), "a+b");

    qs = "";
    MORDOR_TEST_ASSERT(qs.empty());
    MORDOR_TEST_ASSERT_EQUAL(qs.toString(), "");

    qs = "&&;&&;;&";
    MORDOR_TEST_ASSERT(qs.empty());
    MORDOR_TEST_ASSERT_EQUAL(qs.toString(), "");

    qs = "a=b&c";
    MORDOR_TEST_ASSERT_EQUAL(qs.size(), 2u);
    it = qs.begin();
    MORDOR_TEST_ASSERT_EQUAL(it->first, "a");
    MORDOR_TEST_ASSERT_EQUAL(it->second, "b");
    ++it;
    MORDOR_TEST_ASSERT_EQUAL(it->first, "c");

    qs = "a=b=c=d&e=f=g=h";
    MORDOR_TEST_ASSERT_EQUAL(qs.size(), 2u);
    it = qs.begin();
    MORDOR_TEST_ASSERT_EQUAL(it->first, "a");
    MORDOR_TEST_ASSERT_EQUAL(it->second, "b=c=d");
    ++it;
    MORDOR_TEST_ASSERT_EQUAL(it->first, "e");
    MORDOR_TEST_ASSERT_EQUAL(it->second, "f=g=h");

    qs = "a=";
    MORDOR_TEST_ASSERT_EQUAL(qs.size(), 1u);
    it = qs.begin();
    MORDOR_TEST_ASSERT_EQUAL(it->first, "a");
    MORDOR_TEST_ASSERT(it->second.empty());

    qs = "a=&=b&=";
    MORDOR_TEST_ASSERT_EQUAL(qs.size(), 2u);
    it = qs.begin();
    MORDOR_TEST_ASSERT(it->first.empty());
    MORDOR_TEST_ASSERT_EQUAL(it->second, "b");
    ++it;
    MORDOR_TEST_ASSERT_EQUAL(it->first, "a");
    MORDOR_TEST_ASSERT(it->second.empty());

    qs.clear();
    qs.insert(std::make_pair("ampersand", "and&and"));
    MORDOR_TEST_ASSERT_EQUAL(qs.toString(), "ampersand=and%26and");

    uri = URI();
    uri.query(qs);
    MORDOR_TEST_ASSERT_EQUAL(uri.toString(), "?ampersand=and%26and");
}

MORDOR_UNITTEST(URI, encoding)
{
    URI uri;
    uri.path.segments.push_back(std::string());
    uri.path.segments.push_back("WiX Tutorial \xe2\x80\x94 Introduction to the Windows Installer XML Toolset.URL");
    MORDOR_TEST_ASSERT_EQUAL(uri.toString(),
        "/WiX%20Tutorial%20%E2%80%94%20Introduction%20to%20the%20Windows%20Installer%20XML%20Toolset.URL");

    uri.path.segments[1] = "\xe5\xa4\x9a\xe8\xa8\x80\xe8\xaa\x9e\xe5\xaf\xbe\xe5\xbf\x9c\xe3\x82"
        "\xb5\xe3\x83\xbc\xe3\x83\x81\xe3\x82\xa8\xe3\x83\xb3\xe3\x82\xb8\xe3"
        "\x83\xb3\xe3\x81\xae\xe6\x97\xa5\xe6\x9c\xac\xe7\x89\x88\xe3\x80\x82"
        "\xe3\x82\xa6\xe3\x82\xa7\xe3\x83\x96\xe3\x80\x81\xe3\x82\xa4\xe3\x83"
        "\xa1\xe3\x83\xbc\xe3\x82\xb8\xe3\x81\x8a\xe3\x82\x88\xe3\x81\xb3\xe3"
        "\x83\x8b\xe3\x83\xa5\xe3\x83\xbc\xe3\x82\xb9\xe6\xa4\x9c\xe7\xb4\xa2.txt";
    MORDOR_TEST_ASSERT_EQUAL(uri.toString(),
        "/%E5%A4%9A%E8%A8%80%E8%AA%9E%E5%AF%BE%E5%BF%9C%E3%82"
        "%B5%E3%83%BC%E3%83%81%E3%82%A8%E3%83%B3%E3%82%B8%E3"
        "%83%B3%E3%81%AE%E6%97%A5%E6%9C%AC%E7%89%88%E3%80%82"
        "%E3%82%A6%E3%82%A7%E3%83%96%E3%80%81%E3%82%A4%E3%83"
        "%A1%E3%83%BC%E3%82%B8%E3%81%8A%E3%82%88%E3%81%B3%E3"
        "%83%8B%E3%83%A5%E3%83%BC%E3%82%B9%E6%A4%9C%E7%B4%A2.txt");
}

MORDOR_UNITTEST(URI, emptyFirstComponent)
{
    serializeAndParse("/");
    serializeAndParse("http://localhost/");
    serializeAndParse("http://localhost//");
    serializeAndParse("http://localhost///");

    URI::Path path;
    path = "/";
    MORDOR_TEST_ASSERT_EQUAL(path.segments.size(), 2u);
    MORDOR_TEST_ASSERT(path.isAbsolute());
    MORDOR_TEST_ASSERT(path.segments.front().empty());
    MORDOR_TEST_ASSERT(path.segments.back().empty());
    path = "";
    MORDOR_TEST_ASSERT(path.isRelative());
    MORDOR_TEST_ASSERT(path.segments.empty());
    path = "//";
    MORDOR_TEST_ASSERT(path.isAbsolute());
    MORDOR_TEST_ASSERT_EQUAL(path.segments.size(), 3u);
    path = "a//";
    MORDOR_TEST_ASSERT(path.isRelative());
    MORDOR_TEST_ASSERT_EQUAL(path.segments.size(), 3u);

    URI uri = "http://localhost/";
    MORDOR_TEST_ASSERT_EQUAL(uri.toString(), "http://localhost/");
    uri.path.append("hi");
    MORDOR_TEST_ASSERT_EQUAL(uri.toString(), "http://localhost/hi");
    uri.path.segments.push_back("");
    MORDOR_TEST_ASSERT_EQUAL(uri.toString(), "http://localhost/hi/");
    uri.path.segments.push_back("bye");
    MORDOR_TEST_ASSERT_EQUAL(uri.toString(), "http://localhost/hi//bye");
    uri.path.segments.push_back("");
    MORDOR_TEST_ASSERT_EQUAL(uri.toString(), "http://localhost/hi//bye/");
    uri.path.append("adios");
    MORDOR_TEST_ASSERT_EQUAL(uri.toString(), "http://localhost/hi//bye/adios");
    uri = "http://localhost/";
    uri.path.append("hi");
    MORDOR_TEST_ASSERT_EQUAL(uri.toString(), "http://localhost/hi");

    // scheme == http, authority is not defined, path = "//hi";
    // serialization has to add an extra // so it's not ambiguous with
    // authority
    uri = URI();
    uri.scheme("http");
    uri.path.segments.push_back(std::string());
    uri.path.segments.push_back(std::string());
    uri.path.segments.push_back("hi");
    MORDOR_TEST_ASSERT_EQUAL(uri.toString(), "http:////hi");
}

MORDOR_UNITTEST(URI, append)
{
    URI uri = "http://localhost";
    uri.path.append("hi");
    MORDOR_TEST_ASSERT_EQUAL(uri, "http://localhost/hi");
    uri.path.append("bye");
    MORDOR_TEST_ASSERT_EQUAL(uri, "http://localhost/hi/bye");
    uri.path.append(std::string());
    MORDOR_TEST_ASSERT_EQUAL(uri, "http://localhost/hi/bye/");
    uri.path.append(std::string());
    MORDOR_TEST_ASSERT_EQUAL(uri, "http://localhost/hi/bye/");

    uri = "http:";
    uri.path.append("hi");
    MORDOR_TEST_ASSERT_EQUAL(uri, "http:hi");
    uri.path.append("bye");
    MORDOR_TEST_ASSERT_EQUAL(uri, "http:hi/bye");

    URI::Path path;
    path.append("hi");
    MORDOR_TEST_ASSERT_EQUAL(path, "hi");
    path.append("bye");
    MORDOR_TEST_ASSERT_EQUAL(path, "hi/bye");

    // The following four tests ensure that path's hidden pointer back to the
    // URI owning it is copied/assigned correctly through URI and Path's
    // copy constructors and assignement operators

    // Force operator=(const URI &uri)
    URI uri2 = "http://localhost";
    uri = uri2;
    uri2.authority.hostDefined(false);
    uri.path.append("hi");
    MORDOR_TEST_ASSERT_EQUAL(uri, "http://localhost/hi");

    // Force URI::URI(const URI &uri)
    uri2.authority.host("localhost");
    URI uri3(uri2);
    uri2.authority.hostDefined(false);
    uri3.path.append("hi");
    MORDOR_TEST_ASSERT_EQUAL(uri3, "http://localhost/hi");

    // Force operator=(const Path &path)
    path = uri3.path;
    path.segments.clear();
    path.append("hi");
    MORDOR_TEST_ASSERT_EQUAL(path, "hi");

    // Force Path::Path(const Path &path)
    URI::Path path2(uri3.path);
    path2.segments.clear();
    path2.append("hi");
    MORDOR_TEST_ASSERT_EQUAL(path2, "hi");
}

MORDOR_UNITTEST(URI, makeAbsolute)
{
    URI::Path path;
    path.makeAbsolute();
    MORDOR_TEST_ASSERT_EQUAL(path, "/");
    path.makeAbsolute();
    MORDOR_TEST_ASSERT_EQUAL(path, "/");
    path.makeRelative();
    MORDOR_TEST_ASSERT(path.segments.empty());
    path.makeRelative();
    MORDOR_TEST_ASSERT(path.segments.empty());
    path.append("hi");
    MORDOR_TEST_ASSERT_EQUAL(path, "hi");
    path.makeAbsolute();
    MORDOR_TEST_ASSERT_EQUAL(path, "/hi");
    path.makeAbsolute();
    MORDOR_TEST_ASSERT_EQUAL(path, "/hi");
    path.makeRelative();
    MORDOR_TEST_ASSERT_EQUAL(path, "hi");
    path.makeRelative();
    MORDOR_TEST_ASSERT_EQUAL(path, "hi");
}
