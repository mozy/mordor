// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "mordor/common/uri.h"
#include "mordor/test/test.h"

static void serializeAndParse(const char *uri, const char *expected = NULL)
{
    if (!expected) expected = uri;
    TEST_ASSERT_EQUAL(URI(uri).toString(), expected);
}

TEST_WITH_SUITE(URI, serializationAndParsing)
{
    TEST_ASSERT_EQUAL(URI::Path("/a/b/c/./../../g").toString(),
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
}

TEST_WITH_SUITE(URI, pathNormalization)
{
    URI::Path p("/a/b/c/./../../g");
    p.removeDotComponents();
    TEST_ASSERT_EQUAL(p, URI::Path("/a/g"));
}

TEST_WITH_SUITE(URI, normalization)
{
    URI lhs("example://a/b/c/%7Bfoo%7D");
    URI rhs("eXAMPLE://a/./b/../b/%63/%7bfoo%7d");

    lhs.normalize();
    rhs.normalize();

    TEST_ASSERT(lhs.isDefined());
    TEST_ASSERT(rhs.isDefined());
    TEST_ASSERT(lhs.schemeDefined());
    TEST_ASSERT(rhs.schemeDefined());
    TEST_ASSERT_EQUAL(lhs.scheme(), rhs.scheme());
    TEST_ASSERT(!lhs.authority.portDefined());
    TEST_ASSERT(!rhs.authority.portDefined());
    TEST_ASSERT(lhs.authority.hostDefined());
    TEST_ASSERT(rhs.authority.hostDefined());
    TEST_ASSERT_EQUAL(lhs.authority.host(), rhs.authority.host());
    TEST_ASSERT(!lhs.authority.userinfoDefined());
    TEST_ASSERT(!rhs.authority.userinfoDefined());
    TEST_ASSERT_EQUAL(lhs.authority, rhs.authority);
    TEST_ASSERT_EQUAL(lhs.path.type, rhs.path.type);
    TEST_ASSERT_EQUAL(lhs.path.segments, rhs.path.segments);
    TEST_ASSERT_EQUAL(lhs.path, rhs.path);
    TEST_ASSERT(!lhs.queryDefined());
    TEST_ASSERT(!rhs.queryDefined());
    TEST_ASSERT(!lhs.fragmentDefined());
    TEST_ASSERT(!rhs.fragmentDefined());
    TEST_ASSERT_EQUAL(lhs, rhs);
}

TEST_WITH_SUITE(URI, transform)
{
    URI base("http://a/b/c/d;p?q");
    TEST_ASSERT_EQUAL(URI::transform(base, URI("g:h")), URI("g:h"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("g")), URI("http://a/b/c/g"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("./g")), URI("http://a/b/c/g"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("g/")), URI("http://a/b/c/g/"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("/g")), URI("http://a/g"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("//g")), URI("http://g"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("?y")), URI("http://a/b/c/d;p?y"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("g?y")), URI("http://a/b/c/g?y"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("#s")), URI("http://a/b/c/d;p?q#s"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("g#s")), URI("http://a/b/c/g#s"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("g?y#s")), URI("http://a/b/c/g?y#s"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI(";x")), URI("http://a/b/c/;x"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("g;x")), URI("http://a/b/c/g;x"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("g;x?y#s")), URI("http://a/b/c/g;x?y#s"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("")), URI("http://a/b/c/d;p?q"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI(".")), URI("http://a/b/c/"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("./")), URI("http://a/b/c/"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("..")), URI("http://a/b/"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("../")), URI("http://a/b/"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("../g")), URI("http://a/b/g"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("../..")), URI("http://a/"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("../../")), URI("http://a/"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("../../g")), URI("http://a/g"));
    
    TEST_ASSERT_EQUAL(URI::transform(base, URI("../../../g")), URI("http://a/g"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("../../../../g")), URI("http://a/g"));
    
    TEST_ASSERT_EQUAL(URI::transform(base, URI("/./g")), URI("http://a/g"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("/../g")), URI("http://a/g"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("g.")), URI("http://a/b/c/g."));
    TEST_ASSERT_EQUAL(URI::transform(base, URI(".g")), URI("http://a/b/c/.g"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("g..")), URI("http://a/b/c/g.."));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("..g")), URI("http://a/b/c/..g"));
    
    TEST_ASSERT_EQUAL(URI::transform(base, URI("./../g")), URI("http://a/b/g"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("./g/.")), URI("http://a/b/c/g/"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("g/./h")), URI("http://a/b/c/g/h"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("g/../h")), URI("http://a/b/c/h"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("g;x=1/./y")), URI("http://a/b/c/g;x=1/y"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("g;x=1/../y")), URI("http://a/b/c/y"));
    
    TEST_ASSERT_EQUAL(URI::transform(base, URI("g?y/./x")), URI("http://a/b/c/g?y/./x"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("g?y/../x")), URI("http://a/b/c/g?y/../x"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("g#s/./x")), URI("http://a/b/c/g#s/./x"));
    TEST_ASSERT_EQUAL(URI::transform(base, URI("g#s/../x")), URI("http://a/b/c/g#s/../x"));
    
    TEST_ASSERT_EQUAL(URI::transform(base, URI("http:g")), URI("http:g"));
}

TEST_WITH_SUITE(URI, serializeCompleteOnBlockBoundary)
{
    Buffer b("http://abc/");
    b.copyIn("more");
    URI uri(b);
    TEST_ASSERT_EQUAL(URI(b), "http://abc/more");
}

TEST_WITH_SUITE(URI, bigBase64URI)
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

TEST_WITH_SUITE(URI, queryString)
{
    URI uri("http://a/b?a&b");
    TEST_ASSERT(uri.queryDefined());
    TEST_ASSERT_EQUAL(uri.query(), "a&b");
    uri = "http://a/b?a%20b";
    TEST_ASSERT(uri.queryDefined());
    TEST_ASSERT_EQUAL(uri.query(), "a b");
    URI::QueryString qs = uri.queryString();

    TEST_ASSERT_EQUAL(qs.size(), 1u);
    TEST_ASSERT_EQUAL(qs.begin()->first, "a b");
    TEST_ASSERT_EQUAL(qs.toString(), "a+b");

    qs = "a&b;c";
    TEST_ASSERT_EQUAL(qs.size(), 3u);
    URI::QueryString::iterator it = qs.begin();
    TEST_ASSERT_EQUAL(it->first, "a");
    ++it;
    TEST_ASSERT_EQUAL(it->first, "b");
    ++it;
    TEST_ASSERT_EQUAL(it->first, "c");

    qs = "a+b";
    TEST_ASSERT_EQUAL(qs.size(), 1u);
    TEST_ASSERT_EQUAL(qs.begin()->first, "a b");
    TEST_ASSERT_EQUAL(qs.toString(), "a+b");
}
