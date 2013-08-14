// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/iomanager.h"
#include "mordor/parallel.h"
#include "mordor/streams/hash.h"
#include "mordor/streams/null.h"
#include "mordor/streams/pipe.h"
#include "mordor/streams/random.h"
#include "mordor/streams/ssl.h"
#include "mordor/streams/transfer.h"
#include "mordor/test/test.h"
#include "mordor/workerpool.h"

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

static void writeLotsaData(
    Stream::ptr stream, unsigned long long toTransfer, bool &complete, std::string &hash)
{
    RandomStream::ptr random(new RandomStream());
    MD5Stream::ptr src(new MD5Stream(random));
    MORDOR_TEST_ASSERT_EQUAL(transferStream(src, stream, toTransfer), toTransfer);
    stream->flush();
    hash = src->hash();
    complete = true;
}

static void readLotsaData(
    Stream::ptr stream, unsigned long long toTransfer, bool &complete, std::string &hash)
{
    MD5Stream::ptr dest(new MD5Stream(NullStream::get_ptr()));
    MORDOR_TEST_ASSERT_EQUAL(transferStream(stream, dest, toTransfer), toTransfer);
    hash = dest->hash();
    complete = true;
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

    // Transfer 1 MB
    long long toTransfer = 1024 * 1024;
    std::vector<boost::function<void ()> > dgs;
    bool complete1 = false, complete2 = false, complete3 = false, complete4 = false;
    std::string hash1, hash2, hash3, hash4;
    dgs.push_back(
        boost::bind(&writeLotsaData, sslserver, toTransfer, boost::ref(complete1), boost::ref(hash1)));
    dgs.push_back(
        boost::bind(&readLotsaData, sslserver, toTransfer, boost::ref(complete2), boost::ref(hash2)));
    dgs.push_back(
        boost::bind(&writeLotsaData, sslclient, toTransfer, boost::ref(complete3), boost::ref(hash3)));
    dgs.push_back(
        boost::bind(&readLotsaData, sslclient, toTransfer, boost::ref(complete4), boost::ref(hash4)));
    parallel_do(dgs);
    MORDOR_ASSERT(complete1);
    MORDOR_ASSERT(complete2);
    MORDOR_ASSERT(complete3);
    MORDOR_ASSERT(complete4);
    MORDOR_TEST_ASSERT_EQUAL(hash1, hash4);
    MORDOR_TEST_ASSERT_EQUAL(hash2, hash3);
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
    pool.schedule(boost::bind(&accept, sslserver));
    sslclient->connect();
    pool.dispatch();

    pool.schedule(boost::bind(&readWorld, client,
        boost::ref(sequence)));
    pool.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 2);
    // Read is pending
    client->write("hello");
    client->flush(false);
    pool.dispatch();
    server->write("world");
    server->flush(false);
    pool.dispatch();
    MORDOR_TEST_ASSERT_EQUAL(++sequence, 4);
}
