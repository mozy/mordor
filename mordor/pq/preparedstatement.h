#ifndef __MORDOR_PQ_PREPAREDSTATEMENT_H__
#define __MORDOR_PQ_PREPAREDSTATEMENT_H__
// Copyright (c) 2010 - Mozy, Inc.

#include <vector>

#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/weak_ptr.hpp>

#include "result.h"

namespace Mordor {

class IOManager;
class Scheduler;

namespace PQ {

#ifdef WIN32
typedef Scheduler SchedulerType;
#else
typedef IOManager SchedulerType;
#endif

struct Null {};
struct Skip {};

class PreparedStatement
{
    friend class Connection;
public:
    enum ResultFormat {
        TEXT   = 0,
        BINARY = 1
    };

private:
    PreparedStatement(boost::shared_ptr<PGconn> conn,
        const std::string &command, const std::string &name,
        SchedulerType *scheduler, ResultFormat format = BINARY)
        : m_conn(conn),
          m_command(command),
          m_name(name),
          m_scheduler(scheduler),
          m_resultFormat(format)
    {}

public:
    PreparedStatement(): m_resultFormat(BINARY) {}

    void bind(size_t param, const Null &);
    void bind(size_t param, const char *value);
    void bind(size_t param, const std::string &value);
    void bind(size_t param, bool value);
    void bind(size_t param, char value);
    void bind(size_t param, short value);
    void bind(size_t param, int value);
    void bind(size_t param, long long value);
    void bind(size_t param, float value);
    void bind(size_t param, double value);
    void bind(size_t param, const boost::posix_time::ptime &value, bool timezone = false);
    void bindUntyped(size_t param, const std::string &value);

    Result execute();
    template <class T1>
    Result execute(const T1 &param1)
    {
        bind(1, param1);
        return execute();
    }
    template <class T1, class T2>
    Result execute(const T1 &param1, const T2 &param2)
    {
        bind(2, param2);
        bind(1, param1);
        return execute();
    }
    template <class T1, class T2, class T3>
    Result execute(const T1 &param1, const T2 &param2, const T3 &param3)
    {
        bind(3, param3);
        bind(2, param2);
        bind(1, param1);
        return execute();
    }
    template <class T1, class T2, class T3, class T4>
    Result execute(const T1 &param1, const T2 &param2, const T3 &param3, const T4 &param4)
    {
        bind(4, param4);
        bind(3, param3);
        bind(2, param2);
        bind(1, param1);
        return execute();
    }
    template <class T1, class T2, class T3, class T4, class T5>
    Result execute(const T1 &param1, const T2 &param2, const T3 &param3, const T4 &param4, const T5 &param5)
    {
        bind(5, param5);
        bind(4, param4);
        bind(3, param3);
        bind(2, param2);
        bind(1, param1);
        return execute();
    }
    template <class T1, class T2, class T3, class T4, class T5, class T6>
    Result execute(const T1 &param1, const T2 &param2, const T3 &param3, const T4 &param4, const T5 &param5, const T6 &param6)
    {
        bind(6, param6);
        bind(5, param5);
        bind(4, param4);
        bind(3, param3);
        bind(2, param2);
        bind(1, param1);
        return execute();
    }
    template <class T1, class T2, class T3, class T4, class T5, class T6, class T7>
    Result execute(const T1 &param1, const T2 &param2, const T3 &param3, const T4 &param4, const T5 &param5, const T6 &param6, const T7 &param7)
    {
        bind(7, param7);
        bind(6, param6);
        bind(5, param5);
        bind(4, param4);
        bind(3, param3);
        bind(2, param2);
        bind(1, param1);
        return execute();
    }
    template <class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
    Result execute(const T1 &param1, const T2 &param2, const T3 &param3, const T4 &param4, const T5 &param5, const T6 &param6, const T7 &param7, const T8 &param8)
    {
        bind(8, param8);
        bind(7, param7);
        bind(6, param6);
        bind(5, param5);
        bind(4, param4);
        bind(3, param3);
        bind(2, param2);
        bind(1, param1);
        return execute();
    }
    template <class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
    Result execute(const T1 &param1, const T2 &param2, const T3 &param3, const T4 &param4, const T5 &param5, const T6 &param6, const T7 &param7, const T8 &param8, const T9 &param9)
    {
        bind(9, param9);
        bind(8, param8);
        bind(7, param7);
        bind(6, param6);
        bind(5, param5);
        bind(4, param4);
        bind(3, param3);
        bind(2, param2);
        bind(1, param1);
        return execute();
    }

private:
    void bind(size_t param, const Skip &) {}
    void setType(size_t param, Oid type);
    void ensure(size_t count);

private:
    boost::weak_ptr<PGconn> m_conn;
    std::string m_command;
    std::string m_name;
    SchedulerType *m_scheduler;
    ResultFormat m_resultFormat;
    std::vector<Oid> m_paramTypes;
    std::vector<std::string> m_paramValues;
    std::vector<const char *> m_params;
    std::vector<int> m_paramLengths, m_paramFormats;
};

}}

#endif
