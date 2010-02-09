// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "pq.h"

#include "assert.h"
#include "endian.h"
#include "log.h"

#define BOOLOID 16
#define CHAROID 18
#define INT8OID 20
#define INT2OID 21
#define INT4OID 23

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

Connection::Connection(const std::string &conninfo, IOManager *ioManager, bool connectImmediately)
: m_conninfo(conninfo),
  m_ioManager(ioManager)
{
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
    if (m_ioManager) {
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
                    m_ioManager->registerEvent(fd, IOManager::READ);
                    Scheduler::getThis()->yieldTo();
                    break;
                case PGRES_POLLING_WRITING:
                    m_ioManager->registerEvent(fd, IOManager::WRITE);
                    Scheduler::getThis()->yieldTo();
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
    } else {
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
    if (m_ioManager) {
        if (!PQresetStart(m_conn.get()))
            throwException(m_conn.get());
        int fd = PQsocket(m_conn.get());
        PostgresPollingStatusType whatToPoll = PGRES_POLLING_WRITING;
        while (true) {
            MORDOR_LOG_DEBUG(g_log) << m_conn.get() << " PQresetPoll(): "
                << whatToPoll;
            switch (whatToPoll) {
                case PGRES_POLLING_READING:
                    m_ioManager->registerEvent(fd, IOManager::READ);
                    Scheduler::getThis()->yieldTo();
                    break;
                case PGRES_POLLING_WRITING:
                    m_ioManager->registerEvent(fd, IOManager::WRITE);
                    Scheduler::getThis()->yieldTo();
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
    } else {
        PQreset(m_conn.get());
        if (status() == CONNECTION_BAD)
            throwException(m_conn.get());
    }
    MORDOR_LOG_INFO(g_log) << m_conn.get() << " PQreset()";
}

static void flush(PGconn *conn, IOManager *ioManager)
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
                ioManager->registerEvent(PQsocket(conn), IOManager::WRITE);
                Scheduler::getThis()->yieldTo();
                continue;
            default:
                MORDOR_NOTREACHED();
        }
    }
}

static PGresult *nextResult(PGconn *conn, IOManager *ioManager)
{
    while (true) {
        if (!PQconsumeInput(conn))
            throwException(conn);
        if (PQisBusy(conn)) {
            MORDOR_LOG_DEBUG(g_log) << conn << " PQisBusy()";
            ioManager->registerEvent(PQsocket(conn), IOManager::READ);
            Scheduler::getThis()->yieldTo();
            continue;
        }
        MORDOR_LOG_DEBUG(g_log) << conn << " PQconsumeInput()";
        return PQgetResult(conn);
    }
}

PreparedStatement
Connection::prepare(const std::string &command, const std::string &name)
{
    if (!name.empty()) {
        if (m_ioManager) {
            if (!PQsendPrepare(m_conn.get(), name.c_str(), command.c_str(), 0, NULL))
                throwException(m_conn.get());
            flush(m_conn.get(), m_ioManager);
            boost::shared_ptr<PGresult> result(nextResult(m_conn.get(), m_ioManager),
                &PQclear);
            while (result) {
                ExecStatusType status = PQresultStatus(result.get());
                MORDOR_LOG_DEBUG(g_log) << m_conn.get() << " PQresultStatus("
                    << result.get() << "): " << PQresStatus(status);
                if (status != PGRES_COMMAND_OK)
                    throwException(result.get());
                result.reset(nextResult(m_conn.get(), m_ioManager),
                    &PQclear);
            }
            MORDOR_LOG_VERBOSE(g_log) << m_conn.get() << " PQsendPrepare(\""
                << name << "\", \"" << command << "\")";
            return PreparedStatement(m_conn, std::string(), name, m_ioManager);
        } else {
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
            return PreparedStatement(m_conn, std::string(), name, m_ioManager);
        }
    } else {
        return PreparedStatement(m_conn, command, name, m_ioManager);
    }
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
    *(short *)&m_paramValues[param - 1][0] = htons(value);
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
    *(int *)&m_paramValues[param - 1][0] = htonl(value);
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
    *(long long *)&m_paramValues[param - 1][0] = htonll(value);
    m_params[param - 1] = m_paramValues[param - 1].c_str();
    m_paramLengths[param - 1] = m_paramValues[param - 1].size();
    m_paramFormats[param - 1] = 1;
    setType(param, INT8OID);
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
        paramTypes = &m_paramTypes[0];
        params = &m_params[0];
        paramLengths = &m_paramLengths[0];
        paramFormats = &m_paramFormats[0];
    }
    const char *api = NULL;
    if (m_name.empty()) {
        if (m_ioManager) {
            api = "PQsendQueryParams";
            if (!PQsendQueryParams(conn, m_command.c_str(),
                nParams, paramTypes, params, paramLengths, paramFormats, 1))
                throwException(conn);
            flush(conn, m_ioManager);
            next.reset(nextResult(conn, m_ioManager), &PQclear);
            while (next) {
                result = next;
                next.reset(nextResult(conn, m_ioManager), &PQclear);
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
        } else {
            api = "PQexecParams";
            result.reset(PQexecParams(conn, m_command.c_str(),
                nParams, paramTypes, params, paramLengths, paramFormats, 1),
                &PQclear);
        }
    } else {
        if (m_ioManager) {
            api = "PQsendQueryPrepared";
            if (!PQsendQueryPrepared(conn, m_name.c_str(),
                nParams, params, paramLengths, paramFormats, 1))
                throwException(conn);
            flush(conn, m_ioManager);
            next.reset(nextResult(conn, m_ioManager), &PQclear);
            while (next) {
                result = next;
                next.reset(nextResult(conn, m_ioManager), &PQclear);
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
        } else {
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
    return !!*PQgetvalue(m_result.get(), (int)row, (int)column);
}

template <>
char
Result::get<char>(size_t row, size_t column) const
{
    MORDOR_ASSERT(getType(column) == CHAROID);
    return *PQgetvalue(m_result.get(), (int)row, (int)column);
}

template <>
long long
Result::get<long long>(size_t row, size_t column) const
{
    switch (getType(column)) {
        case INT8OID:
            return htonll(*(long long *)PQgetvalue(m_result.get(), (int)row, (int)column));
        case INT2OID:
            return htons(*(short *)PQgetvalue(m_result.get(), (int)row, (int)column));
        case INT4OID:
            return htonl(*(int *)PQgetvalue(m_result.get(), (int)row, (int)column));
        default:
            MORDOR_NOTREACHED();
    }
}

template <>
short
Result::get<short>(size_t row, size_t column) const
{
    MORDOR_ASSERT(getType(column) == INT2OID);
    return htons(*(short *)PQgetvalue(m_result.get(), (int)row, (int)column));
}

template <>
int
Result::get<int>(size_t row, size_t column) const
{
    switch (getType(column)) {
        case INT2OID:
            return htons(*(short *)PQgetvalue(m_result.get(), (int)row, (int)column));
        case INT4OID:
            return htonl(*(int *)PQgetvalue(m_result.get(), (int)row, (int)column));
        default:
            MORDOR_NOTREACHED();
    }
}

}}
