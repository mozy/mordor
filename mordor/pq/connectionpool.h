#ifndef __MORDOR_PQ_CONNECTIONPOOL_H__
#define __MORDOR_PQ_CONNECTIONPOOL_H__

#include <list>

#include <boost/shared_ptr.hpp>

#include "mordor/fibersynchronization.h"

namespace Mordor {

template <class T> class ConfigVar;
class IOManager;

namespace PQ {

class Connection;

class ConnectionPool : boost::noncopyable {
public:
    typedef boost::shared_ptr<ConnectionPool> ptr;

    ConnectionPool(const std::string &conninfo, IOManager *iomanager,
        size_t size = 5);
    ~ConnectionPool();
    boost::shared_ptr<Connection> getConnection();
    void resize(size_t num);

private:
    void releaseConnection(Mordor::PQ::Connection* conn);

private:
    std::list<boost::shared_ptr<Mordor::PQ::Connection> > m_busyConnections;
    std::list<boost::shared_ptr<Mordor::PQ::Connection> > m_freeConnections;
    std::string m_conninfo;
    IOManager *m_iomanager;
    FiberMutex m_mutex;
    FiberCondition m_condition;
    size_t m_total;
};

void associateConnectionPoolWithConfigVar(ConnectionPool &pool,
    boost::shared_ptr<ConfigVar<size_t> > configVar);

}}

#endif
