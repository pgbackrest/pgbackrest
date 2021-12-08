/***********************************************************************************************************************************
Postgres Client
***********************************************************************************************************************************/
#include "build.auto.h"

#include <libpq-fe.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/type/list.h"
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
PgClient *
pgClientNew(const String *host, const unsigned int port, const String *database, const String *user, const TimeMSec timeout)
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

    PgClient *this = NULL;

    OBJ_NEW_BEGIN(PgClient)
    {
        this = OBJ_NEW_ALLOC();

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

    String *result = strCatZ(strNew(), "'");

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
    CHECK(AssertError, this->connection == NULL, "invalid connection");

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Base connection string
        String *connInfo = strCatFmt(
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
VariantList *
pgClientQuery(PgClient *this, const String *query)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PG_CLIENT, this);
        FUNCTION_LOG_PARAM(STRING, query);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    CHECK(AssertError, this->connection != NULL, "invalid connection");
    ASSERT(query != NULL);

    VariantList *result = NULL;

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
        Wait *wait = waitNew(pgClientTimeout(this));
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
                    THROW_FMT(DbQueryError, "unable to cancel query '%s': %s", strZ(query), strZ(strTrim(strNewZ(error))));
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
                THROW_FMT(DbQueryError, "query '%s' timed out after %" PRIu64 "ms", strZ(query), pgClientTimeout(this));

            // If this was a command that returned no results then we are done
            ExecStatusType resultStatus = PQresultStatus(pgResult);

            if (resultStatus != PGRES_COMMAND_OK)
            {
                // Expect some rows to be returned
                if (resultStatus != PGRES_TUPLES_OK)
                {
                    THROW_FMT(
                        DbQueryError, "unable to execute query '%s': %s", strZ(query),
                        strZ(strTrim(strNewZ(PQresultErrorMessage(pgResult)))));
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

            CHECK(ServiceError, PQgetResult(this->connection) == NULL, "NULL result required to complete request");
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
        memContextCallbackClear(objMemContext(this));
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
        "{host: %s, port: %u, database: %s, user: %s, queryTimeout %" PRIu64 "}", strZ(strToLog(pgClientHost(this))),
        pgClientPort(this), strZ(strToLog(pgClientDatabase(this))), strZ(strToLog(pgClientUser(this))), pgClientTimeout(this));
}
