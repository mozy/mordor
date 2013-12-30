#ifndef __MORDOR_PQ_CONNECTIONPOOL_H__
#define __MORDOR_PQ_CONNECTIONPOOL_H__

#include <list>

#include <boost/shared_ptr.hpp>

#include "mordor/fibersynchronization.h"

namespace Mordor {

class IOManager;

namespace PQ {

class Connection;

class ConnectionPool : boost::noncopyable {
public:
    typedef boost::shared_ptr<ConnectionPool> ptr;

    /// Constructor of ConnectionPool
    /// @param conninfo       db connect string
    /// @param iomanager      IO manager
    /// @param size           pool size, 5 by default
    /// @param idleTolerance  re-connect interval(in us) for idle
    ///                           connection, disable re-connect by setting it to 0
    ConnectionPool(const std::string &conninfo, IOManager *iomanager,
        size_t size = 5, unsigned long long idleTolerance = 0);
    ~ConnectionPool();
    boost::shared_ptr<Connection> getConnection();
    void resize(size_t num);

private:
    void releaseConnection(Mordor::PQ::Connection* conn);
    bool connectionExpired(unsigned long long freeTime) const;

private:
    std::list<boost::shared_ptr<Mordor::PQ::Connection> > m_busyConnections;

    /// @note element of the free list is pair of connection and a timestamp, the
    ///       timestamp indicates that when the connection was put into the free list.
    //        Before re-using any connection in the free list, should use the timestamp
    //        to calculate wheter the connection has been idle for too long, if so should
    //        re-connect it.
    std::list<std::pair<
        boost::shared_ptr<Mordor::PQ::Connection>, unsigned long long> > m_freeConnections;
    std::string m_conninfo;
    IOManager *m_iomanager;
    FiberMutex m_mutex;
    FiberCondition m_condition;
    size_t m_total;
    unsigned long long m_idleTolerance;
};

}}

#endif
