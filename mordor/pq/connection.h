#ifndef __MORDOR_PQ_CONNECTION_H__
#define __MORDOR_PQ_CONNECTION_H__
// Copyright (c) 2010 Mozy, Inc.

#include <boost/noncopyable.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

#include "preparedstatement.h"

namespace Mordor {

class Stream;

namespace PQ {

class Connection : boost::noncopyable
{
public:
    Connection(const std::string &conninfo, IOManager *ioManager = NULL,
        Scheduler *scheduler = NULL,
        bool connectImmediately = true);

    ConnStatusType status();

    void connect();
    /// @brief Resets the communication channel to the server.
    /// This function will close the connection to the server and attempt to
    /// reestablish a new connection to the same server, using all the same
    /// parameters previously used. This may be useful for error recovery if a
    /// working connection is lost.
    void reset();

    std::string escape(const std::string &string);
    std::string escapeBinary(const std::string &blob);

    /// @param name If non-empty, specifies to prepare this command on the
    /// server.  Statements prepared on the server msut have unique names
    /// (per-connection)
    PreparedStatement prepare(const std::string &command,
        const std::string &name = std::string(), PreparedStatement::ResultFormat = PreparedStatement::BINARY);
    /// Create a PreparedStatement object representing a previously prepared
    /// statement on the server
    PreparedStatement find(const std::string &name);

    Result execute(const std::string &command)
    { return prepare(command).execute(); }
    template <class T1>
    Result execute(const std::string &command, const T1 &param1)
    { return prepare(command).execute(param1); }
    template <class T1, class T2>
    Result execute(const std::string &command, const T1 &param1, const T2 &param2)
    { return prepare(command).execute(param1, param2); }
    template <class T1, class T2, class T3>
    Result execute(const std::string &command, const T1 &param1, const T2 &param2, const T3 &param3)
    { return prepare(command).execute(param1, param2, param3); }
    template <class T1, class T2, class T3, class T4>
    Result execute(const std::string &command, const T1 &param1, const T2 &param2, const T3 &param3, const T4 &param4)
    { return prepare(command).execute(param1, param2, param3, param4); }
    template <class T1, class T2, class T3, class T4, class T5>
    Result execute(const std::string &command, const T1 &param1, const T2 &param2, const T3 &param3, const T4 &param4, const T5 &param5)
    { return prepare(command).execute(param1, param2, param3, param4, param5); }
    template <class T1, class T2, class T3, class T4, class T5, class T6>
    Result execute(const std::string &command, const T1 &param1, const T2 &param2, const T3 &param3, const T4 &param4, const T5 &param5, const T6 &param6)
    { return prepare(command).execute(param1, param2, param3, param4, param5, param6); }
    template <class T1, class T2, class T3, class T4, class T5, class T6, class T7>
    Result execute(const std::string &command, const T1 &param1, const T2 &param2, const T3 &param3, const T4 &param4, const T5 &param5, const T6 &param6, const T7 &param7)
    { return prepare(command).execute(param1, param2, param3, param4, param5, param6, param7); }
    template <class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
    Result execute(const std::string &command, const T1 &param1, const T2 &param2, const T3 &param3, const T4 &param4, const T5 &param5, const T6 &param6, const T7 &param7, const T8 &param8)
    { return prepare(command).execute(param1, param2, param3, param4, param5, param6, param7, param8); }
    template <class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
    Result execute(const std::string &command, const T1 &param1, const T2 &param2, const T3 &param3, const T4 &param4, const T5 &param5, const T6 &param6, const T7 &param7, const T8 &param8, const T9 &param9)
    { return prepare(command).execute(param1, param2, param3, param4, param5, param6, param7, param8, param9); }

    /// Bulk copy data to the server
    struct CopyParams
    {
    protected:
        CopyParams(const std::string &table, boost::shared_ptr<PGconn> conn,
            SchedulerType *scheduler);

    public:
        virtual ~CopyParams() {}

        /// Execute
        virtual boost::shared_ptr<Stream> operator()() = 0;

        CopyParams &columns(const std::vector<std::string> &columns);

        CopyParams &binary();
        CopyParams &csv();

        CopyParams &delimiter(char delimiter);
        CopyParams &nullString(const std::string &nullString);
        CopyParams &header();
        CopyParams &quote(char quote);
        CopyParams &escape(char escape);
        CopyParams &notNullQuoteColumns(const std::vector<std::string> &columns);

    protected:
        boost::shared_ptr<Stream> execute(bool out);

    private:
        std::string m_table;
        SchedulerType *m_scheduler;
        boost::shared_ptr<PGconn> m_conn;
        std::vector<std::string> m_columns, m_notNullQuoteColumns;
        bool m_binary, m_csv, m_header;
        char m_delimiter, m_quote, m_escape;
        std::string m_nullString;
    };

    struct CopyInParams : public CopyParams
    {
    private:
        friend class Connection;
        CopyInParams(const std::string &table, boost::shared_ptr<PGconn> conn,
            SchedulerType *scheduler)
            : CopyParams(table, conn, scheduler)
        {}

    public:
        /// Execute
        boost::shared_ptr<Stream> operator()();
    };

    struct CopyOutParams : public CopyParams
    {
    private:
        friend class Connection;
        CopyOutParams(const std::string &table, boost::shared_ptr<PGconn> conn,
            SchedulerType *scheduler)
            : CopyParams(table, conn, scheduler)
        {}

    public:
        /// Execute
        boost::shared_ptr<Stream> operator()();
    };

    /// See http://www.postgresql.org/docs/current/static/sql-copy.html for the
    /// data format the server is expecting
    CopyInParams copyIn(const std::string &table);
    CopyOutParams copyOut(const std::string &table);

    const PGconn *conn() const { return m_conn.get(); }

private:
    std::string m_conninfo;
    SchedulerType *m_scheduler;
    boost::shared_ptr<PGconn> m_conn;
};

// Internal functions
#ifndef WIN32
void flush(PGconn *conn, SchedulerType *scheduler);
PGresult *nextResult(PGconn *conn, SchedulerType *scheduler);
#endif
std::string escape(PGconn *conn, const std::string &string);

}}

#endif
