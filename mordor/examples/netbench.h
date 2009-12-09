#ifndef NET_BENCH_SERVER_H__
#define NET_BENCH_SERVER_H__

#include <boost/function.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

class NetBenchServer
{
public:
    virtual ~NetBenchServer() {}

    // start the implementation of the server to be benchmarked
    virtual void run(std::string& host,
                     size_t perConnToRead,
                     size_t perConnToWrite,
                     boost::function<void()> done) = 0;

    // called when the test is over prior to destructor
    virtual void stop() = 0;
};

class NetBenchClient
{
public:
    virtual ~NetBenchClient () {}

    // initialize the client to be benchmarked
    virtual void init(std::string& host,
                      size_t perConnToRead, size_t perConnToWrite,
                      boost::function<void()> done) = 0;

    // connect the requested number of clients and make some active.
    // each active client should run iters worth of iterations of
    // reads/writes using the parameters given in the init() call
    virtual void prepClientsForNextRound(size_t newClients,
                                         size_t newActive,
                                         size_t iters,
                                         boost::function<void()> done) = 0;

    // implementers are encouraged to actually tally numOps in the done
    // callback so that we can check to make sure that we did the work
    // that we expected to
    virtual void startRound(boost::function<void(size_t numOps)> done) = 0;

    // called when the test is over prior to destructor
    virtual void stop() = 0;
};

class NetBench
{
public:
    NetBench(int argc, char* argv[]);

    // Kick off the actual benchmark run
    void run(NetBenchServer* server, NetBenchClient* client);

protected:
    void parseOptions(int argc, char* argv[]);
    void serverRunning();
    void runClient();
    void initRound();
    void clientsReady();
    void roundDone(size_t actualOps);

public:
    // Benchmark parameters
    std::string m_host;
    bool m_runServer;
    bool m_runClient;
    bool m_powTwo;
    size_t m_maxConns;
    size_t m_maxActive;
    size_t m_numBytes;

    // Benchmark state

    // Server implementation
    NetBenchServer* m_server;

    // Client implementation
    NetBenchClient* m_client;

    // round that we are on, i.e. a batch of clients (both active
    // and inactive) connect to the server and do their work
    int m_round;

    // Total number of connected clients
    size_t m_totalConns;

    // Total number of active clients
    size_t m_totalActive;

    // Total number of new connections to create in the next round
    size_t m_newConns;

    // Number of new active connections to create in the next round
    size_t m_newActive;

    // Number of iterations for each active client in the round
    size_t m_iters;

    // Total number of clients that have completed the round
    size_t m_numDone;

    // Total number of ops completed in the round (should be
    // equal to m_maxActive * m_iters at the end)
    size_t m_numOps;

    // us at the start of the round
    boost::posix_time::ptime m_start;
};

#endif
