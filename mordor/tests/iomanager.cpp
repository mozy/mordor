// Copyright (c) 2009 - Mozy, Inc.

#include <boost/bind.hpp>

#include "mordor/future.h"
#include "mordor/iomanager.h"
#include "mordor/test/test.h"
#include "mordor/thread.h"
#include "mordor/version.h"
#include "mordor/socket.h"

using namespace Mordor;
using namespace Mordor::Test;

namespace
{
    class EmptyTimeClass
    {
    public:
        typedef boost::shared_ptr<EmptyTimeClass> ptr;
    public:
        EmptyTimeClass()
            : m_timedOut(false)
            , m_future(NULL)
        {}

        void onTimer() {
            m_timedOut = true;
            if (m_future)
                m_future->signal();
        }

        void setFuture(Future<> *future) { m_future = future; }
        bool timedOut() const { return m_timedOut; }

    private:
        bool m_timedOut;
        Future<> * m_future;
    };

    void testTimerExpired(IOManager &manager)
    {
        Future<> future;
        EmptyTimeClass::ptr tester(new EmptyTimeClass());
        tester->setFuture(&future);
        MORDOR_TEST_ASSERT(tester.unique());
        manager.registerTimer(1000, boost::bind(&EmptyTimeClass::onTimer, tester));
        MORDOR_TEST_ASSERT(!tester.unique());
        // wait until timed out
        future.wait();
        MORDOR_TEST_ASSERT(tester->timedOut());
        MORDOR_TEST_ASSERT(tester.unique());
    }

    void testTimerNoExpire(IOManager &manager)
    {
        EmptyTimeClass::ptr tester(new EmptyTimeClass());
        MORDOR_TEST_ASSERT(tester.unique());
        Timer::ptr timer = manager.registerTimer(30000000,
            boost::bind(&EmptyTimeClass::onTimer, tester));
        MORDOR_TEST_ASSERT(!tester.unique());
        timer->cancel();
        MORDOR_TEST_ASSERT(!tester->timedOut());
        MORDOR_TEST_ASSERT(tester.unique());
    }
}

static void
singleTimer(int &sequence, int &expected)
{
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, expected);
}

MORDOR_UNITTEST(IOManager, singleTimer)
{
    int sequence = 0;
    IOManager manager;
    manager.registerTimer(0, boost::bind(&singleTimer, boost::ref(sequence), 1));
    MORDOR_TEST_ASSERT_EQUAL(sequence, 0);
    manager.dispatch();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 2);
}

MORDOR_UNITTEST(IOManager, laterTimer)
{
    int sequence = 0;
    IOManager manager;
    manager.registerTimer(100000, boost::bind(&singleTimer, boost::ref(sequence), 1));
    MORDOR_TEST_ASSERT_EQUAL(sequence, 0);
    manager.dispatch();
    ++sequence;
    MORDOR_TEST_ASSERT_EQUAL(sequence, 2);
}

MORDOR_UNITTEST(IOManager, timerRefCountNoExpired)
{
    IOManager manager;
    manager.schedule(boost::bind(testTimerNoExpire, boost::ref(manager)));
    manager.dispatch();
}

MORDOR_UNITTEST(IOManager, timerRefCountExpired)
{
    IOManager manager;
    manager.schedule(boost::bind(testTimerExpired, boost::ref(manager)));
    manager.dispatch();
}

static void
busyFiber(int &sequence)
{
    // this function will keep the scheduler busy (no idle)
    while (true) {
        --sequence;
        if (sequence <= 0)
            break;
        MORDOR_LOG_INFO(Mordor::Log::root()) << " sequence: " << sequence;
        Scheduler::yield();
    }
}

static void
switchThreadFiber(int &sequence, tid_t tidA, tid_t tidB)
{
    // this function will switch itself to anther thread and continue
    while (true) {
        if (sequence <= 0)
            return;
        tid_t otherTid = (gettid() == tidA) ? tidB : tidA;
        Scheduler::getThis()->schedule(Fiber::getThis(), otherTid);
        Scheduler::yieldTo();
    }
}

MORDOR_UNITTEST(IOManager, tickleDeadlock)
{
    IOManager manager(2, true);
    tid_t tidA = manager.rootThreadId();
    tid_t tidB = 0;
    {
        const std::vector<boost::shared_ptr<Thread> > threads =
            manager.threads();
        MORDOR_TEST_ASSERT_EQUAL(threads.size(), 1U);
        tidB = threads[0]->tid();
    }
    MORDOR_TEST_ASSERT_NOT_EQUAL(tidA, tidB);
    // there are >2 tickles each time the busyFiber get running.
    // sequence initial value will control how long the case will run
#ifdef LINUX
    // pipe buffer is 64K on Linux 2.6.x kernel
    int sequence = 32768;
#elif defined(OSX)
    // MAC pipe buffer is 8K by default
    int sequence = 8192;
#else
    // Other platform, sanity check with a small number
    int sequence = 4096;
#endif

    // schedule a busy fiber
    manager.schedule(boost::bind(busyFiber, boost::ref(sequence)));
    // schedule aditional two fibers to ensure that each thread will get
    // get at least one fiber to execute at anytime, no chance to idle.
    manager.schedule(boost::bind(switchThreadFiber,
                                 boost::ref(sequence), tidA, tidB),
                     tidA);
    manager.schedule(boost::bind(switchThreadFiber,
                                 boost::ref(sequence), tidA, tidB),
                     tidB);
    manager.stop();
}

namespace {
    struct Connection
    {
        Socket::ptr connect;
        Socket::ptr listen;
        Socket::ptr accept;
        IPAddress::ptr address;
    };
}

static Connection establishConn(IOManager &ioManager)
{
    Connection result;
    std::vector<Address::ptr> addresses = Address::lookup("localhost");
    MORDOR_TEST_ASSERT(!addresses.empty());
    result.address = boost::dynamic_pointer_cast<IPAddress>(addresses.front());
    result.listen = result.address->createSocket(ioManager, SOCK_STREAM);
    unsigned int opt = 1;
    result.listen->setOption(SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    while (true) {
        try {
            // Random port > 1000
            result.address->port(rand() % 50000 + 1000);
            result.listen->bind(result.address);
            break;
        } catch (AddressInUseException &) {
        }
    }
    result.listen->listen();
    result.connect = result.address->createSocket(ioManager, SOCK_STREAM);
    return result;
}

static void connect(IOManager &manager)
{
    try {
        Connection conns = establishConn(manager);
        conns.connect->sendTimeout(30000000);
        conns.connect->receiveTimeout(30000000);
        conns.listen->receiveTimeout(30000000);
        conns.connect->connect(conns.address);
        conns.connect->shutdown();
    } catch (TimedOutException &) {
    }
}

#ifdef OSX
#define THREADCOUNT 100U
#else
#define THREADCOUNT 10U
#endif

//Use 100 threads creating sockets, add/delete kevent into/from kqueue at same time,
//overwhelm the system so that it cannot respond to all the events,
//This could easily make kqueue return with EINPROGRESS (deferred deletion) error
//and we make sure error ignored in MacOS -- redmine #145605
MORDOR_UNITTEST(IOManager, ignoreEinprogressErrorOnMac)
{
    IOManager manager(THREADCOUNT, false);

    const std::vector<boost::shared_ptr<Thread> > threads = manager.threads();
    MORDOR_TEST_ASSERT_EQUAL(threads.size(), THREADCOUNT);

    for(size_t i=0; i<threads.size(); i++) {
        manager.schedule(boost::bind(connect, boost::ref(manager)), threads[i]->tid());
    }
    manager.stop();
}
