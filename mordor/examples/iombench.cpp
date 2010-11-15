//
// Mordor IOManager benchmark app.
//
// Can act as both the client and the server.
//

#include "mordor/predef.h"

#include "netbench.h"

#include <iostream>

#include <boost/scoped_array.hpp>

#include "mordor/atomic.h"
#include "mordor/config.h"
#include "mordor/fibersynchronization.h"
#include "mordor/iomanager.h"
#include "mordor/log.h"
#include "mordor/main.h"
#include "mordor/socket.h"

using namespace Mordor;

static ConfigVar<int>::ptr g_iomThreads = Config::lookup<int>(
    "iombench.threads", 1, "Number of threads used by the iomanager");

static Logger::ptr g_log = Log::lookup("mordor:iombench");

class IOMBenchServer : public NetBenchServer
{
public:
    IOMBenchServer(IOManager& iom)
        : m_iom(iom)
    {}

    void run(std::string& host,
             size_t perConnToRead,
             size_t perConnToWrite,
             boost::function<void()> done)
    {
        m_iom.schedule(boost::bind(&IOMBenchServer::server, this, host,
                                   perConnToRead, perConnToWrite));
        done();
    }

    void stop()
    {
        m_sock->cancelAccept();
    }

private:
    void server(std::string host, size_t perConnToRead, size_t perConnToWrite)
    {
        m_data.reset(new char[perConnToWrite]);
        memset(m_data.get(), 'B', perConnToWrite);

        // figure out the host addr to use
        std::vector<Address::ptr> addrs;
        addrs = Address::lookup(host);
        MORDOR_VERIFY(!addrs.empty());

        // setup the server
        m_sock = addrs.front()->createSocket(m_iom, SOCK_STREAM);
        m_sock->setOption(SOL_SOCKET, SO_REUSEADDR, 1);
        m_sock->bind(addrs.front());
        m_sock->listen();

        // accept connections
        while (true) {
            Socket::ptr conn;
            try {
                conn = m_sock->accept();
            } catch (Exception&) {
                return;
            }
            m_iom.schedule(boost::bind(&IOMBenchServer::handleConn,
                                               this,
                                               conn,
                                               perConnToRead,
                                               perConnToWrite));
        }
    }

    void handleConn(Socket::ptr conn,
                    size_t perConnToRead, size_t perConnToWrite)
    {
        boost::scoped_array<char> rdata(new char[perConnToRead]);

        size_t n;
        while (true) {
            n = 0;
            while (n < perConnToRead) {
                n = conn->receive(&rdata[n], perConnToRead - n);
                if (n == 0) {
                    return;
                }
            }

            n = 0;
            while (n < perConnToWrite) {
                n += conn->send(&m_data[n], perConnToWrite - n);
            }
        }
    }

private:
    IOManager& m_iom;
    Socket::ptr m_sock;
    boost::scoped_array<char> m_data;
};

class IOMBenchClient : public NetBenchClient
{
public:
    IOMBenchClient(IOManager& iom)
        : m_iom(iom),
          m_connectedCond(m_mutex),
          m_readyCond(m_mutex),
          m_doneCond(m_mutex),
          m_round(0),
          m_totalClients(0),
          m_stop(false)
    { }

    void init(std::string& host, size_t perConnToRead, size_t perConnToWrite,
              boost::function<void()> done)
    {
        m_perConnToRead = perConnToRead;
        m_perConnToWrite = perConnToWrite;

        // prep the common send buffer
        m_data.reset(new char[m_perConnToWrite]);
        memset(m_data.get(), 'A', m_perConnToWrite);

        // figure out the host addr to use
        std::vector<Address::ptr> addrs;
        addrs = Address::lookup(host);
        MORDOR_VERIFY(!addrs.empty());

        // save off the server addr
        m_addr = addrs.front();

        done();
    }

    void prepClientsForNextRound(size_t newClients,
                                 size_t newActive,
                                 size_t iters,
                                 boost::function<void()> done)
    {
        m_iters = iters;
        m_newClients = 0;
        m_newClientsNeeded = newClients;
        m_totalClients += newClients;

        MORDOR_LOG_DEBUG(g_log) << "prep "
            << "newClients " << newClients << " "
            << "newActive " << newActive << " "
            << "iters " << iters;

        for (size_t i = 0; i < newClients; i++) {
            m_iom.schedule(boost::bind(&IOMBenchClient::client,
                                               this, newActive > 0));
            if (newActive) {
                newActive--;
            }
        }

        // Wait for all new clients to get connected and waiting at the
        // top of their request loop
        FiberMutex::ScopedLock lock(m_mutex);
        while (m_newClients != m_newClientsNeeded) {
            m_connectedCond.wait();
        }
        lock.unlock();

        done();
    }

    // implementers are encouraged to actually tally numOps in the done
    // callback so that we can check to make sure that we did the work
    // that we expected to
    void startRound(boost::function<void(size_t numOps)> done)
    {
        m_clientsDone = 0;
        m_opsDone = 0;
        m_round++;
        MORDOR_LOG_DEBUG(g_log) << "round start " << m_round
            << " " << m_clientsDone << " " << m_totalClients;
        m_readyCond.broadcast();

        // Wait for all clients to finish
        FiberMutex::ScopedLock lock(m_mutex);
        while (m_clientsDone != m_totalClients) {
            MORDOR_LOG_DEBUG(g_log) << "round wait " << m_clientsDone
                << " " << m_totalClients;
            m_doneCond.wait();
        }
        lock.unlock();

        MORDOR_LOG_DEBUG(g_log) << "round done " << m_opsDone;
        done(m_opsDone);
    }

    void stop()
    {
        FiberMutex::ScopedLock lock(m_mutex);
        m_stop = true;
        m_readyCond.broadcast();
    }

    void client(bool active)
    {
        MORDOR_LOG_DEBUG(g_log) << "client start " << active;

        // due to other synchronization I don't think we need
        // to actually lock to store the round number, but meh
        FiberMutex::ScopedLock lock(m_mutex);
        int round = m_round;
        lock.unlock();

        Socket::ptr conn = m_addr->createSocket(m_iom, SOCK_STREAM);
        conn->connect(m_addr);

        lock.lock();
        if (++m_newClients == m_newClientsNeeded) {
            m_connectedCond.signal();
        }
        lock.unlock();

        while (true) {
            waitForNextRound(round);

            if (m_stop) {
                return;
            }

            if (active) {
                sendRequests(conn);
            }

            if (++m_clientsDone == m_totalClients) {
                m_doneCond.signal();
            }
        }
    }

    void waitForNextRound(int& round)
    {
        FiberMutex::ScopedLock lock(m_mutex);
        ++round;
        while (!m_stop && round != m_round) {
            m_readyCond.wait();
        }
    }

    void sendRequests(Socket::ptr conn)
    {
        for (size_t i = 0; i < m_iters; ++i) {
            size_t n = 0;
            while (n < m_perConnToWrite) {
                n = conn->send(&m_data[n], m_perConnToWrite - n);
            }

            boost::scoped_array<char> buf(new char[m_perConnToRead]);
            n = 0;
            while (n < m_perConnToRead) {
                n += conn->receive(&buf[n], m_perConnToRead - n);
                MORDOR_ASSERT(n != 0);
            }

            m_opsDone++;
        }
    }

private:
    IOManager& m_iom;
    Address::ptr m_addr;

    boost::scoped_array<char> m_data;

    FiberMutex m_mutex;
    FiberCondition m_connectedCond;
    FiberCondition m_readyCond;
    FiberCondition m_doneCond;

    int m_round;
    size_t m_totalClients;
    size_t m_newClients;
    size_t m_newClientsNeeded;

    size_t m_iters;
    size_t m_perConnToRead;
    size_t m_perConnToWrite;

    Atomic<size_t> m_clientsDone;
    Atomic<size_t> m_opsDone;

    bool m_stop;
};

MORDOR_MAIN(int argc, char *argv[])
{
    try {
        NetBench bench(argc, argv);

        Config::loadFromEnvironment();
        IOManager iom(g_iomThreads->val());

        IOMBenchServer server(iom);
        IOMBenchClient client(iom);

        bench.run(&server, &client);
        iom.stop();
        return 0;
    } catch (...) {
        std::cerr << "caught: "
                  << boost::current_exception_diagnostic_information() << "\n";
        throw;
    }
}
