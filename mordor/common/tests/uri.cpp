// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "mordor/common/uri.h"
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
    MORDOR_TEST_ASSERT_EQUAL(lhs.path.type, rhs.path.type);
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
}
