/***********************************************************************************************************************************
Postgres Client
***********************************************************************************************************************************/
#include "build.auto.h"

// #include <unistd.h>
#include </usr/include/postgresql/libpq-fe.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"
#include "common/type/list.h"
#include "common/type/string.h"
#include "common/type/variantList.h"
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
pgClientNew(const String *host, const unsigned int port, const String *database, const String *user)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(STRING, database);
        FUNCTION_LOG_PARAM(STRING, user);
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
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(PG_CLIENT, this);
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
        // sleep(2);
        String *connInfo = strNewFmt("postgresql:///%s?port=%u", strPtr(this->database), this->port);
        // postgresql:///mydb?host=localhost&port=5433

        // Make the connection
        this->connection = PQconnectdb(strPtr(connInfo));

        // Set a callback to shutdown the connection
        memContextCallbackSet(this->memContext, pgClientFreeResource, this);

        // Handle errors
        if (PQstatus(this->connection) != CONNECTION_OK)
            THROW_FMT(DbConnectError, "unable to connect to '%s': %s", strPtr(connInfo), strPtr(strTrim(strNew(PQerrorMessage(this->connection)))));
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
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    CHECK(this->connection != NULL);

    VariantList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PGresult *pgResult = PQexec(this->connection, strPtr(query));

        TRY_BEGIN()
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
                                        resultRow, varNewBool(varBoolForce(varNewStrZ(PQgetvalue(pgResult, rowIdx, columnIdx)))));
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
                                case 24:                            // int4
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
        FINALLY()
        {
            PQclear(pgResult);
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
