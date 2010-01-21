#include "netbench.h"

#include "mordor/version.h"

#include <stdio.h>
#ifndef WINDOWS
#include <sys/resource.h>
#endif
#include <iostream>

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

static const char *usage =
"iombench [-h host:port] [-n conns] [-a active] [-b bytes_to_xfer] [-s] [-c] [-p]\n\n"
"-h: host:port (defaults to 'localhost:9000')\n"
"-s: run as server\n"
"-c: run as client (can run as both in same process)\n"
"-p: perform power of 2 increase in number of connected sockets\n";

NetBench::NetBench(int argc, char* argv[])
    : m_host("localhost:9000"),
      m_runServer(false),
      m_runClient(false),
      m_powTwo(false),
      m_maxConns(1),
      m_maxActive(0),
      m_numBytes(1),
      m_client(NULL),
      m_totalConns(1),
      m_totalActive(1),
      m_newConns(1),
      m_newActive(1),
      m_iters(10000)
{
    parseOptions(argc, argv);
}

void
NetBench::parseOptions(int argc, char* argv[])
{
#ifndef WINDOWS
    int c;
    extern char *optarg;

    if (argc == 1) {
        std::cerr << "usage: " << usage;
        exit(-1);
    }

    while ((c = getopt(argc, argv, "cspn:a:b:h:")) != -1) {
        switch (c) {
            case 'n':
                m_maxConns = boost::lexical_cast<size_t>(optarg);
                break;
            case 'a':
                m_maxActive = boost::lexical_cast<size_t>(optarg);
                break;
            case 'b':
                m_numBytes = boost::lexical_cast<size_t>(optarg);
                break;
            case 'h':
                m_host = optarg;
                break;
            case 's':
                m_runServer = true;
                break;
            case 'c':
                m_runClient = true;
                break;
            case 'p':
                m_powTwo = true;
                break;
            default:
                std::cerr << "usage: " << usage;
                exit(-1);
        }
    }
#else
    m_runServer = true;
    m_runClient = true;
    m_powTwo = true;
    m_maxConns = 8000;
#endif

    if (!m_runServer && !m_runClient) {
        std::cerr << "must run either the client or the server\n";
        std::cerr << "usage: " << usage;
        exit(-1);
    }

    if (m_maxConns < m_maxActive) {
        std::cerr << "numConns must be >= numActive\n";
        exit(-1);
    }

    if (m_maxActive == 0) {
        m_maxActive = m_maxConns;
    }
}

void
NetBench::run(NetBenchServer* server, NetBenchClient* client)
{
    std::cerr << "server "
              << "runServer: " << (m_runServer ? "yes " : "no ")
              << "runClient: " << (m_runClient ? "yes " : "no ")
              << "numConns: " << m_maxConns << " "
              << "numActive: " << m_maxActive << " "
              << "numBytes: " << m_numBytes << "\n";

#ifndef WINDOWS
    // the extra 20 fds are just some fluff in case we are called with
    // some small number of desired fds, like 1
    rlim_t rlfds = ((m_runServer && m_runClient ? 2 : 1) *
                    m_maxConns + 20);
    struct rlimit rl;
    rl.rlim_cur = rlfds * 2 + 20;
    rl.rlim_max = rlfds * 2 + 20;
    if (setrlimit(RLIMIT_NOFILE, &rl) < 0) {
        perror("setrlimit");
        exit(-1);
    }
#endif

    m_server = server;
    m_client = client;

    if (m_runServer) {
        server->run(m_host, 1, m_numBytes,
                    boost::bind(&NetBench::serverRunning, this));
    } else {
        runClient();
    }
}

void
NetBench::serverRunning()
{
    if (m_runClient) {
        runClient();
    }
}

void
NetBench::runClient()
{
    //
    // Print headers using gnuplot comments so that the output
    // of this benchmark can be used as a gnuplot data file.
    //
    std::cout << "#runServer: " << (m_runServer ? "yes " : "no ")
              << "runClient: " << (m_runClient ? "yes " : "no ")
              << "numConns: " << m_maxConns << " "
              << "numActive: " << m_maxActive << " "
              << "numBytes: " << m_numBytes << "\n"
              << "#\n"
              << "# total_conns total_time_sec num_ops "
              << "avg_time_usec ops_per_sec bw_MB_per_sec\n";

    m_client->init(m_host, m_numBytes, 1,
                   boost::bind(&NetBench::initRound, this));
}

void
NetBench::initRound()
{
    m_client->prepClientsForNextRound(m_newConns, m_newActive, m_iters,
                                      boost::bind(&NetBench::clientsReady,
                                                  this));
}

void
NetBench::clientsReady()
{
    m_start = boost::posix_time::microsec_clock::universal_time();
    m_client->startRound(boost::bind(&NetBench::roundDone, this, _1));
}

void
NetBench::roundDone(size_t actualOps)
{
    boost::posix_time::ptime end;
    end = boost::posix_time::microsec_clock::universal_time();
    boost::posix_time::time_duration diff = end - m_start;

    // Do all stat math using double to avoid rounding errors
    // during divides
    double active = (double)std::min(m_totalConns, m_maxActive);
    double elapsed = (double)diff.total_microseconds();
    double usAvg = elapsed / (active * m_iters);
    double numOps = active * m_iters;
    double opsSec = (numOps / elapsed) * 1000 * 1000;
    double mb = (m_numBytes * numOps) * 1024 * 1024;
    double bw = (mb / elapsed) / (1000 * 1000);

    // This is a sanity check to make sure that our sychronization
    // logic is correct and that we are really doing the amount
    // of work that we think we are
    if (actualOps != numOps) {
        std::cerr << "actualOps != numOps ("
                  << actualOps << " != " << numOps << ")\n";
        abort();
    }

    std::cout
        << m_totalConns << " "
        << elapsed / 1000000 << " "
        << numOps << " "
        << usAvg << " "
        << opsSec << " "
        << bw << "\n";

    // Figure out how many new clients to run with next round
    if (m_powTwo) {
        m_newConns = std::min(m_totalConns * 2 - m_totalConns,
                              m_maxConns - m_totalConns);
    } else {
        m_newConns = 1;
    }

    // Figure out how many total conns and active conns for next round
    m_totalConns += m_newConns;
    m_newActive = std::min(m_totalConns, m_maxActive) - m_totalActive;
    m_totalActive = m_totalActive + m_newActive;

    // Figure out how many iters to run for the next round.
    // We'll run for 1ms for non power of two based runs and
    // a full 3s otherwise (really, for full graphs you should
    // always run pow2)
    unsigned long long targetUs = m_powTwo ? 3000000 : 1000;
    m_iters = (size_t) (targetUs / m_totalActive / usAvg);
    if (m_iters == 0) {
        m_iters = 1;
    }

    if (m_newConns && m_totalConns <= m_maxConns) {
        initRound();
    } else {
        m_client->stop();
        m_server->stop();
    }
}
