/***********************************************************************************************************************************
Test PostgreSQL Client
***********************************************************************************************************************************/
#include "common/type/json.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("pgClient"))
    {
        if (system("sudo pg_createcluster 11 test") != 0)
            THROW(AssertError, "unable to create cluster");

        if (system("sudo pg_ctlcluster 11 test start") != 0)
            THROW(AssertError, "unable to start cluster");

        if (system(strPtr(strNewFmt("sudo -u postgres psql -c 'create user %s superuser'", testUser()))) != 0)
            THROW(AssertError, "unable to create superuser");

        // Test connection error
        // -------------------------------------------------------------------------------------------------------------------------
        PgClient *client = NULL;
        TEST_ASSIGN(client, pgClientNew(NULL, 5433, strNew("postgres"), NULL, 3000), "new client");
        TEST_ERROR(
            pgClientOpen(client), DbConnectError,
            "unable to connect to 'postgresql:///postgres?port=5433': could not connect to server: No such file or directory\n"
                "\tIs the server running locally and accepting\n"
                "\tconnections on Unix domain socket \"/var/run/postgresql/.s.PGSQL.5433\"?");
        TEST_RESULT_VOID(pgClientFree(client), "free client");

        // Test send error
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(client, pgClientOpen(pgClientNew(NULL, 5432, strNew("postgres"), NULL, 3000)), "new client");
        PQsendQuery(client->connection, "select bogus from pg_class");

        String *query = strNew("select bogus from pg_class");

        TEST_ERROR(
            pgClientQuery(client, query), DbQueryError,
            "unable to send query 'select bogus from pg_class': another command is already in progress");

        TEST_RESULT_VOID(pgClientFree(client), "free client");

        // Connect and test queries
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(
            client, pgClientOpen(pgClientNew(strNew("/var/run/postgresql"), 5432, strNew("postgres"), NULL, 500)), "new client");

        // Invalid query
        query = strNew("select bogus from pg_class");

        TEST_ERROR(
            pgClientQuery(client, query), DbQueryError,
            "unable to execute query 'select bogus from pg_class': ERROR:  column \"bogus\" does not exist\n"
                "LINE 1: select bogus from pg_class\n"
                "               ^");

        // Timeout query
        query = strNew("select pg_sleep(30)");

        TEST_ERROR(pgClientQuery(client, query), DbQueryError, "query 'select pg_sleep(30)' timed out after 500ms");

        // Execute do block and raise notice
        query = strNew("do $$ begin raise notice 'mememe'; end $$");

        TEST_RESULT_PTR(pgClientQuery(client, query), NULL, "excecute do block");

        // Unsupported type
        query = strNew("select clock_timestamp()");

        TEST_ERROR(
            pgClientQuery(client, query), FormatError,
            "unable to parse type 1184 in column 0 for query 'select clock_timestamp()'");

        // Successful query
        query = strNew(
            "select oid, null, relname, relname = 'pg_class' from pg_class where relname in ('pg_class', 'pg_proc')"
                " order by relname");

        TEST_RESULT_STR(
            strPtr(jsonFromVar(varNewVarLst(pgClientQuery(client, query)), 0)),
            "[[1259,null,\"pg_class\",true],[1255,null,\"pg_proc\",false]]", "simple query");

        TEST_RESULT_VOID(pgClientClose(client), "close client");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
