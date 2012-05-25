// Copyright (c) 2012 - Mozy, Inc.

#include "mordor/http/proxy.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::HTTP;
using namespace Mordor::Test;

static const char proxy[] = "http=http-proxy.example.com:8080";
static const URI proxyURI("http://http-proxy.example.com:8080");

MORDOR_UNITTEST(HTTPProxy, proxyFromList_bypassMatch)
{
    // Positive tests. The bypass list should match in each of the following
    // cases.
    std::vector<URI> v;
    v = proxyFromList(URI("http://www.google.com"), proxy, "*");
    MORDOR_TEST_ASSERT_EQUAL(v.size(), 0U);

    v = proxyFromList(URI("http://www.google.com"), proxy, "*.google.com");
    MORDOR_TEST_ASSERT_EQUAL(v.size(), 0U);

    v = proxyFromList(URI("http://www.google.com"), proxy, "www.*.com");
    MORDOR_TEST_ASSERT_EQUAL(v.size(), 0U);

    v = proxyFromList(URI("http://128.196.1.100"), proxy, "128.196.*");
    MORDOR_TEST_ASSERT_EQUAL(v.size(), 0U);

    v = proxyFromList(URI("http://www.google.com"), proxy, "*.google.com");
    MORDOR_TEST_ASSERT_EQUAL(v.size(), 0U);

    v = proxyFromList(URI("http://www.google.com:80"), proxy, "http://*.google.com");
    MORDOR_TEST_ASSERT_EQUAL(v.size(), 0U);

    v = proxyFromList(URI("http://www.google.com:80"), proxy, "*.google.com:80");
    MORDOR_TEST_ASSERT_EQUAL(v.size(), 0U);

    v = proxyFromList(URI("http://www.google.com:80"), proxy, "http://*.google.com:80");
    MORDOR_TEST_ASSERT_EQUAL(v.size(), 0U);

    // Test a bypass list with multiple entries; the second entry matches our
    // URI, so this URI should not use the proxy.
    v = proxyFromList(URI("http://www.google.com"), proxy, "*.example.com;*.google.com");
    MORDOR_TEST_ASSERT_EQUAL(v.size(), 0U);
}

MORDOR_UNITTEST(HTTPProxy, proxyFromList_noBypassMatch)
{
    // Negative tests. Each of the following URLs should use the proxy.
    std::vector<URI> v;
    v = proxyFromList(URI("http://www.google.com"), proxy, "");
    MORDOR_TEST_ASSERT_NOT_EQUAL(v.size(), 0U);
    MORDOR_TEST_ASSERT_EQUAL(v[0], proxyURI);

    v = proxyFromList(URI("http://www.google.com"), proxy, "*.example.com");
    MORDOR_TEST_ASSERT_NOT_EQUAL(v.size(), 0U);
    MORDOR_TEST_ASSERT_EQUAL(v[0], proxyURI);

    v = proxyFromList(URI("http://mail.google.com"), proxy, "www.*.com");
    MORDOR_TEST_ASSERT_NOT_EQUAL(v.size(), 0U);
    MORDOR_TEST_ASSERT_EQUAL(v[0], proxyURI);

    v = proxyFromList(URI("http://www.google.org"), proxy, "www.*.com");
    MORDOR_TEST_ASSERT_NOT_EQUAL(v.size(), 0U);
    MORDOR_TEST_ASSERT_EQUAL(v[0], proxyURI);

    v = proxyFromList(URI("http://192.168.1.100"), proxy, "10.10.*");
    MORDOR_TEST_ASSERT_NOT_EQUAL(v.size(), 0U);
    MORDOR_TEST_ASSERT_EQUAL(v[0], proxyURI);

    v = proxyFromList(URI("http://www.google.com"), proxy, "https://*.google.com");
    MORDOR_TEST_ASSERT_NOT_EQUAL(v.size(), 0U);
    MORDOR_TEST_ASSERT_EQUAL(v[0], proxyURI);

    v = proxyFromList(URI("http://www.google.com:80"), proxy, "*.google.com:8080");
    MORDOR_TEST_ASSERT_NOT_EQUAL(v.size(), 0U);
    MORDOR_TEST_ASSERT_EQUAL(v[0], proxyURI);

    v = proxyFromList(URI("http://www.google.com:80"), proxy, "https://*.google.com:8080");
    MORDOR_TEST_ASSERT_NOT_EQUAL(v.size(), 0U);
    MORDOR_TEST_ASSERT_EQUAL(v[0], proxyURI);

    // Test a bypass list with multiple entries; none of these entries matches
    // our URI, so this URI should use the proxy.
    v = proxyFromList(URI("http://www.google.com"), proxy, "*.example.com;192.168.*");
    MORDOR_TEST_ASSERT_NOT_EQUAL(v.size(), 0U);
    MORDOR_TEST_ASSERT_EQUAL(v[0], proxyURI);
}

MORDOR_UNITTEST(HTTPProxy, proxyFromList_multipleProxies)
{
    // Test a proxy list with multiple proxies.
    //
    // This proxy list on has one http proxy, but it has proxies for other URI
    // schemes.
    std::vector<URI> v;
    v = proxyFromList(URI("http://www.google.com"),
                      "http=http-proxy.example.com:8080;" \
                      "ftp=ftp-proxy.example.com:2121;"   \
                      "https=https-proxy.example.com:4443",
                      "");
    MORDOR_TEST_ASSERT_EQUAL(v.size(), 1U);
    MORDOR_TEST_ASSERT_EQUAL(v[0], URI("http://http-proxy.example.com:8080"));

    // This proxy list has multiple proxies for the http scheme.
    v = proxyFromList(URI("http://www.google.com"),
                      "http=http-proxy-1.example.com:8080;" \
                      "http=http-proxy-2.example.com:8080;" \
                      "ftp=ftp-proxy.example.com:2121;"     \
                      "https=https-proxy.example.com:4443",
                      "");
    MORDOR_TEST_ASSERT_EQUAL(v.size(), 2U);
    MORDOR_TEST_ASSERT_EQUAL(v[0], URI("http://http-proxy-1.example.com:8080"));
    MORDOR_TEST_ASSERT_EQUAL(v[1], URI("http://http-proxy-2.example.com:8080"));
}

MORDOR_UNITTEST(HTTPProxy, proxyFromList_splitProxyString)
{
    // Test splitting the proxy string.
    static const char proxyAndBypass[] = "http=http-proxy.example.com:8080!*.example.com";
    std::vector<URI> v;
    v = proxyFromList(URI("http://www.example.com"), proxyAndBypass, "");
    MORDOR_TEST_ASSERT_EQUAL(v.size(), 0U);

    v = proxyFromList(URI("http://www.google.com"), proxyAndBypass, "");
    MORDOR_TEST_ASSERT_NOT_EQUAL(v.size(), 0U);
    MORDOR_TEST_ASSERT_EQUAL(v[0], proxyURI);

    // Test that an explicit bypass list argument takes precedence over the
    // '!' notation.
    v = proxyFromList(URI("http://www.google.com"), proxyAndBypass, "*.google.com");
    MORDOR_TEST_ASSERT_EQUAL(v.size(), 0U);
}
