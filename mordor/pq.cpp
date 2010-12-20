// Copyright (c) 2009 - Mozy, Inc.

#include "pq.h"

#include "assert.h"
#include "endian.h"
#include "iomanager.h"
#include "log.h"
#include "streams/buffer.h"
#include "streams/stream.h"

#ifdef MSVC
#pragma comment(lib, "libpq")
#endif

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

static Logger::ptr g_log = Log::lookup("mordor:pq");

namespace PQ {

static void throwException(PGconn *conn)
{
    const char *error = PQerrorMessage(conn);
    MORDOR_LOG_ERROR(g_log) << conn << " connection error: " << error;
    MORDOR_THROW_EXCEPTION(ConnectionException(error));
}

static void throwException(PGresult *result)
{
    std::string message = PQresultErrorMessage(result);
    const char *sqlstate = PQresultErrorField(result, PG_DIAG_SQLSTATE);

    MORDOR_LOG_ERROR(g_log) << result << " " << sqlstate << ": " << message;
    if (!sqlstate || strlen(sqlstate) != 5)
        MORDOR_THROW_EXCEPTION(Exception(message));
    switch (sqlstate[0]) {
        case '2':
            switch (sqlstate[1]) {
                case '2':
                    if (strncmp(sqlstate + 2, "000", 3) == 0)
                        MORDOR_THROW_EXCEPTION(DataException(message));
                    else if (strncmp(sqlstate + 2, "02E", 3) == 0)
                        MORDOR_THROW_EXCEPTION(ArraySubscriptError(message));
                    else if (strncmp(sqlstate + 2, "021", 3) == 0)
                        MORDOR_THROW_EXCEPTION(CharacterNotInRepertoireException(message));
                    else if (strncmp(sqlstate + 2, "008", 3) == 0)
                        MORDOR_THROW_EXCEPTION(OverflowException(message));
                    else if (strncmp(sqlstate + 2, "012", 3) == 0)
                        MORDOR_THROW_EXCEPTION(DivisionByZeroException(message));
                    else if (strncmp(sqlstate + 2, "005", 3) == 0)
                        MORDOR_THROW_EXCEPTION(AssignmentError(message));
                    else if (strncmp(sqlstate + 2, "00B", 3) == 0)
                        MORDOR_THROW_EXCEPTION(EscapeCharacterConflictException(message));
                    else if (strncmp(sqlstate + 2, "01E", 3) == 0 ||
                        strncmp(sqlstate + 2, "01F", 3) == 0 ||
                        strncmp(sqlstate + 2, "01G", 3) == 0)
                        MORDOR_THROW_EXCEPTION(InvalidArgumentException(message));
                    else if (strncmp(sqlstate + 2, "004", 3) == 0)
                        MORDOR_THROW_EXCEPTION(NullValueNotAllowedException(message));
                    else
                        MORDOR_THROW_EXCEPTION(DataException(message));
                case '3':
                    if (strncmp(sqlstate + 2, "000", 3) == 0)
                        MORDOR_THROW_EXCEPTION(IntegrityConstraintViolationException(message));
                    else if (strncmp(sqlstate + 2, "001", 3) == 0)
                        MORDOR_THROW_EXCEPTION(RestrictViolationException(message));
                    else if (strncmp(sqlstate + 2, "502", 3) == 0)
                        MORDOR_THROW_EXCEPTION(NotNullViolationException(message));
                    else if (strncmp(sqlstate + 2, "503", 3) == 0)
                        MORDOR_THROW_EXCEPTION(ForeignKeyViolationException(message));
                    else if (strncmp(sqlstate + 2, "505", 3) == 0)
                        MORDOR_THROW_EXCEPTION(UniqueViolationException(message));
                    else if (strncmp(sqlstate + 2, "514", 3) == 0)
                        MORDOR_THROW_EXCEPTION(UniqueViolationException(message));
                    else
                        MORDOR_THROW_EXCEPTION(IntegrityConstraintViolationException(message));
                case '5':
                    if (strncmp(sqlstate + 2, "000", 3) == 0)
                        MORDOR_THROW_EXCEPTION(InvalidTransactionStateException(message));
                    else if (strncmp(sqlstate + 2, "001", 3) == 0)
                        MORDOR_THROW_EXCEPTION(ActiveTransactionException(message));
                    else if (strncmp(sqlstate + 2, "002", 3) == 0)
                        MORDOR_THROW_EXCEPTION(BranchTransactionAlreadyActiveException(message));
                    else if (strncmp(sqlstate + 2, "008", 3) == 0)
                        MORDOR_THROW_EXCEPTION(HeldCursorRequiresSameIsolationLevelException(message));
                    else if (strncmp(sqlstate + 2, "003", 3) == 0)
                        MORDOR_THROW_EXCEPTION(InappropriateAccessModeForBranchTransactionException(message));
                    else if (strncmp(sqlstate + 2, "004", 3) == 0)
                        MORDOR_THROW_EXCEPTION(InappropriateIsolationLevelForBranchTransactionException(message));
                    else if (strncmp(sqlstate + 2, "005", 3) == 0)
                        MORDOR_THROW_EXCEPTION(NoActiveTransactionForBranchException(message));
                    else if (strncmp(sqlstate + 2, "006", 3) == 0)
                        MORDOR_THROW_EXCEPTION(ReadOnlyTransactionException(message));
                    else if (strncmp(sqlstate + 2, "007", 3) == 0)
                        MORDOR_THROW_EXCEPTION(SchemaAndDataStatementMixingNotSupportedException(message));
                    else if (strncmp(sqlstate + 2, "P01", 3) == 0)
                        MORDOR_THROW_EXCEPTION(NoActiveTransactionException(message));
                    else if (strncmp(sqlstate + 2, "P02", 3) == 0)
                        MORDOR_THROW_EXCEPTION(InFailedTransactionException(message));
                    else
                        MORDOR_THROW_EXCEPTION(InvalidTransactionStateException(message));
                default:
                    break;
            }
        case '4':
            switch (sqlstate[1]) {
                case '0':
                    if (strncmp(sqlstate + 2, "000", 3) == 0)
                        MORDOR_THROW_EXCEPTION(TransactionRollbackException(message));
                    else if (strncmp(sqlstate + 2, "002", 3) == 0)
                        MORDOR_THROW_EXCEPTION(TransactionIntegrityConstraintViolationException(message));
                    else if (strncmp(sqlstate + 2, "001", 3) == 0)
                        MORDOR_THROW_EXCEPTION(SerializationFailureException(message));
                    else if (strncmp(sqlstate + 2, "003", 3) == 0)
                        MORDOR_THROW_EXCEPTION(StatementCompletionUnknownException(message));
                    else if (strncmp(sqlstate + 2, "P01", 3) == 0)
                        MORDOR_THROW_EXCEPTION(DeadlockDetectedException(message));
                    else
                        MORDOR_THROW_EXCEPTION(TransactionRollbackException(message));
                case '2':
                    if (strncmp(sqlstate + 2, "000", 3) == 0)
                        MORDOR_THROW_EXCEPTION(AccessRuleViolationException(message));
                    else if (strncmp(sqlstate + 2, "601", 3) == 0)
                        MORDOR_THROW_EXCEPTION(SyntaxError(message));
                    else if (strncmp(sqlstate + 2, "501", 3) == 0)
                        MORDOR_THROW_EXCEPTION(InsufficientPrivilegeException(message));
                    else if (strncmp(sqlstate + 2, "846", 3) == 0)
                        MORDOR_THROW_EXCEPTION(CannotCoerceException(message));
                    else if (strncmp(sqlstate + 2, "803", 3) == 0)
                        MORDOR_THROW_EXCEPTION(GroupingError(message));
                    else if (strncmp(sqlstate + 2, "P20", 3) == 0)
                        MORDOR_THROW_EXCEPTION(WindowingError(message));
                    else if (strncmp(sqlstate + 2, "P19", 3) == 0)
                        MORDOR_THROW_EXCEPTION(InvalidRecursionException(message));
                    else if (strncmp(sqlstate + 2, "830", 3) == 0)
                        MORDOR_THROW_EXCEPTION(InvalidForeignKeyException(message));
                    else if (strncmp(sqlstate + 2, "602", 3) == 0)
                        MORDOR_THROW_EXCEPTION(InvalidNameException(message));
                    else if (strncmp(sqlstate + 2, "622", 3) == 0)
                        MORDOR_THROW_EXCEPTION(NameTooLongException(message));
                    else if (strncmp(sqlstate + 2, "939", 3) == 0)
                        MORDOR_THROW_EXCEPTION(ReservedNameException(message));
                    else if (strncmp(sqlstate + 2, "804", 3) == 0)
                        MORDOR_THROW_EXCEPTION(DatatypeMismatchException(message));
                    else if (strncmp(sqlstate + 2, "P18", 3) == 0)
                        MORDOR_THROW_EXCEPTION(IndeterminateDatatypeException(message));
                    else if (strncmp(sqlstate + 2, "809", 3) == 0)
                        MORDOR_THROW_EXCEPTION(WrongObjectTypeException(message));
                    else if (strncmp(sqlstate + 2, "703", 3) == 0)
                        MORDOR_THROW_EXCEPTION(UndefinedColumnException(message));
                    else if (strncmp(sqlstate + 2, "883", 3) == 0)
                        MORDOR_THROW_EXCEPTION(UndefinedFunctionException(message));
                    else if (strncmp(sqlstate + 2, "P01", 3) == 0)
                        MORDOR_THROW_EXCEPTION(UndefinedTableException(message));
                    else if (strncmp(sqlstate + 2, "P02", 3) == 0)
                        MORDOR_THROW_EXCEPTION(UndefinedParameterException(message));
                    else if (strncmp(sqlstate + 2, "704", 3) == 0)
                        MORDOR_THROW_EXCEPTION(UndefinedObjectException(message));
                    else if (strncmp(sqlstate + 2, "701", 3) == 0)
                        MORDOR_THROW_EXCEPTION(DuplicateColumnException(message));
                    else if (strncmp(sqlstate + 2, "P03", 3) == 0)
                        MORDOR_THROW_EXCEPTION(DuplicateCursorException(message));
                    else if (strncmp(sqlstate + 2, "P04", 3) == 0)
                        MORDOR_THROW_EXCEPTION(DuplicateDatabaseException(message));
                    else if (strncmp(sqlstate + 2, "723", 3) == 0)
                        MORDOR_THROW_EXCEPTION(DuplicateFunctionException(message));
                    else if (strncmp(sqlstate + 2, "P05", 3) == 0)
                        MORDOR_THROW_EXCEPTION(DuplicatePreparedStatementException(message));
                    else if (strncmp(sqlstate + 2, "P06", 3) == 0)
                        MORDOR_THROW_EXCEPTION(DuplicateSchemaException(message));
                    else if (strncmp(sqlstate + 2, "P07", 3) == 0)
                        MORDOR_THROW_EXCEPTION(DuplicateTableException(message));
                    else if (strncmp(sqlstate + 2, "712", 3) == 0)
                        MORDOR_THROW_EXCEPTION(DuplicateAliasException(message));
                    else if (strncmp(sqlstate + 2, "710", 3) == 0)
                        MORDOR_THROW_EXCEPTION(DuplicateObjectException(message));
                    else if (strncmp(sqlstate + 2, "702", 3) == 0)
                        MORDOR_THROW_EXCEPTION(AmbiguousColumnException(message));
                    else if (strncmp(sqlstate + 2, "725", 3) == 0)
                        MORDOR_THROW_EXCEPTION(AmbiguousFunctionException(message));
                    else if (strncmp(sqlstate + 2, "P08", 3) == 0)
                        MORDOR_THROW_EXCEPTION(AmbiguousParameterException(message));
                    else if (strncmp(sqlstate + 2, "P09", 3) == 0)
                        MORDOR_THROW_EXCEPTION(AmbiguousAliasException(message));
                    else if (strncmp(sqlstate + 2, "P10", 3) == 0)
                        MORDOR_THROW_EXCEPTION(InvalidColumnReferenceException(message));
                    else if (strncmp(sqlstate + 2, "P11", 3) == 0)
                        MORDOR_THROW_EXCEPTION(InvalidColumnDefinitionException(message));
                    else if (strncmp(sqlstate + 2, "P12", 3) == 0)
                        MORDOR_THROW_EXCEPTION(InvalidDatabaseDefinitionException(message));
                    else if (strncmp(sqlstate + 2, "P13", 3) == 0)
                        MORDOR_THROW_EXCEPTION(InvalidFunctionDefinitionException(message));
                    else if (strncmp(sqlstate + 2, "P14", 3) == 0)
                        MORDOR_THROW_EXCEPTION(InvalidPreparedStatementDefinitionException(message));
                    else if (strncmp(sqlstate + 2, "P15", 3) == 0)
                        MORDOR_THROW_EXCEPTION(InvalidSchemaDefinitionException(message));
                    else if (strncmp(sqlstate + 2, "P16", 3) == 0)
                        MORDOR_THROW_EXCEPTION(InvalidTableDefinitionException(message));
                    else if (strncmp(sqlstate + 2, "P17", 3) == 0)
                        MORDOR_THROW_EXCEPTION(InvalidObjectDefinitionException(message));
                    else
                        MORDOR_THROW_EXCEPTION(AccessRuleViolationException(message));
                default:
                    break;
            }
        case '5':
            switch (sqlstate[1]) {
                case '3':
                    if (strncmp(sqlstate + 2, "000", 3) == 0)
                        MORDOR_THROW_EXCEPTION(InsufficientResourcesException(message));
                    else if (strncmp(sqlstate + 2, "100", 3) == 0)
                        MORDOR_THROW_EXCEPTION(DiskFullException(message));
                    else if (strncmp(sqlstate + 2, "200", 3) == 0)
                        MORDOR_THROW_EXCEPTION(OutOfMemoryException(message));
                    else if (strncmp(sqlstate + 2, "300", 3) == 0)
                        MORDOR_THROW_EXCEPTION(TooManyConnectionsException(message));
                    else
                        MORDOR_THROW_EXCEPTION(InsufficientResourcesException(message));
                case '4':
                    if (strncmp(sqlstate + 2, "000", 3) == 0)
                        MORDOR_THROW_EXCEPTION(ProgramLimitExceededException(message));
                    else if (strncmp(sqlstate + 2, "001", 3) == 0)
                        MORDOR_THROW_EXCEPTION(StatementTooComplexException(message));
                    else if (strncmp(sqlstate + 2, "011", 3) == 0)
                        MORDOR_THROW_EXCEPTION(TooManyColumnsException(message));
                    else if (strncmp(sqlstate + 2, "023", 3) == 0)
                        MORDOR_THROW_EXCEPTION(TooManyArgumentsException(message));
                    else
                        MORDOR_THROW_EXCEPTION(ProgramLimitExceededException(message));
                case '7':
                    if (strncmp(sqlstate + 2, "000", 3) == 0)
                        MORDOR_THROW_EXCEPTION(OperatorInterventionException(message));
                    else if (strncmp(sqlstate + 2, "014", 3) == 0)
                        MORDOR_THROW_EXCEPTION(QueryCanceledException(message));
                    else if (strncmp(sqlstate + 2, "P01", 3) == 0)
                        MORDOR_THROW_EXCEPTION(AdminShutdownException(message));
                    else if (strncmp(sqlstate + 2, "P02", 3) == 0)
                        MORDOR_THROW_EXCEPTION(CrashShutdownException(message));
                    else if (strncmp(sqlstate + 2, "P03", 3) == 0)
                        MORDOR_THROW_EXCEPTION(CannotConnectNowException(message));
                    else
                        MORDOR_THROW_EXCEPTION(OperatorInterventionException(message));
                case '8':
                    if (strncmp(sqlstate + 2, "030", 3) == 0)
                        MORDOR_THROW_EXCEPTION(IOError(message));
                    else if (strncmp(sqlstate + 2, "P01", 3) == 0)
                        MORDOR_THROW_EXCEPTION(UndefinedFileException(message));
                    else if (strncmp(sqlstate + 2, "P02", 3) == 0)
                        MORDOR_THROW_EXCEPTION(DuplicateFileException(message));
                    else
                        MORDOR_THROW_EXCEPTION(SystemError(message));
                default:
                    break;
            }
        case 'F':
            switch (sqlstate[1]) {
                case '0':
                    if (strncmp(sqlstate + 2, "000", 3) == 0)
                        MORDOR_THROW_EXCEPTION(ConfigFileError(message));
                    else if (strncmp(sqlstate + 2, "001", 3) == 0)
                        MORDOR_THROW_EXCEPTION(LockFileExistsException(message));
                    else
                        MORDOR_THROW_EXCEPTION(ConfigFileError(message));
                default:
                    break;
            }
        case 'X':
            switch (sqlstate[1]) {
                case 'X':
                    if (strncmp(sqlstate + 2, "000", 3) == 0)
                        MORDOR_THROW_EXCEPTION(InternalError(message));
                    else if (strncmp(sqlstate + 2, "001", 3) == 0)
                        MORDOR_THROW_EXCEPTION(DataCorruptedException(message));
                    else if (strncmp(sqlstate + 2, "002", 3) == 0)
                        MORDOR_THROW_EXCEPTION(IndexCorruptedException(message));
                    else
                        MORDOR_THROW_EXCEPTION(InternalError(message));
                default:
                    break;
            }
        default:
            break;
    }
    MORDOR_THROW_EXCEPTION(Exception(message));
}

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

static std::string escape(PGconn *conn, const std::string &string)
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
static void flush(PGconn *conn, SchedulerType *scheduler)
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

static PGresult *nextResult(PGconn *conn, SchedulerType *scheduler)
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
Connection::prepare(const std::string &command, const std::string &name)
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
            return PreparedStatement(m_conn, std::string(), name, m_scheduler);
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
            return PreparedStatement(m_conn, std::string(), name, m_scheduler);
        }
    } else {
        return PreparedStatement(m_conn, command, name, m_scheduler);
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
            length = std::min(m_readBuffer.readAvailable(), length);
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

        length = std::min(m_readBuffer.readAvailable(), length);
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
PreparedStatement::bind(size_t param, const boost::posix_time::ptime &value)
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
    setType(param, TIMESTAMPOID);
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
                nParams, paramTypes, params, paramLengths, paramFormats, 1))
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
                nParams, paramTypes, params, paramLengths, paramFormats, 1),
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
                nParams, params, paramLengths, paramFormats, 1),
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

size_t
Result::rows() const
{
    return (size_t)PQntuples(m_result.get());
}

size_t
Result::columns() const
{
    return (size_t)PQnfields(m_result.get());
}

size_t
Result::column(const char *name) const
{
    return (size_t)PQfnumber(m_result.get(), name);
}

Oid
Result::getType(size_t column) const
{
    return PQftype(m_result.get(), (int)column);
}

bool Result::getIsNull(size_t row, size_t column) const {
    return PQgetisnull(m_result.get(), (int)row, (int)column) == 0;
}
template <>
std::string
Result::get<std::string>(size_t row, size_t column) const
{
    return std::string(PQgetvalue(m_result.get(), (int)row, (int)column),
        PQgetlength(m_result.get(), (int)row, (int)column));
}

template <>
const char *
Result::get<const char *>(size_t row, size_t column) const
{
    return PQgetvalue(m_result.get(), (int)row, (int)column);
}

template <>
bool
Result::get<bool>(size_t row, size_t column) const
{
    MORDOR_ASSERT(getType(column) == BOOLOID);
    MORDOR_ASSERT(PQgetlength(m_result.get(), (int)row, (int)column) == 1);
    return !!*PQgetvalue(m_result.get(), (int)row, (int)column);
}

template <>
char
Result::get<char>(size_t row, size_t column) const
{
    MORDOR_ASSERT(getType(column) == CHAROID);
    MORDOR_ASSERT(PQgetlength(m_result.get(), (int)row, (int)column) == 1);
    return *PQgetvalue(m_result.get(), (int)row, (int)column);
}

template <>
long long
Result::get<long long>(size_t row, size_t column) const
{
    switch (getType(column)) {
        case INT8OID:
            MORDOR_ASSERT(PQgetlength(m_result.get(), (int)row, (int)column) == 8);
            return byteswapOnLittleEndian(*(long long *)PQgetvalue(m_result.get(), (int)row, (int)column));
        case INT2OID:
            MORDOR_ASSERT(PQgetlength(m_result.get(), (int)row, (int)column) == 2);
            return byteswapOnLittleEndian(*(short *)PQgetvalue(m_result.get(), (int)row, (int)column));
        case INT4OID:
            MORDOR_ASSERT(PQgetlength(m_result.get(), (int)row, (int)column) == 4);
            return byteswapOnLittleEndian(*(int *)PQgetvalue(m_result.get(), (int)row, (int)column));
        default:
            MORDOR_NOTREACHED();
    }
}

template <>
short
Result::get<short>(size_t row, size_t column) const
{
    MORDOR_ASSERT(getType(column) == INT2OID);
    MORDOR_ASSERT(PQgetlength(m_result.get(), (int)row, (int)column) == 2);
    return byteswapOnLittleEndian(*(short *)PQgetvalue(m_result.get(), (int)row, (int)column));
}

template <>
int
Result::get<int>(size_t row, size_t column) const
{
    switch (getType(column)) {
        case INT2OID:
            MORDOR_ASSERT(PQgetlength(m_result.get(), (int)row, (int)column) == 2);
            return byteswapOnLittleEndian(*(short *)PQgetvalue(m_result.get(), (int)row, (int)column));
        case INT4OID:
            MORDOR_ASSERT(PQgetlength(m_result.get(), (int)row, (int)column) == 4);
            return byteswapOnLittleEndian(*(int *)PQgetvalue(m_result.get(), (int)row, (int)column));
        default:
            MORDOR_NOTREACHED();
    }
}

template <>
float
Result::get<float>(size_t row, size_t column) const
{
    MORDOR_ASSERT(getType(column) == FLOAT4OID);
    MORDOR_ASSERT(PQgetlength(m_result.get(), (int)row, (int)column) == 4);
    int temp = byteswapOnLittleEndian(*(int *)PQgetvalue(m_result.get(), (int)row, (int)column));
    return *(float *)&temp;
}

template <>
double
Result::get<double>(size_t row, size_t column) const
{
    int templ;
    long long templl;
    switch (getType(column)) {
        case FLOAT4OID:
            MORDOR_ASSERT(PQgetlength(m_result.get(), (int)row, (int)column) == 4);
            templ = byteswapOnLittleEndian(*(int *)PQgetvalue(m_result.get(), (int)row, (int)column));
            return *(float *)&templ;
        case FLOAT8OID:
            MORDOR_ASSERT(PQgetlength(m_result.get(), (int)row, (int)column) == 8);
            templl = byteswapOnLittleEndian(*(long long *)PQgetvalue(m_result.get(), (int)row, (int)column));
            return *(double *)&templl;
        default:
            MORDOR_NOTREACHED();
    }
}

template<>
boost::posix_time::ptime
Result::get<boost::posix_time::ptime>(size_t row, size_t column) const
{
    MORDOR_ASSERT(getType(column) == TIMESTAMPOID ||
        getType(column) == TIMESTAMPTZOID);
    if (PQgetlength(m_result.get(), (int)row, (int)column) == 0)
        return boost::posix_time::ptime();
    MORDOR_ASSERT(PQgetlength(m_result.get(), (int)row, (int)column) == 8);
    long long microseconds = byteswapOnLittleEndian(*(long long *)PQgetvalue(m_result.get(), (int)row, (int)column));
    return postgres_epoch +
        boost::posix_time::seconds((long)(microseconds / 1000000)) +
        boost::posix_time::microseconds(microseconds % 1000000);
}

Transaction::Transaction(Connection &connection, IsolationLevel isolationLevel)
: m_connection(connection),
  m_active(true)
{
    switch (isolationLevel) {
        case DEFAULT:
            m_connection.execute("BEGIN");
            break;
        case SERIALIZABLE:
            m_connection.execute("BEGIN ISOLATION LEVEL SERIALIZABLE");
            break;
        case READ_COMMITTED:
            m_connection.execute("BEGIN ISOLATION LEVEL READ COMMITTED");
            break;
        default:
            MORDOR_NOTREACHED();
    }
}

Transaction::Transaction(Connection &connection, IsolationLevel isolationLevel,
                         Mode mode)
: m_connection(connection),
  m_active(true)
{
    switch (isolationLevel) {
        case DEFAULT:
            switch (mode) {
                case READ_WRITE:
                    m_connection.execute("BEGIN READ WRITE");
                    break;
                case READ_ONLY:
                    m_connection.execute("BEGIN READ ONLY");
                    break;
                default:
                    MORDOR_NOTREACHED();
            }
            break;
        case SERIALIZABLE:
            switch (mode) {
                case READ_WRITE:
                    m_connection.execute("BEGIN ISOLATION LEVEL SERIALIZABLE READ WRITE");
                    break;
                case READ_ONLY:
                    m_connection.execute("BEGIN ISOLATION LEVEL SERIALIZABLE READ ONLY");
                    break;
                default:
                    MORDOR_NOTREACHED();
            }
            break;
        case READ_COMMITTED:
            switch (mode) {
                case READ_WRITE:
                    m_connection.execute("BEGIN ISOLATION LEVEL READ COMMITTED READ WRITE");
                    break;
                case READ_ONLY:
                    m_connection.execute("BEGIN ISOLATION LEVEL READ COMMITTED READ ONLY");
                    break;
                default:
                    MORDOR_NOTREACHED();
            }
            break;
        default:
            MORDOR_NOTREACHED();
    }
}

Transaction::~Transaction()
{
    if (m_active) {
        try {
            m_connection.execute("ROLLBACK");
        } catch (...) {
            // No exceptions in destructors
        }
    }
}

void
Transaction::commit()
{
    MORDOR_ASSERT(m_active);
    m_connection.execute("COMMIT");
    m_active = false;
}

void
Transaction::rollback()
{
    MORDOR_ASSERT(m_active);
    m_connection.execute("ROLLBACK");
    m_active = false;
}

}}
