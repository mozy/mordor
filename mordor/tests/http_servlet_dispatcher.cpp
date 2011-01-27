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
        ca(new DummyServlet), ma(new DummyServlet);

    MORDOR_TEST_ASSERT(!dispatcher.getServlet("/d/e/f"));

    dispatcher.registerServlet("/", root);
    dispatcher.registerServlet("/a/b", ab);
    dispatcher.registerServlet("/c/a", ca);
    dispatcher.registerServlet("/m/a/", ma);

    MORDOR_TEST_ASSERT(dispatcher.getServlet("/d/e/f") == root);
    MORDOR_TEST_ASSERT(dispatcher.getServlet("/a/b") == ab);
    MORDOR_TEST_ASSERT(dispatcher.getServlet("/a/b/") == ab);
    MORDOR_TEST_ASSERT(dispatcher.getServlet("/a/b/c") == ab);
    MORDOR_TEST_ASSERT(dispatcher.getServlet("/a/b/c/d") == ab);
    MORDOR_TEST_ASSERT(dispatcher.getServlet("/A/B") == root);
    MORDOR_TEST_ASSERT(dispatcher.getServlet("/a") == root);
    MORDOR_TEST_ASSERT(dispatcher.getServlet("/a/bc") == root);
    MORDOR_TEST_ASSERT(dispatcher.getServlet("/m/a/b/c") == ma);
    MORDOR_TEST_ASSERT(dispatcher.getServlet("/m/a/") == ma);
    MORDOR_TEST_ASSERT(dispatcher.getServlet("/m/a") == root);
    // Dispatcher does URI normalization
    MORDOR_TEST_ASSERT(dispatcher.getServlet("/a/b/../../c/a") == ca);
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
    MORDOR_TEST_ASSERT(dispatcher.getServlet("//trogdor/a/b") == trogdor);
    MORDOR_TEST_ASSERT(dispatcher.getServlet("//mordor/a/b") == mordor);
    MORDOR_TEST_ASSERT(dispatcher.getServlet("http://trogdor/a/b") == trogdor);
    MORDOR_TEST_ASSERT(dispatcher.getServlet("http://mordor/a/b") == mordor);

    dispatcher.registerServlet("/", root);
    MORDOR_TEST_ASSERT(dispatcher.getServlet("/") == root);
    MORDOR_TEST_ASSERT(dispatcher.getServlet("//triton/") == root);
    MORDOR_TEST_ASSERT(dispatcher.getServlet("//trogdor/a/b") == trogdor);
    MORDOR_TEST_ASSERT(dispatcher.getServlet("//mordor/a/b") == mordor);
}
