// Copyright (c) 2010 - Mozy, Inc.

#include "preparedstatement.h"

#include "mordor/assert.h"
#include "mordor/endian.h"
#include "mordor/log.h"
#include "mordor/iomanager.h"

#include "connection.h"
#include "exception.h"

#define BOOLOID 16
#define CHAROID 18
#define INT8OID 20
#define INT2OID 21
#define INT4OID 23
#define FLOAT4OID 700
#define FLOAT8OID 701
#define TIMESTAMPOID 1114
#define TIMESTAMPTZOID 1184

namespace Mordor {
namespace PQ {

static Logger::ptr g_log = Log::lookup("mordor:pq");

void
PreparedStatement::bind(size_t param, const Null &)
{
    ensure(param);
    m_paramValues[param - 1].clear();
    m_params[param - 1] = NULL;
    m_paramLengths[param - 1] = 0;
    m_paramFormats[param - 1] = 1;
    setType(param, 0);
}

void
PreparedStatement::bind(size_t param, const std::string &value)
{
    ensure(param);
    m_paramValues[param - 1] = value;
    m_params[param - 1] = m_paramValues[param - 1].c_str();
    m_paramLengths[param - 1] = m_paramValues[param - 1].size();
    m_paramFormats[param - 1] = 1;
    setType(param, 0);
}

void
PreparedStatement::bind(size_t param, const char *value)
{
    ensure(param);
    m_paramValues[param - 1] = value;
    m_params[param - 1] = m_paramValues[param - 1].c_str();
    m_paramLengths[param - 1] = m_paramValues[param - 1].size();
    m_paramFormats[param - 1] = 1;
    setType(param, 0);
}

void
PreparedStatement::bind(size_t param, bool value)
{
    ensure(param);
    m_paramValues[param - 1].resize(1);
    m_paramValues[param - 1][0] = value ? 1 : 0;
    m_params[param - 1] = m_paramValues[param - 1].c_str();
    m_paramLengths[param - 1] = m_paramValues[param - 1].size();
    m_paramFormats[param - 1] = 1;
    setType(param, BOOLOID);
}

void
PreparedStatement::bind(size_t param, char value)
{
    ensure(param);
    m_paramValues[param - 1].resize(1);
    m_paramValues[param - 1][0] = value;
    m_params[param - 1] = m_paramValues[param - 1].c_str();
    m_paramLengths[param - 1] = m_paramValues[param - 1].size();
    m_paramFormats[param - 1] = 1;
    setType(param, CHAROID);
}

void
PreparedStatement::bind(size_t param, short value)
{
    ensure(param);
    m_paramValues[param - 1].resize(2);
    *(short *)&m_paramValues[param - 1][0] = byteswapOnLittleEndian(value);
    m_params[param - 1] = m_paramValues[param - 1].c_str();
    m_paramLengths[param - 1] = m_paramValues[param - 1].size();
    m_paramFormats[param - 1] = 1;
    setType(param, INT2OID);
}

void
PreparedStatement::bind(size_t param, int value)
{
    ensure(param);
    m_paramValues[param - 1].resize(4);
    *(int *)&m_paramValues[param - 1][0] = byteswapOnLittleEndian(value);
    m_params[param - 1] = m_paramValues[param - 1].c_str();
    m_paramLengths[param - 1] = m_paramValues[param - 1].size();
    m_paramFormats[param - 1] = 1;
    setType(param, INT4OID);
}

void
PreparedStatement::bind(size_t param, long long value)
{
    ensure(param);
    m_paramValues[param - 1].resize(8);
    *(long long *)&m_paramValues[param - 1][0] = byteswapOnLittleEndian(value);
    m_params[param - 1] = m_paramValues[param - 1].c_str();
    m_paramLengths[param - 1] = m_paramValues[param - 1].size();
    m_paramFormats[param - 1] = 1;
    setType(param, INT8OID);
}

void
PreparedStatement::bind(size_t param, float value)
{
    ensure(param);
    m_paramValues[param - 1].resize(4);
    *(int *)&m_paramValues[param - 1][0] = byteswapOnLittleEndian(*(int *)&value);
    m_params[param - 1] = m_paramValues[param - 1].c_str();
    m_paramLengths[param - 1] = m_paramValues[param - 1].size();
    m_paramFormats[param - 1] = 1;
    setType(param, FLOAT4OID);
}

void
PreparedStatement::bind(size_t param, double value)
{
    ensure(param);
    m_paramValues[param - 1].resize(8);
    *(long long *)&m_paramValues[param - 1][0] = byteswapOnLittleEndian(*(long long *)&value);
    m_params[param - 1] = m_paramValues[param - 1].c_str();
    m_paramLengths[param - 1] = m_paramValues[param - 1].size();
    m_paramFormats[param - 1] = 1;
    setType(param, FLOAT8OID);
}

static const boost::posix_time::ptime postgres_epoch(boost::gregorian::date(2000, 1, 1));

void
PreparedStatement::bind(size_t param, const boost::posix_time::ptime &value, bool timezone)
{
    if (value.is_not_a_date_time()) {
        bind(param, Null());
        return;
    }
    ensure(param);
    m_paramValues[param - 1].resize(8);
    long long ticks = (value - postgres_epoch).total_microseconds();
    *(long long *)&m_paramValues[param - 1][0] = byteswapOnLittleEndian(*(long long *)&ticks);
    m_params[param - 1] = m_paramValues[param - 1].c_str();
    m_paramLengths[param - 1] = m_paramValues[param - 1].size();
    m_paramFormats[param - 1] = 1;
    setType(param, timezone ? TIMESTAMPTZOID : TIMESTAMPOID);
}

void
PreparedStatement::bindUntyped(size_t param, const std::string &value)
{
    ensure(param);
    m_paramValues[param - 1] = value;
    m_params[param - 1] = m_paramValues[param - 1].c_str();
    m_paramLengths[param - 1] = m_paramValues[param - 1].size();
    m_paramFormats[param - 1] = 0;
    setType(param, 0);
}

Result
PreparedStatement::execute()
{
    PGconn *conn = m_conn.lock().get();
    boost::shared_ptr<PGresult> result, next;
    int nParams = (int)m_params.size();
    Oid *paramTypes = NULL;
    int *paramLengths = NULL, *paramFormats = NULL;
    const char **params = NULL;
    if (nParams) {
        if (m_name.empty())
            paramTypes = &m_paramTypes[0];
        params = &m_params[0];
        paramLengths = &m_paramLengths[0];
        paramFormats = &m_paramFormats[0];
    }
    const char *api = NULL;
#ifndef WINDOWS
    SchedulerSwitcher switcher(m_scheduler);
#endif
    if (m_name.empty()) {
#ifndef WINDOWS
        if (m_scheduler) {
            api = "PQsendQueryParams";
            if (!PQsendQueryParams(conn, m_command.c_str(),
                nParams, paramTypes, params, paramLengths, paramFormats, m_resultFormat))
                throwException(conn);
            flush(conn, m_scheduler);
            next.reset(nextResult(conn, m_scheduler), &PQclear);
            while (next) {
                result = next;
                next.reset(nextResult(conn, m_scheduler), &PQclear);
                if (next) {
                    ExecStatusType status = PQresultStatus(next.get());
                    MORDOR_LOG_VERBOSE(g_log) << conn << "PQresultStatus(" <<
                        next.get() << "): " << PQresStatus(status);
                    switch (status) {
                        case PGRES_COMMAND_OK:
                        case PGRES_TUPLES_OK:
                            break;
                        default:
                            throwException(next.get());
                            MORDOR_NOTREACHED();
                    }
                }
            }
        } else
#endif
        {
            api = "PQexecParams";
            result.reset(PQexecParams(conn, m_command.c_str(),
                nParams, paramTypes, params, paramLengths, paramFormats, m_resultFormat),
                &PQclear);
        }
    } else {
#ifndef WINDOWS
        if (m_scheduler) {
            api = "PQsendQueryPrepared";
            if (!PQsendQueryPrepared(conn, m_name.c_str(),
                nParams, params, paramLengths, paramFormats, 1))
                throwException(conn);
            flush(conn, m_scheduler);
            next.reset(nextResult(conn, m_scheduler), &PQclear);
            while (next) {
                result = next;
                next.reset(nextResult(conn, m_scheduler), &PQclear);
                if (next) {
                    ExecStatusType status = PQresultStatus(next.get());
                    MORDOR_LOG_VERBOSE(g_log) << conn << "PQresultStatus(" <<
                        next.get() << "): " << PQresStatus(status);
                    switch (status) {
                        case PGRES_COMMAND_OK:
                        case PGRES_TUPLES_OK:
                            break;
                        default:
                            throwException(next.get());
                            MORDOR_NOTREACHED();
                    }
                }
            }
        } else
#endif
        {
            api = "PQexecPrepared";
            result.reset(PQexecPrepared(conn, m_name.c_str(),
                nParams, params, paramLengths, paramFormats, m_resultFormat),
                &PQclear);
        }
    }
    if (!result)
        throwException(conn);
    ExecStatusType status = PQresultStatus(result.get());
    MORDOR_ASSERT(api);
    MORDOR_LOG_VERBOSE(g_log) << conn << " " << api << "(\"" << m_command
        << m_name << "\", " << nParams << "), PQresultStatus(" << result.get()
        << "): " << PQresStatus(status);
    switch (status) {
        case PGRES_COMMAND_OK:
        case PGRES_TUPLES_OK:
            return Result(result);
        default:
            throwException(result.get());
            MORDOR_NOTREACHED();
    }
}

void
PreparedStatement::setType(size_t param, Oid type)
{
    if (m_name.empty())
        m_paramTypes[param - 1] = type;
}

void
PreparedStatement::ensure(size_t param)
{
    if (m_params.size() < param) {
        m_paramValues.resize(param);
        m_params.resize(param);
        for (size_t i = 0; i < param; ++i)
            m_params[i] = m_paramValues[i].c_str();
        m_paramLengths.resize(param);
        m_paramFormats.resize(param);
        if (m_name.empty())
            m_paramTypes.resize(param);
    }
}

}}
