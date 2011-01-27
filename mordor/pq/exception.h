#ifndef __MORDOR_PQ_EXCEPTION_H__
#define __MORDOR_PQ_EXCEPTION_H__
// Copyright (c) 2010 - Mozy, Inc.

#include <libpq-fe.h>

#include "mordor/exception.h"

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

#define DEFINE_MORDOR_PQ_EXCEPTION(Exception, Base)                             \
struct Exception : Base                                                         \
{                                                                               \
    Exception()                                                                 \
    {}                                                                          \
    Exception(const std::string &message)                                       \
        : Base(message)                                                         \
    {}                                                                          \
};

DEFINE_MORDOR_PQ_EXCEPTION(ConnectionException, Exception);

DEFINE_MORDOR_PQ_EXCEPTION(DataException, Exception);
DEFINE_MORDOR_PQ_EXCEPTION(ArraySubscriptError, DataException);
DEFINE_MORDOR_PQ_EXCEPTION(CharacterNotInRepertoireException, DataException);
DEFINE_MORDOR_PQ_EXCEPTION(OverflowException, DataException);
DEFINE_MORDOR_PQ_EXCEPTION(DivisionByZeroException, DataException);
DEFINE_MORDOR_PQ_EXCEPTION(AssignmentError, DataException);
DEFINE_MORDOR_PQ_EXCEPTION(EscapeCharacterConflictException, DataException);
DEFINE_MORDOR_PQ_EXCEPTION(InvalidArgumentException, DataException);
DEFINE_MORDOR_PQ_EXCEPTION(NullValueNotAllowedException, DataException);

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

void throwException(PGconn *conn);
void throwException(PGresult *result);

}}

#endif
