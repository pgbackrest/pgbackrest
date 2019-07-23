/***********************************************************************************************************************************
Postgres Client
***********************************************************************************************************************************/
#include "build.auto.h"

#include <libpq-fe.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"
#include "common/time.h"
#include "common/type/list.h"
#include "common/type/string.h"
#include "common/type/variantList.h"
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

OBJECT_DEFINE_FREE(PG_CLIENT);

/***********************************************************************************************************************************
Close protocol connection
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(PG_CLIENT, LOG, logLevelTrace)
{
    PQfinish(this->connection);
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/***********************************************************************************************************************************
Create object
***********************************************************************************************************************************/
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
        this->memContext = memContextCurrent();

        this->host = strDup(host);
        this->port = port;
        this->database = strDup(database);
        this->user = strDup(user);
        this->queryTimeout = queryTimeout;
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
Open connection to PostgreSQL
***********************************************************************************************************************************/
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
        String *connInfo = strNewFmt("postgresql:///%s?port=%u", strPtr(this->database), this->port);

        if (this->user != NULL)
            strCatFmt(connInfo, "&user=%s", strPtr(this->user));

        if (this->host != NULL)
            strCatFmt(connInfo, "&host=%s", strPtr(this->host));

        // Make the connection
        this->connection = PQconnectdb(strPtr(connInfo));

        // Set a callback to shutdown the connection
        memContextCallbackSet(this->memContext, pgClientFreeResource, this);

        // Handle errors
        if (PQstatus(this->connection) != CONNECTION_OK)
        {
            THROW_FMT(
                DbConnectError, "unable to connect to '%s': %s", strPtr(connInfo),
                strPtr(strTrim(strNew(PQerrorMessage(this->connection)))));
        }

        // Set notice and warning processor
        PQsetNoticeProcessor(this->connection, pgClientNoticeProcessor, NULL);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PG_CLIENT, this);
}

/***********************************************************************************************************************************
Execute a query and return results
***********************************************************************************************************************************/
VariantList *
pgClientQuery(PgClient *this, const String *query)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PG_CLIENT, this);
        FUNCTION_LOG_PARAM(STRING, query);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    CHECK(this->connection != NULL);

    VariantList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Send the query without waiting for results so we can timeout if needed
        if (!PQsendQuery(this->connection, strPtr(query)))
        {
            THROW_FMT(
                DbQueryError, "unable to send query '%s': %s", strPtr(query),
                strPtr(strTrim(strNew(PQerrorMessage(this->connection)))));
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
            PQrequestCancel(this->connection);

        // Get the result even if we cancelled
        PGresult *pgResult = PQgetResult(this->connection);

        TRY_BEGIN()
        {
            // Throw timeout error if cancelled
            if (busy)
                THROW_FMT(DbQueryError, "query '%s' timed out after %" PRIu64 "ms", strPtr(query), this->queryTimeout);

            // If this was a command that returned no results then we are done
            if (PQresultStatus(pgResult) != PGRES_COMMAND_OK)
            {
                if (PQresultStatus(pgResult) != PGRES_TUPLES_OK)
                {
                    THROW_FMT(
                        DbQueryError, "unable to execute query '%s': %s", strPtr(query),
                        strPtr(strTrim(strNew(PQresultErrorMessage(pgResult)))));
                }

                result = varLstNew();

                MEM_CONTEXT_BEGIN(lstMemContext((List *)result))
                {
                    for (int rowIdx = 0; rowIdx < PQntuples(pgResult); rowIdx++)
                    {
                        VariantList *resultRow = varLstNew();

                        for (int columnIdx = 0; columnIdx < PQnfields(pgResult); columnIdx++)
                        {
                            if (PQgetisnull(pgResult, rowIdx, columnIdx))
                                varLstAdd(resultRow, NULL);
                            else
                            {
                                switch (PQftype(pgResult, columnIdx))
                                {
                                    // Boolean type
                                    case 16:                            // bool
                                    {
                                        varLstAdd(
                                            resultRow,
                                            varNewBool(varBoolForce(varNewStrZ(PQgetvalue(pgResult, rowIdx, columnIdx)))));
                                        break;
                                    }

                                    // Text/char types
                                    case 18:                            // char
                                    case 19:                            // name
                                    case 25:                            // text
                                    {
                                        varLstAdd(resultRow, varNewStrZ(PQgetvalue(pgResult, rowIdx, columnIdx)));
                                        break;
                                    }

                                    // Integer types
                                    case 20:                            // int8
                                    case 21:                            // int2
                                    case 23:                            // int4
                                    case 26:                            // oid
                                    {
                                        varLstAdd(resultRow, varNewInt64(cvtZToInt64(PQgetvalue(pgResult, rowIdx, columnIdx))));
                                        break;
                                    }

                                    default:
                                    {
                                        THROW_FMT(
                                            FormatError, "unable to parse type %u in column %d for query '%s'",
                                            PQftype(pgResult, columnIdx), columnIdx, strPtr(query));
                                    }
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

        varLstMove(result, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(VARIANT_LIST, result);
}

/***********************************************************************************************************************************
Close connection to PostgreSQL
***********************************************************************************************************************************/
void
pgClientClose(PgClient *this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PG_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    CHECK(this->connection != NULL);

    memContextCallbackClear(this->memContext);
    PQfinish(this->connection);
    this->connection = NULL;

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
pgClientToLog(const PgClient *this)
{
    return strNewFmt(
        "{host: %s, port: %u, database: %s, user: %s}", strPtr(strToLog(this->host)), this->port, strPtr(strToLog(this->database)),
        strPtr(strToLog(this->user)));
}
