// Copyright (c) 2010 - Mozy, Inc.

#include "mordor/http/servlet.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::HTTP;

namespace {
class DummyServlet : public Servlet
{
public:
    void request(boost::shared_ptr<ServerRequest> request) {}
};
}

MORDOR_UNITTEST(ServletDispatcher, basic)
{
    ServletDispatcher dispatcher;
    Servlet::ptr root(new DummyServlet), ab(new DummyServlet),
        ca(new DummyServlet), ma(new DummyServlet), wl(new DummyServlet);

    MORDOR_TEST_ASSERT(!dispatcher.getServlet("/d/e/f"));

    dispatcher.registerServlet("/", root);
    dispatcher.registerServlet("/a/b", ab);
    dispatcher.registerServlet("/c/a", ca);
    dispatcher.registerServlet("/m/a/", ma);
    dispatcher.registerServlet("/w/*/l/", wl);

    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/d/e/f"), root);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/a/b") , ab);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/a/b/") , ab);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/a/b/c") , ab);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/a/b/c/d") , ab);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/A/B") , root);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/a") , root);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/a/bc") , root);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/m/a/b/c") , ma);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/m/a/") , ma);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/m/a") , root);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/w/*/l/") , wl);
    // can only match the root
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/w/a/l/") , root);
    // Dispatcher does URI normalization
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/a/b/../../c/a"), ca);
}

MORDOR_UNITTEST(ServletDispatcher, vhosts)
{
    ServletDispatcher dispatcher;
    Servlet::ptr root(new DummyServlet), trogdor(new DummyServlet),
        mordor(new DummyServlet);

    dispatcher.registerServlet("http://trogdor/", trogdor);
    dispatcher.registerServlet("//mordor/", mordor);

    MORDOR_TEST_ASSERT(!dispatcher.getServlet("/"));
    MORDOR_TEST_ASSERT(!dispatcher.getServlet("//triton/"));
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("//trogdor/a/b") , trogdor);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("//mordor/a/b") , mordor);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("http://trogdor/a/b") , trogdor);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("http://mordor/a/b") , mordor);

    dispatcher.registerServlet("/", root);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/") , root);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("//triton/") , root);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("//trogdor/a/b") , trogdor);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("//mordor/a/b") , mordor);
}

MORDOR_UNITTEST(ServletDispatcher, wildcardBasic)
{
    ServletDispatcher dispatcher(true);
    Servlet::ptr root(new DummyServlet), axb(new DummyServlet), cxdx(new DummyServlet);

    dispatcher.registerServlet("/", root);
    dispatcher.registerServlet("/a/*/b", axb);
    dispatcher.registerServlet("/c/*/d/*/", cxdx);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/a/b/b"), axb);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/a/cc/b"), axb);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/a/cc/b/d"), axb);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/c/y/d/m/n"), cxdx);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/a/b/b/b"), axb);
    // not match
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/a/b/c"), root);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/b/*/b"), root);
}

MORDOR_UNITTEST(ServletDispatcher, wildcardLowerPriority)
{
    ServletDispatcher dispatcher(true);
    Servlet::ptr axb(new DummyServlet), abb(new DummyServlet);

    dispatcher.registerServlet("/a/*/b", axb);
    dispatcher.registerServlet("/a/b/b", abb);
    // both match, wildcard has lower priority
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/a/b/b/c"), abb);
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/a/c/b/c"), axb);
}

MORDOR_UNITTEST(ServletDispatcher, wildcardMatchSeqOrder)
{
    ServletDispatcher dispatcher(true);
    Servlet::ptr axb(new DummyServlet), acx(new DummyServlet);

    dispatcher.registerServlet("/a/*/b", axb);
    dispatcher.registerServlet("/a/c/*", acx);
    // only axb match
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/a/x/b/c"), axb);
    // only acx match
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/a/c/x/c"), acx);
    // both match, the one who has the wildcard in the most right win
    MORDOR_TEST_ASSERT_EQUAL(dispatcher.getServlet("/a/c/b/d"), acx);
}
