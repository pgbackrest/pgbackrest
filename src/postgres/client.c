/***********************************************************************************************************************************
Postgres Client
***********************************************************************************************************************************/
#include "build.auto.h"

#include <libpq-fe.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/list.h"
#include "common/type/object.h"
#include "common/wait.h"
#include "postgres/client.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct PgClient
{
    MemContext *memContext;
    const String *host;
    unsigned int port;
    const String *database;
    const String *user;
    TimeMSec queryTimeout;

    PGconn *connection;
};

OBJECT_DEFINE_MOVE(PG_CLIENT);
OBJECT_DEFINE_FREE(PG_CLIENT);

/***********************************************************************************************************************************
Close protocol connection
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(PG_CLIENT, LOG, logLevelTrace)
{
    PQfinish(this->connection);
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/**********************************************************************************************************************************/
PgClient *
pgClientNew(const String *host, const unsigned int port, const String *database, const String *user, const TimeMSec queryTimeout)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(STRING, database);
        FUNCTION_LOG_PARAM(STRING, user);
        FUNCTION_LOG_PARAM(TIME_MSEC, queryTimeout);
    FUNCTION_LOG_END();

    ASSERT(port >= 1 && port <= 65535);
    ASSERT(database != NULL);

    PgClient *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("PgClient")
    {
        this = memNew(sizeof(PgClient));

        *this = (PgClient)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .host = strDup(host),
            .port = port,
            .database = strDup(database),
            .user = strDup(user),
            .queryTimeout = queryTimeout,
        };
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(PG_CLIENT, this);
}

/***********************************************************************************************************************************
Just ignore notices and warnings
***********************************************************************************************************************************/
static void
pgClientNoticeProcessor(void *arg, const char *message)
{
    (void)arg;
    (void)message;
}

/***********************************************************************************************************************************
Encode string to escape ' and \
***********************************************************************************************************************************/
static String *
pgClientEscape(const String *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    ASSERT(string != NULL);

    String *result = strNew("'");

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

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
PgClient *
pgClientOpen(PgClient *this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PG_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    CHECK(this->connection == NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Base connection string
        String *connInfo = strNewFmt("dbname=%s port=%u", strZ(pgClientEscape(this->database)), this->port);

        // Add user if specified
        if (this->user != NULL)
            strCatFmt(connInfo, " user=%s", strZ(pgClientEscape(this->user)));

        // Add host if specified
        if (this->host != NULL)
            strCatFmt(connInfo, " host=%s", strZ(pgClientEscape(this->host)));

        // Make the connection
        this->connection = PQconnectdb(strZ(connInfo));

        // Set a callback to shutdown the connection
        memContextCallbackSet(this->memContext, pgClientFreeResource, this);

        // Handle errors
        if (PQstatus(this->connection) != CONNECTION_OK)
        {
            THROW_FMT(
                DbConnectError, "unable to connect to '%s': %s", strZ(connInfo),
                strZ(strTrim(strNew(PQerrorMessage(this->connection)))));
        }

        // Set notice and warning processor
        PQsetNoticeProcessor(this->connection, pgClientNoticeProcessor, NULL);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PG_CLIENT, this);
}

/**********************************************************************************************************************************/
VariantList *
pgClientQuery(PgClient *this, const String *query)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PG_CLIENT, this);
        FUNCTION_LOG_PARAM(STRING, query);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    CHECK(this->connection != NULL);
    ASSERT(query != NULL);

    VariantList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Send the query without waiting for results so we can timeout if needed
        if (!PQsendQuery(this->connection, strZ(query)))
        {
            THROW_FMT(
                DbQueryError, "unable to send query '%s': %s", strZ(query),
                strZ(strTrim(strNew(PQerrorMessage(this->connection)))));
        }

        // Wait for a result
        Wait *wait = waitNew(this->queryTimeout);
        bool busy = false;

        do
        {
            PQconsumeInput(this->connection);
            busy = PQisBusy(this->connection);
        }
        while (busy && waitMore(wait));

        // If the query is still busy after the timeout attempt to cancel
        if (busy)
        {
            PGcancel *cancel = PQgetCancel(this->connection);

            // If cancel is NULL then more than likely the server process crashed or disconnected
            if (cancel == NULL)
                THROW_FMT(DbQueryError, "unable to cancel query '%s': connection was lost", strZ(query));

            TRY_BEGIN()
            {
                char error[256];

                if (!PQcancel(cancel, error, sizeof(error)))
                    THROW_FMT(DbQueryError, "unable to cancel query '%s': %s", strZ(query), strZ(strTrim(strNew(error))));
            }
            FINALLY()
            {
                PQfreeCancel(cancel);
            }
            TRY_END();
        }

        // Get the result (even if query was cancelled -- to prevent the connection being left in a bad state)
        PGresult *pgResult = PQgetResult(this->connection);

        TRY_BEGIN()
        {
            // Throw timeout error if cancelled
            if (busy)
                THROW_FMT(DbQueryError, "query '%s' timed out after %" PRIu64 "ms", strZ(query), this->queryTimeout);

            // If this was a command that returned no results then we are done
            ExecStatusType resultStatus = PQresultStatus(pgResult);

            if (resultStatus != PGRES_COMMAND_OK)
            {
                // Expect some rows to be returned
                if (resultStatus != PGRES_TUPLES_OK)
                {
                    THROW_FMT(
                        DbQueryError, "unable to execute query '%s': %s", strZ(query),
                        strZ(strTrim(strNew(PQresultErrorMessage(pgResult)))));
                }

                // Fetch row and column values
                result = varLstNew();

                MEM_CONTEXT_BEGIN(lstMemContext((List *)result))
                {
                    int rowTotal = PQntuples(pgResult);
                    int columnTotal = PQnfields(pgResult);

                    // Get column types
                    Oid *columnType = memNew(sizeof(int) * (size_t)columnTotal);

                    for (int columnIdx = 0; columnIdx < columnTotal; columnIdx++)
                        columnType[columnIdx] = PQftype(pgResult, columnIdx);

                    // Get values
                    for (int rowIdx = 0; rowIdx < rowTotal; rowIdx++)
                    {
                        VariantList *resultRow = varLstNew();

                        for (int columnIdx = 0; columnIdx < columnTotal; columnIdx++)
                        {
                            char *value = PQgetvalue(pgResult, rowIdx, columnIdx);

                            // If value is zero-length then check if it is null
                            if (value[0] == '\0' && PQgetisnull(pgResult, rowIdx, columnIdx))
                            {
                                varLstAdd(resultRow, NULL);
                            }
                            // Else convert the value to a variant
                            else
                            {
                                // Convert column type.  Not all PostgreSQL types are supported but these should suffice.
                                switch (columnType[columnIdx])
                                {
                                    // Boolean type
                                    case 16:                            // bool
                                        varLstAdd(resultRow, varNewBool(varBoolForce(varNewStrZ(value))));
                                        break;

                                    // Text/char types
                                    case 18:                            // char
                                    case 19:                            // name
                                    case 25:                            // text
                                        varLstAdd(resultRow, varNewStrZ(value));
                                        break;

                                    // Integer types
                                    case 20:                            // int8
                                    case 21:                            // int2
                                    case 23:                            // int4
                                    case 26:                            // oid
                                        varLstAdd(resultRow, varNewInt64(cvtZToInt64(value)));
                                        break;

                                    default:
                                        THROW_FMT(
                                            FormatError, "unable to parse type %u in column %d for query '%s'",
                                            columnType[columnIdx], columnIdx, strZ(query));
                                }
                            }
                        }

                        varLstAdd(result, varNewVarLst(resultRow));
                    }
                }
                MEM_CONTEXT_END();
            }
        }
        FINALLY()
        {
            // Free the result
            PQclear(pgResult);

            // Need to get a NULL result to complete the request
            CHECK(PQgetResult(this->connection) == NULL);
        }
        TRY_END();

        varLstMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(VARIANT_LIST, result);
}

/**********************************************************************************************************************************/
void
pgClientClose(PgClient *this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PG_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    if (this->connection != NULL)
    {
        memContextCallbackClear(this->memContext);
        PQfinish(this->connection);
        this->connection = NULL;
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
String *
pgClientToLog(const PgClient *this)
{
    return strNewFmt(
        "{host: %s, port: %u, database: %s, user: %s, queryTimeout %" PRIu64 "}", strZ(strToLog(this->host)), this->port,
        strZ(strToLog(this->database)), strZ(strToLog(this->user)), this->queryTimeout);
}
