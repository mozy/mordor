// Copyright (c) 2010 - Mozy, Inc.

#include "mordor/predef.h"

#include "exception.h"

#include "mordor/log.h"

namespace Mordor {
namespace PQ {

static Logger::ptr g_log = Log::lookup("mordor:pq");

void throwException(PGconn *conn)
{
    const char *error = PQerrorMessage(conn);
    MORDOR_LOG_ERROR(g_log) << conn << " connection error: " << error;
    MORDOR_THROW_EXCEPTION(ConnectionException(error));
}

void throwException(PGresult *result)
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

}}
