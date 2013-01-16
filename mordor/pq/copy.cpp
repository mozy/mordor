// Copyright (c) 2010 - Mozy, Inc.

#include "mordor/predef.h"

#include "mordor/assert.h"
#include "mordor/iomanager.h"
#include "mordor/log.h"
#include "mordor/streams/buffer.h"
#include "mordor/streams/stream.h"

#include "connection.h"
#include "exception.h"

namespace Mordor {
namespace PQ {

static Logger::ptr g_log = Log::lookup("mordor:pq");

Connection::CopyParams::CopyParams(const std::string &table,
    boost::shared_ptr<PGconn> conn, SchedulerType *scheduler)
    : m_table(table),
      m_scheduler(scheduler),
      m_conn(conn),
      m_binary(false),
      m_csv(false),
      m_header(false),
      m_delimiter('\0'),
      m_quote('\0'),
      m_escape('\0')
{}

Connection::CopyParams &
Connection::CopyParams::columns(const std::vector<std::string> &columns)
{
    m_columns = columns;
    return *this;
}

Connection::CopyParams &
Connection::CopyParams::binary()
{
    MORDOR_ASSERT(!m_csv);
    m_binary = true;
    return *this;
}

Connection::CopyParams &
Connection::CopyParams::csv()
{
    MORDOR_ASSERT(!m_binary);
    m_csv = true;
    return *this;
}

Connection::CopyParams &
Connection::CopyParams::delimiter(char delimiter)
{
    MORDOR_ASSERT(!m_binary);
    m_delimiter = delimiter;
    return *this;
}

Connection::CopyParams &
Connection::CopyParams::nullString(const std::string &nullString)
{
    MORDOR_ASSERT(!m_binary);
    m_nullString = nullString;
    return *this;
}

Connection::CopyParams &
Connection::CopyParams::header()
{
    MORDOR_ASSERT(m_csv);
    m_header = true;
    return *this;
}

Connection::CopyParams &
Connection::CopyParams::quote(char quote)
{
    MORDOR_ASSERT(m_csv);
    m_quote = quote;
    return *this;
}

Connection::CopyParams &
Connection::CopyParams::escape(char escape)
{
    MORDOR_ASSERT(m_csv);
    m_escape = escape;
    return *this;
}

Connection::CopyParams &
Connection::CopyParams::notNullQuoteColumns(const std::vector<std::string> &columns)
{
    MORDOR_ASSERT(m_csv);
    m_notNullQuoteColumns = columns;
    return *this;
}

class CopyInStream : public Stream
{
public:
    CopyInStream(boost::shared_ptr<PGconn> conn, SchedulerType *scheduler)
        : m_conn(conn),
          m_scheduler(scheduler)
    {}

    ~CopyInStream()
    {
        boost::shared_ptr<PGconn> sharedConn = m_conn.lock();
        if (sharedConn) {
            PGconn *conn = sharedConn.get();
            try {
                putCopyEnd(conn, "COPY IN aborted");
            } catch (...) {
            }
        }
    }

    bool supportsWrite() { return true; }

    void close(CloseType type)
    {
        MORDOR_ASSERT(type & WRITE);
        boost::shared_ptr<PGconn> sharedConn = m_conn.lock();
        if (sharedConn) {
            PGconn *conn = sharedConn.get();
            putCopyEnd(conn, NULL);
            m_conn.reset();
        }
    }

    size_t write(const void *buffer, size_t length)
    {
        boost::shared_ptr<PGconn> sharedConn = m_conn.lock();
        MORDOR_ASSERT(sharedConn);
        PGconn *conn = sharedConn.get();
        int status = 0;
        length = std::min<size_t>(length, 0x7fffffff);
#ifdef WINDOWS
        SchedulerSwitcher switcher(m_scheduler);
#endif
        while (status == 0) {
            status = PQputCopyData(conn, (const char *)buffer, (int)length);
            switch (status) {
                case 1:
                    return length;
                case -1:
                    throwException(conn);
#ifndef WINDOWS
                case 0:
                    MORDOR_ASSERT(m_scheduler);
                    m_scheduler->registerEvent(PQsocket(conn),
                        SchedulerType::WRITE);
                    Scheduler::yieldTo();
                    break;
#endif
                default:
                    MORDOR_NOTREACHED();
            }
        }
        MORDOR_NOTREACHED();
    }

private:
    void putCopyEnd(PGconn *conn, const char *error) {
#ifdef WINDOWS
        SchedulerSwitcher switcher(m_scheduler);
#endif
        int status = 0;
        while (status == 0) {
            status = PQputCopyEnd(conn, error);
            switch (status) {
                case 1:
                    break;
                case -1:
                    throwException(conn);
#ifndef WINDOWS
                case 0:
                    MORDOR_ASSERT(m_scheduler);
                    m_scheduler->registerEvent(PQsocket(conn),
                        SchedulerType::WRITE);
                    Scheduler::yieldTo();
                    break;
#endif
                default:
                    MORDOR_NOTREACHED();
            }
        }
#ifndef WINDOWS
        if (m_scheduler)
            PQ::flush(conn, m_scheduler);
#endif
        boost::shared_ptr<PGresult> result;
#ifndef WINDOWS
        if (m_scheduler)
            result.reset(nextResult(conn, m_scheduler), &PQclear);
        else
#endif
            result.reset(PQgetResult(conn), &PQclear);
        while (result) {
            ExecStatusType status = PQresultStatus(result.get());
            MORDOR_LOG_DEBUG(g_log) << conn << " PQresultStatus("
                << result.get() << "): " << PQresStatus(status);
            if (status != PGRES_COMMAND_OK)
                throwException(result.get());
#ifndef WINDOWS
            if (m_scheduler)
                result.reset(nextResult(conn, m_scheduler), &PQclear);
            else
#endif
                result.reset(PQgetResult(conn), &PQclear);
        }
        MORDOR_LOG_VERBOSE(g_log) << conn << " PQputCopyEnd(\""
            << (error ? error : "") << "\")";
    }

private:
    boost::weak_ptr<PGconn> m_conn;
    SchedulerType *m_scheduler;
};

class CopyOutStream : public Stream
{
public:
    CopyOutStream(boost::shared_ptr<PGconn> conn, SchedulerType *scheduler)
        : m_conn(conn),
          m_scheduler(scheduler)
    {}

    bool supportsRead() { return true; }

    size_t read(Buffer &buffer, size_t length)
    {
        if (m_readBuffer.readAvailable()) {
            length = (std::min)(m_readBuffer.readAvailable(), length);
            buffer.copyIn(m_readBuffer, length);
            m_readBuffer.consume(length);
            return length;
        }
        boost::shared_ptr<PGconn> sharedConn = m_conn.lock();
        if (!sharedConn)
            return 0;
        PGconn *conn = sharedConn.get();
#ifndef WINDOWS
        SchedulerSwitcher switcher(m_scheduler);
#endif
        int status = 0;
        do {
            char *data = NULL;
            status = PQgetCopyData(conn, &data,
#ifdef WINDOWS
                0
#else
                m_scheduler ? 1 : 0
#endif
                );
            switch (status) {
                case 0:
#ifdef WINDOWS
                    MORDOR_NOTREACHED();
#else
                    MORDOR_ASSERT(m_scheduler);
                    m_scheduler->registerEvent(PQsocket(conn),
                        SchedulerType::READ);
                    Scheduler::yieldTo();
                    continue;
#endif
                case -1:
                    break;
                case -2:
                    throwException(conn);
                default:
                    MORDOR_ASSERT(status > 0);
                    try {
                        m_readBuffer.copyIn(data, status);
                    } catch (...) {
                        PQfreemem(data);
                        throw;
                    }
                    PQfreemem(data);
                    break;
            }
        } while (false);

        if (status == -1) {
            m_conn.reset();
            boost::shared_ptr<PGresult> result;
#ifndef WINDOWS
            if (m_scheduler)
                result.reset(nextResult(conn, m_scheduler), &PQclear);
            else
#endif
                result.reset(PQgetResult(conn), &PQclear);
            while (result) {
                ExecStatusType status = PQresultStatus(result.get());
                MORDOR_LOG_DEBUG(g_log) << conn << " PQresultStatus("
                    << result.get() << "): " << PQresStatus(status);
                if (status != PGRES_COMMAND_OK)
                    throwException(result.get());
#ifndef WINDOWS
                if (m_scheduler)
                    result.reset(nextResult(conn, m_scheduler), &PQclear);
                else
#endif
                    result.reset(PQgetResult(conn), &PQclear);
            }
        }

        length = (std::min)(m_readBuffer.readAvailable(), length);
        buffer.copyIn(m_readBuffer, length);
        m_readBuffer.consume(length);
        return length;
    }

private:
    boost::weak_ptr<PGconn> m_conn;
    SchedulerType *m_scheduler;
    Buffer m_readBuffer;
};

Stream::ptr
Connection::CopyParams::execute(bool out)
{
    PGconn *conn = m_conn.get();
    std::ostringstream os;
    os << "COPY " << m_table << " ";
    if (!m_columns.empty()) {
        os << "(";
        for (std::vector<std::string>::const_iterator it(m_columns.begin());
            it != m_columns.end();
            ++it) {
            if (it != m_columns.begin())
                os << ", ";
            os << *it;
        }
        os << ") ";
    }
    os << (out ? "TO STDOUT" : "FROM STDIN");
    if (m_binary) {
        os << " BINARY";
    } else {
        if (m_delimiter != '\0')
            os << " DELIMITER '"
                << PQ::escape(conn, std::string(1, m_delimiter)) << '\'';
        if (!m_nullString.empty())
            os << " NULL '" << PQ::escape(conn, m_nullString) << '\'';
        if (m_csv) {
            os << " CSV";
            if (m_header)
                os << " HEADER";
            if (m_quote != '\0')
                os << " QUOTE '"
                    << PQ::escape(conn, std::string(1, m_quote)) << '\'';
            if (m_escape != '\0')
                os << " ESCAPE '"
                    << PQ::escape(conn, std::string(1, m_escape)) << '\'';
            if (!m_notNullQuoteColumns.empty()) {
                os << (out ? " FORCE QUOTE" : " FORCE NOT NULL ");
                for (std::vector<std::string>::const_iterator it(m_notNullQuoteColumns.begin());
                    it != m_notNullQuoteColumns.end();
                    ++it) {
                    if (it != m_notNullQuoteColumns.begin())
                        os << ", ";
                    os << *it;
                }
            }
        }
    }

    boost::shared_ptr<PGresult> result, next;
    const char *api = NULL;
#ifdef WINDOWS
    SchedulerSwitcher switcher(m_scheduler);
#else
    if (m_scheduler) {
        api = "PQsendQuery";
        if (!PQsendQuery(conn, os.str().c_str()))
            throwException(conn);
        flush(conn, m_scheduler);
        next.reset(nextResult(conn, m_scheduler), &PQclear);
        while (next) {
            result = next;
            if (PQresultStatus(result.get()) ==
                (out ? PGRES_COPY_OUT : PGRES_COPY_IN))
                break;
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
        api = "PQexec";
        result.reset(PQexec(conn, os.str().c_str()), &PQclear);
    }
    if (!result)
        throwException(conn);
    ExecStatusType status = PQresultStatus(result.get());
    MORDOR_ASSERT(api);
    MORDOR_LOG_VERBOSE(g_log) << conn << " " << api << "(\"" << os.str()
        << "\"), PQresultStatus(" << result.get() << "): "
        << PQresStatus(status);
    switch (status) {
        case PGRES_COMMAND_OK:
        case PGRES_TUPLES_OK:
            MORDOR_NOTREACHED();
        case PGRES_COPY_IN:
            MORDOR_ASSERT(!out);
            return Stream::ptr(new CopyInStream(m_conn, m_scheduler));
        case PGRES_COPY_OUT:
            MORDOR_ASSERT(out);
            return Stream::ptr(new CopyOutStream(m_conn, m_scheduler));
        default:
            throwException(result.get());
            MORDOR_NOTREACHED();
    }
}

Stream::ptr
Connection::CopyInParams::operator()()
{
    return execute(false);
}

Stream::ptr
Connection::CopyOutParams::operator()()
{
    return execute(true);
}

}}
