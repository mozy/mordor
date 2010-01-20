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
    client->flush(false);
    MORDOR_TEST_ASSERT_EQUAL(server->read(buf, 5), 5u);
    MORDOR_TEST_ASSERT_EQUAL((const char *)buf, "hello");
    server->write("world");
    server->flush(false);
    MORDOR_TEST_ASSERT_EQUAL(client->read(buf, 5), 5u);
    MORDOR_TEST_ASSERT_EQUAL((const char *)buf, "world");
}

static void writeLotsaData(Stream::ptr stream, unsigned long long toTransfer)
{
    RandomStream random;
    MORDOR_TEST_ASSERT_EQUAL(transferStream(random, stream, toTransfer), toTransfer);
    stream->flush();
}

static void readLotsaData(Stream::ptr stream, unsigned long long toTransfer)
{
    MORDOR_TEST_ASSERT_EQUAL(transferStream(stream, NullStream::get(), toTransfer), toTransfer);
}

MORDOR_UNITTEST(SSLStream, duplexStress)
{
    WorkerPool pool;
    // Force more fiber context switches by having a smaller buffer
    std::pair<Stream::ptr, Stream::ptr> pipes = pipeStream(1024);

    SSLStream::ptr sslserver(new SSLStream(pipes.first, false));
    SSLStream::ptr sslclient(new SSLStream(pipes.second, true));

    pool.schedule(boost::bind(&accept, sslserver));
    sslclient->connect();
    pool.dispatch();

    // Transfer 100 MB
    long long toTransfer = 100 * 1024 * 1024;
    std::vector<boost::function<void ()> > dgs;
    dgs.push_back(boost::bind(&writeLotsaData, sslserver, toTransfer));
    dgs.push_back(boost::bind(&readLotsaData, sslserver, toTransfer));
    dgs.push_back(boost::bind(&writeLotsaData, sslclient, toTransfer));
    dgs.push_back(boost::bind(&readLotsaData, sslclient, toTransfer));
    parallel_do(dgs);
}

static void readWorld(Stream::ptr stream, int &sequence)
{
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 1);
    char buf[6];
    buf[5] = '\0';
    MORDOR_TEST_ASSERT_EQUAL(stream->read(buf, 5), 5u);
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 3);
    MORDOR_TEST_ASSERT_EQUAL((const char *)buf, "world");
}

MORDOR_UNITTEST(SSLStream, forceDuplex)
{
    WorkerPool pool;
    std::pair<Stream::ptr, Stream::ptr> pipes = pipeStream();

    SSLStream::ptr sslserver(new SSLStream(pipes.first, false));
    SSLStream::ptr sslclient(new SSLStream(pipes.second, true));

    Stream::ptr server = sslserver, client = sslclient;

    int sequence = 0;
    pool.schedule(boost::bind(&readWorld, client,
        boost::ref(sequence)));
    pool.dispatch();
    // Read is pending
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 2);
    pool.schedule(boost::bind(&accept, sslserver));
    client->write("hello");
    client->flush(false);
    pool.dispatch();
    server->write("world");
    server->flush(false);
    pool.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 4);
}
