/***********************************************************************************************************************************
Postgres Client
***********************************************************************************************************************************/
#include "build.auto.h"

#include <libpq-fe.h>

#include "common/debug.h"
#include "common/io/fd.h"
#include "common/log.h"
#include "common/wait.h"
#include "postgres/client.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct PgClient
{
    PgClientPub pub;                                                // Publicly accessible variables
    PGconn *connection;                                             // Pg connection
};

/***********************************************************************************************************************************
Close protocol connection
***********************************************************************************************************************************/
static void
pgClientFreeResource(THIS_VOID)
{
    THIS(PgClient);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(PG_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    PQfinish(this->connection);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN PgClient *
pgClientNew(
    const String *const host, const unsigned int port, const String *const database, const String *const user,
    const TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(STRING, database);
        FUNCTION_LOG_PARAM(STRING, user);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    ASSERT(port >= 1 && port <= 65535);
    ASSERT(database != NULL);

    OBJ_NEW_BEGIN(PgClient, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        *this = (PgClient)
        {
            .pub =
            {
                .host = strDup(host),
                .port = port,
                .database = strDup(database),
                .user = strDup(user),
                .timeout = timeout,
            },
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(PG_CLIENT, this);
}

/***********************************************************************************************************************************
Just ignore notices and warnings
***********************************************************************************************************************************/
static void
pgClientNoticeProcessor(void *const arg, const char *const message)
{
    (void)arg;
    (void)message;
}

/***********************************************************************************************************************************
Encode string to escape ' and \
***********************************************************************************************************************************/
static String *
pgClientEscape(const String *const string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    ASSERT(string != NULL);

    String *const result = strCatZ(strNew(), "'");

    // Iterate all characters in the string
    for (unsigned stringIdx = 0; stringIdx < strSize(string); stringIdx++)
    {
        char stringChar = strZ(string)[stringIdx];

        // These characters are escaped
        if (stringChar == '\'' || stringChar == '\\')
            strCatChr(result, '\\');

        strCatChr(result, stringChar);
    }

    strCatChr(result, '\'');

    FUNCTION_TEST_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN PgClient *
pgClientOpen(PgClient *const this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PG_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    CHECK(AssertError, this->connection == NULL, "invalid connection");

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Base connection string
        String *const connInfo = strCatFmt(
            strNew(), "dbname=%s port=%u", strZ(pgClientEscape(pgClientDatabase(this))), pgClientPort(this));

        // Add user if specified
        if (pgClientUser(this) != NULL)
            strCatFmt(connInfo, " user=%s", strZ(pgClientEscape(pgClientUser(this))));

        // Add host if specified
        if (pgClientHost(this) != NULL)
            strCatFmt(connInfo, " host=%s", strZ(pgClientEscape(pgClientHost(this))));

        // Make the connection
        this->connection = PQconnectdb(strZ(connInfo));

        // Set a callback to shutdown the connection
        memContextCallbackSet(objMemContext(this), pgClientFreeResource, this);

        // Handle errors
        if (PQstatus(this->connection) != CONNECTION_OK)
        {
            THROW_FMT(
                DbConnectError, "unable to connect to '%s': %s", strZ(connInfo),
                strZ(strTrim(strNewZ(PQerrorMessage(this->connection)))));
        }

        // Set notice and warning processor
        PQsetNoticeProcessor(this->connection, pgClientNoticeProcessor, NULL);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PG_CLIENT, this);
}

/**********************************************************************************************************************************/
FN_EXTERN Pack *
pgClientQuery(PgClient *const this, const String *const query, const PgClientQueryResult resultType)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PG_CLIENT, this);
        FUNCTION_LOG_PARAM(STRING, query);
        FUNCTION_LOG_PARAM(STRING_ID, resultType);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    CHECK(AssertError, this->connection != NULL, "invalid connection");
    ASSERT(query != NULL);
    ASSERT(resultType != 0);

    Pack *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Send the query without waiting for results so we can timeout if needed
        if (!PQsendQuery(this->connection, strZ(query)))
        {
            THROW_FMT(
                DbQueryError, "unable to send query '%s': %s", strZ(query),
                strZ(strTrim(strNewZ(PQerrorMessage(this->connection)))));
        }

        // Wait for a result
        Wait *const wait = waitNew(pgClientTimeout(this));
        bool busy = false;

        do
        {
            PQconsumeInput(this->connection);
            busy = PQisBusy(this->connection);
        }
        while (busy && fdReadyRead(PQsocket(this->connection), waitRemains(wait)));

        // If the query is still busy after the timeout attempt to cancel
        if (busy)
        {
            PGcancel *const cancel = PQgetCancel(this->connection);

            // If cancel is NULL then more than likely the server process crashed or disconnected
            if (cancel == NULL)
                THROW_FMT(DbQueryError, "unable to cancel query '%s': connection was lost", strZ(query));

            TRY_BEGIN()
            {
                char error[256];

                if (!PQcancel(cancel, error, sizeof(error)))
                    THROW_FMT(DbQueryError, "unable to cancel query '%s': %s", strZ(query), strZ(strTrim(strNewZ(error))));
            }
            FINALLY()
            {
                PQfreeCancel(cancel);
            }
            TRY_END();
        }

        // Get the result (even if query was cancelled -- to prevent the connection being left in a bad state)
        PGresult *const pgResult = PQgetResult(this->connection);

        TRY_BEGIN()
        {
            // Throw timeout error if cancelled
            if (busy)
                THROW_FMT(DbQueryError, "query '%s' timed out after %" PRIu64 "ms", strZ(query), pgClientTimeout(this));

            // If this was a command that returned no results then we are done
            const ExecStatusType resultStatus = PQresultStatus(pgResult);

            if (resultStatus == PGRES_COMMAND_OK)
            {
                if (resultType != pgClientQueryResultNone && resultType != pgClientQueryResultAny)
                    THROW_FMT(DbQueryError, "result expected from '%s'", strZ(query));
            }
            else
            {
                // If no tuples then the result is an error
                if (resultStatus != PGRES_TUPLES_OK)
                {
                    THROW_FMT(
                        DbQueryError, "unable to execute query '%s': %s", strZ(query),
                        strZ(strTrim(strNewZ(PQresultErrorMessage(pgResult)))));
                }

                // Expect some rows to be returned
                if (resultType == pgClientQueryResultNone)
                    THROW_FMT(DbQueryError, "no result expected from '%s'", strZ(query));

                // Fetch row and column values
                PackWrite *const pack = pckWriteNewP();

                const int rowTotal = PQntuples(pgResult);
                const int columnTotal = PQnfields(pgResult);

                if (resultType != pgClientQueryResultAny)
                {
                    if (rowTotal != 1)
                        THROW_FMT(DbQueryError, "expected one row from '%s'", strZ(query));

                    if (resultType == pgClientQueryResultColumn && columnTotal != 1)
                        THROW_FMT(DbQueryError, "expected one column from '%s'", strZ(query));
                }

                // Get column types
                Oid *const columnType = memNew(sizeof(int) * (size_t)columnTotal);

                for (int columnIdx = 0; columnIdx < columnTotal; columnIdx++)
                    columnType[columnIdx] = PQftype(pgResult, columnIdx);

                // Get values
                for (int rowIdx = 0; rowIdx < rowTotal; rowIdx++)
                {
                    if (resultType == pgClientQueryResultAny)
                        pckWriteArrayBeginP(pack);

                    for (int columnIdx = 0; columnIdx < columnTotal; columnIdx++)
                    {
                        const char *const value = PQgetvalue(pgResult, rowIdx, columnIdx);

                        // If value is zero-length then check if it is null
                        if (value[0] == '\0' && PQgetisnull(pgResult, rowIdx, columnIdx))
                        {
                            pckWriteNullP(pack);
                        }
                        // Else convert the value to a variant
                        else
                        {
                            // Convert column type. Not all PostgreSQL types are supported but these should suffice.
                            switch (columnType[columnIdx])
                            {
                                // Boolean type
                                case 16:                            // bool
                                    pckWriteBoolP(pack, varBoolForce(varNewStrZ(value)), .defaultWrite = true);
                                    break;

                                // Text/char types
                                case 18:                            // char
                                case 19:                            // name
                                case 25:                            // text
                                    pckWriteStrP(pack, STR(value), .defaultWrite = true);
                                    break;

                                // 64-bit integer type
                                case 20:                            // int8
                                    pckWriteI64P(pack, cvtZToInt64(value), .defaultWrite = true);
                                    break;

                                // 32-bit integer type
                                case 23:                            // int4
                                    pckWriteI32P(pack, cvtZToInt(value), .defaultWrite = true);
                                    break;

                                // 32-bit unsigned integer type
                                case 26:                            // oid
                                    pckWriteU32P(pack, cvtZToUInt(value), .defaultWrite = true);
                                    break;

                                default:
                                    THROW_FMT(
                                        FormatError, "unable to parse type %u in column %d for query '%s'",
                                        columnType[columnIdx], columnIdx, strZ(query));
                            }
                        }
                    }

                    if (resultType == pgClientQueryResultAny)
                        pckWriteArrayEndP(pack);
                }

                // End pack and return result
                pckWriteEndP(pack);

                result = pckMove(pckWriteResult(pack), memContextPrior());
            }
        }
        FINALLY()
        {
            // Free the result
            PQclear(pgResult);

            CHECK(ServiceError, PQgetResult(this->connection) == NULL, "NULL result required to complete request");
        }
        TRY_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PACK, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
pgClientToLog(const PgClient *const this, StringStatic *const debugLog)
{
    strStcCat(debugLog, "{host: ");
    strStcResultSizeInc(
        debugLog,
        FUNCTION_LOG_OBJECT_FORMAT(pgClientHost(this), strToLog, strStcRemains(debugLog), strStcRemainsSize(debugLog)));

    strStcCat(debugLog, ", database: ");
    strToLog(pgClientDatabase(this), debugLog);

    strStcCat(debugLog, ", user: ");
    strStcResultSizeInc(
        debugLog,
        FUNCTION_LOG_OBJECT_FORMAT(pgClientUser(this), strToLog, strStcRemains(debugLog), strStcRemainsSize(debugLog)));

    strStcFmt(
        debugLog, ", port: %u, queryTimeout %" PRIu64 "}", pgClientPort(this), pgClientTimeout(this));
}
