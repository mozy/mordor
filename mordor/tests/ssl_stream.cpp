// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "mordor/iomanager.h"
#include "mordor/streams/null.h"
#include "mordor/streams/pipe.h"
#include "mordor/streams/random.h"
#include "mordor/streams/ssl.h"
#include "mordor/streams/transfer.h"
#include "mordor/test/test.h"

using namespace Mordor;

static void accept(SSLStream::ptr server)
{
    server->accept();
}

MORDOR_UNITTEST(SSLStream, basic)
{
    WorkerPool pool;
    std::pair<Stream::ptr, Stream::ptr> pipes = pipeStream();
    
    SSLStream::ptr sslserver(new SSLStream(pipes.first, false));
    SSLStream::ptr sslclient(new SSLStream(pipes.second, true));

    pool.schedule(boost::bind(&accept, sslserver));
    sslclient->connect();
    pool.dispatch();

    Stream::ptr server = sslserver, client = sslclient;

    char buf[6];
    buf[5] = '\0';
    client->write("hello");
    client->flush();
    MORDOR_TEST_ASSERT_EQUAL(server->read(buf, 5), 5u);
    MORDOR_TEST_ASSERT_EQUAL((const char *)buf, "hello");
    server->write("world");
    server->flush();
    MORDOR_TEST_ASSERT_EQUAL(client->read(buf, 5), 5u);
    MORDOR_TEST_ASSERT_EQUAL((const char *)buf, "world");
}
