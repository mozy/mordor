#ifndef __MORDOR_PQ_H__
#define __MORDOR_PQ_H__
// Copyright (c) 2010 Mozy, Inc.

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include <postgresql/libpq-fe.h>

#include "mordor/exception.h"
#include "mordor/iomanager.h"

namespace Mordor {
namespace PQ {

struct Exception : virtual ::Mordor::Exception
{
    Exception(const std::string &message)
        : m_message(message)
    {}
    Exception()
    {}
    ~Exception() throw () {}

    const char *what() const throw () { return m_message.c_str(); }
private:
    std::string m_message;
};
struct BadConnectionException : virtual Exception {};

#define DEFINE_MORDOR_PQ_EXCEPTION(Exception, Base)                             \
struct Exception : Base                                                         \
{                                                                               \
    Exception(const std::string &message)                                       \
        : Base(message)                                                         \
    {}                                                                          \
};

DEFINE_MORDOR_PQ_EXCEPTION(DataException, Exception);
DEFINE_MORDOR_PQ_EXCEPTION(ArraySubscriptError, DataException);
DEFINE_MORDOR_PQ_EXCEPTION(CharacterNotInRepertoireException, DataException);
DEFINE_MORDOR_PQ_EXCEPTION(OverflowException, DataException);
DEFINE_MORDOR_PQ_EXCEPTION(DivisionByZeroException, DataException);
DEFINE_MORDOR_PQ_EXCEPTION(AssignmentError, DataException);
DEFINE_MORDOR_PQ_EXCEPTION(EscapeCharacterConflictException, DataException);
DEFINE_MORDOR_PQ_EXCEPTION(InvalidArgumentException, DataException);
DEFINE_MORDOR_PQ_EXCEPTION(InvalidCharacterValueForCastException, DataException);

DEFINE_MORDOR_PQ_EXCEPTION(IntegrityConstraintViolationException, Exception)
DEFINE_MORDOR_PQ_EXCEPTION(RestrictViolationException, IntegrityConstraintViolationException)
DEFINE_MORDOR_PQ_EXCEPTION(NotNullViolationException, IntegrityConstraintViolationException)
DEFINE_MORDOR_PQ_EXCEPTION(ForeignKeyViolationException, IntegrityConstraintViolationException)
DEFINE_MORDOR_PQ_EXCEPTION(UniqueViolationException, IntegrityConstraintViolationException)
DEFINE_MORDOR_PQ_EXCEPTION(CheckViolationException, IntegrityConstraintViolationException)

DEFINE_MORDOR_PQ_EXCEPTION(InvalidTransactionStateException, Exception);
DEFINE_MORDOR_PQ_EXCEPTION(ActiveTransactionException, InvalidTransactionStateException);
DEFINE_MORDOR_PQ_EXCEPTION(BranchTransactionAlreadyActiveException, InvalidTransactionStateException);
DEFINE_MORDOR_PQ_EXCEPTION(HeldCursorRequiresSameIsolationLevelException, InvalidTransactionStateException);
DEFINE_MORDOR_PQ_EXCEPTION(InappropriateAccessModeForBranchTransactionException, InvalidTransactionStateException);
DEFINE_MORDOR_PQ_EXCEPTION(InappropriateIsolationLevelForBranchTransactionException, InvalidTransactionStateException);
DEFINE_MORDOR_PQ_EXCEPTION(NoActiveTransactionForBranchException, InvalidTransactionStateException);
DEFINE_MORDOR_PQ_EXCEPTION(ReadOnlyTransactionException, InvalidTransactionStateException);
DEFINE_MORDOR_PQ_EXCEPTION(SchemaAndDataStatementMixingNotSupportedException, InvalidTransactionStateException);
DEFINE_MORDOR_PQ_EXCEPTION(NoActiveTransactionException, InvalidTransactionStateException);
DEFINE_MORDOR_PQ_EXCEPTION(InFailedTransactionException, InvalidTransactionStateException);

DEFINE_MORDOR_PQ_EXCEPTION(TransactionRollbackException, Exception);
DEFINE_MORDOR_PQ_EXCEPTION(TransactionIntegrityConstraintViolationException, TransactionRollbackException);
DEFINE_MORDOR_PQ_EXCEPTION(SerializationFailureException, TransactionRollbackException);
DEFINE_MORDOR_PQ_EXCEPTION(StatementCompletionUnknownException, TransactionRollbackException);
DEFINE_MORDOR_PQ_EXCEPTION(DeadlockDetectedException, TransactionRollbackException);

DEFINE_MORDOR_PQ_EXCEPTION(AccessRuleViolationException, Exception);
DEFINE_MORDOR_PQ_EXCEPTION(SyntaxError, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(InsufficientPrivilegeException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(CannotCoerceException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(GroupingError, SyntaxError);
DEFINE_MORDOR_PQ_EXCEPTION(WindowingError, SyntaxError);
DEFINE_MORDOR_PQ_EXCEPTION(InvalidRecursionException, SyntaxError);
DEFINE_MORDOR_PQ_EXCEPTION(InvalidForeignKeyException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(InvalidNameException, SyntaxError);
DEFINE_MORDOR_PQ_EXCEPTION(NameTooLongException, SyntaxError);
DEFINE_MORDOR_PQ_EXCEPTION(ReservedNameException, SyntaxError);
DEFINE_MORDOR_PQ_EXCEPTION(DatatypeMismatchException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(IndeterminateDatatypeException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(WrongObjectTypeException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(UndefinedColumnException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(UndefinedFunctionException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(UndefinedTableException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(UndefinedParameterException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(UndefinedObjectException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(DuplicateColumnException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(DuplicateCursorException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(DuplicateDatabaseException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(DuplicateFunctionException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(DuplicatePreparedStatementException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(DuplicateSchemaException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(DuplicateTableException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(DuplicateAliasException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(DuplicateObjectException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(AmbiguousColumnException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(AmbiguousFunctionException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(AmbiguousParameterException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(AmbiguousAliasException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(InvalidColumnReferenceException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(InvalidColumnDefinitionException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(InvalidCursorDefinitionException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(InvalidDatabaseDefinitionException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(InvalidFunctionDefinitionException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(InvalidPreparedStatementDefinitionException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(InvalidSchemaDefinitionException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(InvalidTableDefinitionException, AccessRuleViolationException);
DEFINE_MORDOR_PQ_EXCEPTION(InvalidObjectDefinitionException, AccessRuleViolationException);

DEFINE_MORDOR_PQ_EXCEPTION(InsufficientResourcesException, Exception);
DEFINE_MORDOR_PQ_EXCEPTION(DiskFullException, InsufficientResourcesException);
DEFINE_MORDOR_PQ_EXCEPTION(OutOfMemoryException, InsufficientResourcesException);
DEFINE_MORDOR_PQ_EXCEPTION(TooManyConnectionsException, InsufficientResourcesException);

DEFINE_MORDOR_PQ_EXCEPTION(ProgramLimitExceededException, Exception);
DEFINE_MORDOR_PQ_EXCEPTION(StatementTooComplexException, ProgramLimitExceededException);
DEFINE_MORDOR_PQ_EXCEPTION(TooManyColumnsException, ProgramLimitExceededException);
DEFINE_MORDOR_PQ_EXCEPTION(TooManyArgumentsException, ProgramLimitExceededException);

DEFINE_MORDOR_PQ_EXCEPTION(OperatorInterventionException, Exception);
DEFINE_MORDOR_PQ_EXCEPTION(QueryCanceledException, OperatorInterventionException);
DEFINE_MORDOR_PQ_EXCEPTION(AdminShutdownException, OperatorInterventionException);
DEFINE_MORDOR_PQ_EXCEPTION(CrashShutdownException, OperatorInterventionException);
DEFINE_MORDOR_PQ_EXCEPTION(CannotConnectNowException, OperatorInterventionException);

DEFINE_MORDOR_PQ_EXCEPTION(SystemError, Exception);
DEFINE_MORDOR_PQ_EXCEPTION(IOError, SystemError);
DEFINE_MORDOR_PQ_EXCEPTION(UndefinedFileException, SystemError);
DEFINE_MORDOR_PQ_EXCEPTION(DuplicateFileException, SystemError);

DEFINE_MORDOR_PQ_EXCEPTION(ConfigFileError, Exception);
DEFINE_MORDOR_PQ_EXCEPTION(LockFileExistsException, ConfigFileError);

DEFINE_MORDOR_PQ_EXCEPTION(InternalError, Exception);
DEFINE_MORDOR_PQ_EXCEPTION(DataCorruptedException, InternalError);
DEFINE_MORDOR_PQ_EXCEPTION(IndexCorruptedException, InternalError);

class PreparedStatement;

class Result
{
    friend class Connection;
    friend class PreparedStatement;
private:
    Result(boost::shared_ptr<PGresult> result)
        : m_result(result)
    {}

public:
    size_t rows() const;
    size_t columns() const;

    size_t column(const char *name) const;
    size_t column(const std::string &name) const
    { return column(name.c_str()); }

    Oid getType(size_t column) const;
    Oid getType(const char *col) const
    { return getType(column(col)); }
    Oid getType(const std::string &col) const
    { return getType(column(col)); }

    bool getIsNull(size_t row, size_t column) const;
    bool getIsNull(size_t row, const char *col) const
    { return getIsNull(row, column(col)); }
    bool getIsNull(size_t row, const std::string &col) const
    { return getIsNull(row, column(col)); }

    template <class T> T get(size_t row, size_t column) const;

private:
    boost::shared_ptr<PGresult> m_result;
};

struct Null {};
struct Skip {};

class PreparedStatement
{
    friend class Connection;
private:
    PreparedStatement(boost::shared_ptr<PGconn> conn,
        const std::string &command, const std::string &name,
        IOManager *ioManager)
        : m_conn(conn),
          m_command(command),
          m_name(name),
          m_ioManager(ioManager)
    {}

public:
    void bind(size_t param, const Null &);
    void bind(size_t param, const char *value);
    void bind(size_t param, const std::string &value);
    void bind(size_t param, bool value);
    void bind(size_t param, char value);
    void bind(size_t param, short value);
    void bind(size_t param, int value);
    void bind(size_t param, long long value);
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
    IOManager *m_ioManager;
    std::vector<Oid> m_paramTypes;
    std::vector<std::string> m_paramValues;
    std::vector<const char *> m_params;
    std::vector<int> m_paramLengths, m_paramFormats;
};

class Connection : boost::noncopyable
{
public:
    Connection(const std::string &conninfo, IOManager *ioManager, bool connectImmediately = true);

    ConnStatusType status();

    void connect();
    /// @brief Resets the communication channel to the server.
    /// This function will close the connection to the server and attempt to
    /// reestablish a new connection to the same server, using all the same
    /// parameters previously used. This may be useful for error recovery if a
    /// working connection is lost.
    void reset();

    /// @param name If non-empty, specifies to prepare this command on the
    /// server.  Statements prepared on the server msut have unique names
    /// (per-connection)
    PreparedStatement prepare(const std::string &command,
        const std::string &name = std::string());
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

private:
    const std::string &m_conninfo;
    IOManager *m_ioManager;
    boost::shared_ptr<PGconn> m_conn;
};

}}

#endif
