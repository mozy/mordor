// Copyright (c) 2009 - Mozy, Inc.

#include "connection.h"

#include "mordor/assert.h"
#include "mordor/iomanager.h"
#include "mordor/log.h"

#include "exception.h"

#ifdef MSVC
#pragma comment(lib, "libpq")
#endif

namespace Mordor {
namespace PQ {

static Logger::ptr g_log = Log::lookup("mordor:pq");

Connection::Connection(const std::string &conninfo, IOManager *ioManager,
    Scheduler *scheduler, bool connectImmediately)
: m_conninfo(conninfo)
{
#ifdef WINDOWS
    m_scheduler = scheduler;
#else
    m_scheduler = ioManager;
#endif
    if (connectImmediately)
        connect();
}

ConnStatusType
Connection::status()
{
    if (!m_conn)
        return CONNECTION_BAD;
    return PQstatus(m_conn.get());
}

void
Connection::connect()
{
#ifdef WINDOWS
    SchedulerSwitcher switcher(m_scheduler);
#else
    if (m_scheduler) {
        m_conn.reset(PQconnectStart(m_conninfo.c_str()), &PQfinish);
        if (!m_conn)
            MORDOR_THROW_EXCEPTION(std::bad_alloc());
        if (status() == CONNECTION_BAD)
            throwException(m_conn.get());
        if (PQsetnonblocking(m_conn.get(), 1))
            throwException(m_conn.get());
        int fd = PQsocket(m_conn.get());
        PostgresPollingStatusType whatToPoll = PGRES_POLLING_WRITING;
        while (true) {
            MORDOR_LOG_DEBUG(g_log) << m_conn.get() << " PQconnectPoll(): "
                << whatToPoll;
            switch (whatToPoll) {
                case PGRES_POLLING_READING:
                    m_scheduler->registerEvent(fd, SchedulerType::READ);
                    Scheduler::yieldTo();
                    break;
                case PGRES_POLLING_WRITING:
                    m_scheduler->registerEvent(fd, SchedulerType::WRITE);
                    Scheduler::yieldTo();
                    break;
                case PGRES_POLLING_FAILED:
                    throwException(m_conn.get());
                case PGRES_POLLING_OK:
                    MORDOR_LOG_INFO(g_log) << m_conn.get() << " PQconnectStart(\""
                        << m_conninfo << "\")";
                    return;
                default:
                    MORDOR_NOTREACHED();
            }
            whatToPoll = PQconnectPoll(m_conn.get());
        }
    } else
#endif
    {
        m_conn.reset(PQconnectdb(m_conninfo.c_str()), &PQfinish);
        if (!m_conn)
            MORDOR_THROW_EXCEPTION(std::bad_alloc());
        if (status() == CONNECTION_BAD)
            throwException(m_conn.get());
    }
    MORDOR_LOG_INFO(g_log) << m_conn.get() << " PQconnectdb(\"" << m_conninfo << "\")";
}

void
Connection::reset()
{
#ifdef WINDOWS
    SchedulerSwitcher switcher(m_scheduler);
#else
    if (m_scheduler) {
        if (!PQresetStart(m_conn.get()))
            throwException(m_conn.get());
        int fd = PQsocket(m_conn.get());
        PostgresPollingStatusType whatToPoll = PGRES_POLLING_WRITING;
        while (true) {
            MORDOR_LOG_DEBUG(g_log) << m_conn.get() << " PQresetPoll(): "
                << whatToPoll;
            switch (whatToPoll) {
                case PGRES_POLLING_READING:
                    m_scheduler->registerEvent(fd, SchedulerType::READ);
                    Scheduler::yieldTo();
                    break;
                case PGRES_POLLING_WRITING:
                    m_scheduler->registerEvent(fd, SchedulerType::WRITE);
                    Scheduler::yieldTo();
                    break;
                case PGRES_POLLING_FAILED:
                    throwException(m_conn.get());
                case PGRES_POLLING_OK:
                    MORDOR_LOG_INFO(g_log) << m_conn.get() << " PQresetStart()";
                    return;
                default:
                    MORDOR_NOTREACHED();
            }
            whatToPoll = PQresetPoll(m_conn.get());
        }
    } else
#endif
    {
        PQreset(m_conn.get());
        if (status() == CONNECTION_BAD)
            throwException(m_conn.get());
    }
    MORDOR_LOG_INFO(g_log) << m_conn.get() << " PQreset()";
}

std::string escape(PGconn *conn, const std::string &string)
{
    std::string result;
    result.resize(string.size() * 2);
    int error = 0;
    size_t resultSize = PQescapeStringConn(conn, &result[0],
        string.c_str(), string.size(), &error);
    if (error)
        throwException(conn);
    result.resize(resultSize);
    return result;
}

std::string
Connection::escape(const std::string &string)
{
    return PQ::escape(m_conn.get(), string);
}

static std::string escapeBinary(PGconn *conn, const std::string &blob)
{
    size_t length;
    std::string resultString;
    char *result = (char *)PQescapeByteaConn(conn,
        (unsigned char *)blob.c_str(), blob.size(), &length);
    if (!result)
        throwException(conn);
    try {
        resultString.append(result, length);
    } catch (...) {
        PQfreemem(result);
        throw;
    }
    PQfreemem(result);
    return resultString;
}

std::string
Connection::escapeBinary(const std::string &blob)
{
    return PQ::escapeBinary(m_conn.get(), blob);
}

#ifndef WINDOWS
void flush(PGconn *conn, SchedulerType *scheduler)
{
    while (true) {
        int result = PQflush(conn);
        MORDOR_LOG_DEBUG(g_log) << conn << " PQflush(): " << result;
        switch (result) {
            case 0:
                return;
            case -1:
                throwException(conn);
            case 1:
                scheduler->registerEvent(PQsocket(conn), SchedulerType::WRITE);
                Scheduler::yieldTo();
                continue;
            default:
                MORDOR_NOTREACHED();
        }
    }
}

PGresult *nextResult(PGconn *conn, SchedulerType *scheduler)
{
    while (true) {
        if (!PQconsumeInput(conn))
            throwException(conn);
        if (PQisBusy(conn)) {
            MORDOR_LOG_DEBUG(g_log) << conn << " PQisBusy()";
            scheduler->registerEvent(PQsocket(conn), SchedulerType::READ);
            Scheduler::yieldTo();
            continue;
        }
        MORDOR_LOG_DEBUG(g_log) << conn << " PQconsumeInput()";
        return PQgetResult(conn);
    }
}
#endif

PreparedStatement
Connection::prepare(const std::string &command, const std::string &name, PreparedStatement::ResultFormat resultFormat)
{
    if (!name.empty()) {
#ifdef WINDOWS
        SchedulerSwitcher switcher(m_scheduler);
#else
        if (m_scheduler) {
            if (!PQsendPrepare(m_conn.get(), name.c_str(), command.c_str(), 0, NULL))
                throwException(m_conn.get());
            flush(m_conn.get(), m_scheduler);
            boost::shared_ptr<PGresult> result(nextResult(m_conn.get(), m_scheduler),
                &PQclear);
            while (result) {
                ExecStatusType status = PQresultStatus(result.get());
                MORDOR_LOG_DEBUG(g_log) << m_conn.get() << " PQresultStatus("
                    << result.get() << "): " << PQresStatus(status);
                if (status != PGRES_COMMAND_OK)
                    throwException(result.get());
                result.reset(nextResult(m_conn.get(), m_scheduler),
                    &PQclear);
            }
            MORDOR_LOG_VERBOSE(g_log) << m_conn.get() << " PQsendPrepare(\""
                << name << "\", \"" << command << "\")";
            return PreparedStatement(m_conn, std::string(), name, m_scheduler, resultFormat);
        } else
#endif
        {
            boost::shared_ptr<PGresult> result(PQprepare(m_conn.get(),
                name.c_str(), command.c_str(), 0, NULL), &PQclear);
            if (!result)
                throwException(m_conn.get());
            ExecStatusType status = PQresultStatus(result.get());
            MORDOR_LOG_DEBUG(g_log) << m_conn.get() << " PQresultStatus("
                << result.get() << "): " << PQresStatus(status);
            if (status != PGRES_COMMAND_OK)
                throwException(result.get());
            MORDOR_LOG_VERBOSE(g_log) << m_conn.get() << " PQprepare(\"" << name
                << "\", \"" << command << "\")";
            return PreparedStatement(m_conn, std::string(), name, m_scheduler, resultFormat);
        }
    } else {
        return PreparedStatement(m_conn, command, name, m_scheduler, resultFormat);
    }
}

Connection::CopyInParams
Connection::copyIn(const std::string &table)
{
    return CopyInParams(table, m_conn, m_scheduler);
}

Connection::CopyOutParams
Connection::copyOut(const std::string &table)
{
    return CopyOutParams(table, m_conn, m_scheduler);
}

}}
