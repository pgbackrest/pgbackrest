/***********************************************************************************************************************************
Test PostgreSQL Client
***********************************************************************************************************************************/
#include "common/type/json.h"

#include "common/harnessPq.h"

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
#ifdef HARNESS_PQ_REAL
        if (system("sudo pg_createcluster 11 test") != 0)
            THROW(AssertError, "unable to create cluster");

        if (system("sudo pg_ctlcluster 11 test start") != 0)
            THROW(AssertError, "unable to start cluster");

        if (system(strPtr(strNewFmt("sudo -u postgres psql -c 'create user %s superuser'", testUser()))) != 0)
            THROW(AssertError, "unable to create superuser");
#endif

        // Test connection error
        // -------------------------------------------------------------------------------------------------------------------------
#ifndef HARNESS_PQ_REAL
        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_CONNECTDB, .param = "[\"dbname='postg \\\\'\\\\\\\\res' port=5433\"]"},
            {.function = HRNPQ_STATUS, .resultInt = CONNECTION_BAD},
            {.function = HRNPQ_ERRORMESSAGE, .resultZ =
                "could not connect to server: No such file or directory\n"
                    "\tIs the server running locally and accepting\n"
                    "\tconnections on Unix domain socket \"/var/run/postgresql/.s.PGSQL.5433\"?\n"},
            {.function = HRNPQ_FINISH},
            {.function = NULL}
        });
#endif

        PgClient *client = NULL;
        TEST_ASSIGN(client, pgClientNew(NULL, 5433, strNew("postg '\\res"), NULL, 3000), "new client");
        TEST_ERROR(
            pgClientOpen(client), DbConnectError,
            "unable to connect to 'dbname='postg \\'\\\\res' port=5433': could not connect to server: No such file or directory\n"
                "\tIs the server running locally and accepting\n"
                "\tconnections on Unix domain socket \"/var/run/postgresql/.s.PGSQL.5433\"?");
        TEST_RESULT_VOID(pgClientFree(client), "free client");

        // Test send error
        // -------------------------------------------------------------------------------------------------------------------------
#ifndef HARNESS_PQ_REAL
        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_CONNECTDB, .param = "[\"dbname='postgres' port=5432\"]"},
            {.function = HRNPQ_STATUS, .resultInt = CONNECTION_OK},
            {.function = HRNPQ_SENDQUERY, .param = "[\"select bogus from pg_class\"]", .resultInt = 0},
            {.function = HRNPQ_ERRORMESSAGE, .resultZ = "another command is already in progress\n"},
            {.function = HRNPQ_FINISH},
            {.function = NULL}
        });
#endif

        TEST_ASSIGN(client, pgClientOpen(pgClientNew(NULL, 5432, strNew("postgres"), NULL, 3000)), "new client");

#ifdef HARNESS_PQ_REAL
        PQsendQuery(client->connection, "select bogus from pg_class");
#endif

        String *query = strNew("select bogus from pg_class");

        TEST_ERROR(
            pgClientQuery(client, query), DbQueryError,
            "unable to send query 'select bogus from pg_class': another command is already in progress");

        TEST_RESULT_VOID(pgClientFree(client), "free client");

        // Connect
        // -------------------------------------------------------------------------------------------------------------------------
#ifndef HARNESS_PQ_REAL
        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_CONNECTDB, .param = "[\"dbname='postgres' port=5432 user='vagrant' host='/var/run/postgresql'\"]"},
            {.function = HRNPQ_STATUS, .resultInt = CONNECTION_OK},
            {.function = NULL}
        });
#endif

        TEST_ASSIGN(
            client, pgClientOpen(pgClientNew(strNew("/var/run/postgresql"), 5432, strNew("postgres"), strNew(testUser()), 500)),
            "new client");

        // Invalid query
        // -------------------------------------------------------------------------------------------------------------------------
#ifndef HARNESS_PQ_REAL
        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_SENDQUERY, .param = "[\"select bogus from pg_class\"]", .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT},
            {.function = HRNPQ_ISBUSY},
            {.function = HRNPQ_GETRESULT},
            {.function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_FATAL_ERROR},
            {.function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_FATAL_ERROR},
            {.function = HRNPQ_RESULTERRORMESSAGE, .resultZ =
                "ERROR:  column \"bogus\" does not exist\n"
                    "LINE 1: select bogus from pg_class\n"
                    "               ^                 \n"},
            {.function = HRNPQ_CLEAR},
            {.function = HRNPQ_GETRESULT, .resultNull = true},
            {.function = NULL}
        });
#endif

        query = strNew("select bogus from pg_class");

        TEST_ERROR(
            pgClientQuery(client, query), DbQueryError,
            "unable to execute query 'select bogus from pg_class': ERROR:  column \"bogus\" does not exist\n"
                "LINE 1: select bogus from pg_class\n"
                "               ^");

        // Timeout query
        // -------------------------------------------------------------------------------------------------------------------------
#ifndef HARNESS_PQ_REAL
        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_SENDQUERY, .param = "[\"select pg_sleep(3000)\"]", .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT, .sleep = 600},
            {.function = HRNPQ_ISBUSY, .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT},
            {.function = HRNPQ_ISBUSY, .resultInt = 1},
            {.function = HRNPQ_GETCANCEL},
            {.function = HRNPQ_CANCEL, .resultInt = 1},
            {.function = HRNPQ_FREECANCEL},
            {.function = HRNPQ_GETRESULT},
            {.function = HRNPQ_CLEAR},
            {.function = HRNPQ_GETRESULT, .resultNull = true},
            {.function = NULL}
        });
#endif

        query = strNew("select pg_sleep(3000)");

        TEST_ERROR(pgClientQuery(client, query), DbQueryError, "query 'select pg_sleep(3000)' timed out after 500ms");

        // Execute do block and raise notice
        // -------------------------------------------------------------------------------------------------------------------------
#ifndef HARNESS_PQ_REAL
        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_SENDQUERY, .param = "[\"do $$ begin raise notice 'mememe'; end $$\"]", .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT},
            {.function = HRNPQ_ISBUSY},
            {.function = HRNPQ_GETRESULT},
            {.function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},
            {.function = HRNPQ_CLEAR},
            {.function = HRNPQ_GETRESULT, .resultNull = true},
            {.function = NULL}
        });
#endif

        query = strNew("do $$ begin raise notice 'mememe'; end $$");

        TEST_RESULT_PTR(pgClientQuery(client, query), NULL, "excecute do block");

        // Unsupported type
        // -------------------------------------------------------------------------------------------------------------------------
#ifndef HARNESS_PQ_REAL
        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_SENDQUERY, .param = "[\"select clock_timestamp()\"]", .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT},
            {.function = HRNPQ_ISBUSY},
            {.function = HRNPQ_GETRESULT},
            {.function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},
            {.function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},
            {.function = HRNPQ_NTUPLES, .resultInt = 1},
            {.function = HRNPQ_NFIELDS, .resultInt = 1},
            {.function = HRNPQ_GETISNULL, .param = "[0,0]", .resultInt = 0},
            {.function = HRNPQ_FTYPE, .param = "[0]", .resultInt = 1184},
            {.function = HRNPQ_FTYPE, .param = "[0]", .resultInt = 1184},
            {.function = HRNPQ_CLEAR},
            {.function = HRNPQ_GETRESULT, .resultNull = true},
            {.function = NULL}
        });
#endif

        query = strNew("select clock_timestamp()");

        TEST_ERROR(
            pgClientQuery(client, query), FormatError,
            "unable to parse type 1184 in column 0 for query 'select clock_timestamp()'");

        // Successful query
        // -------------------------------------------------------------------------------------------------------------------------
#ifndef HARNESS_PQ_REAL
        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_SENDQUERY, .param =
                "[\"select oid, null, relname, relname = 'pg_class' from pg_class where relname in ('pg_class', 'pg_proc') "
                    "order by relname\"]",
                .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT},
            {.function = HRNPQ_ISBUSY},
            {.function = HRNPQ_GETRESULT},
            {.function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},
            {.function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},

            {.function = HRNPQ_NTUPLES, .resultInt = 2},
            {.function = HRNPQ_NFIELDS, .resultInt = 4},
            {.function = HRNPQ_GETISNULL, .param = "[0,0]", .resultInt = 0},
            {.function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_INT},
            {.function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = "1259"},
            {.function = HRNPQ_NFIELDS, .resultInt = 4},
            {.function = HRNPQ_GETISNULL, .param = "[0,1]", .resultInt = 1},
            {.function = HRNPQ_NFIELDS, .resultInt = 4},
            {.function = HRNPQ_GETISNULL, .param = "[0,2]", .resultInt = 0},
            {.function = HRNPQ_FTYPE, .param = "[2]", .resultInt = HRNPQ_TYPE_TEXT},
            {.function = HRNPQ_GETVALUE, .param = "[0,2]", .resultZ = "pg_class"},
            {.function = HRNPQ_NFIELDS, .resultInt = 4},
            {.function = HRNPQ_GETISNULL, .param = "[0,3]", .resultInt = 0},
            {.function = HRNPQ_FTYPE, .param = "[3]", .resultInt = HRNPQ_TYPE_BOOL},
            {.function = HRNPQ_GETVALUE, .param = "[0,3]", .resultZ = "t"},
            {.function = HRNPQ_NFIELDS, .resultInt = 4},

            {.function = HRNPQ_NTUPLES, .resultInt = 2},
            {.function = HRNPQ_NFIELDS, .resultInt = 4},
            {.function = HRNPQ_GETISNULL, .param = "[1,0]", .resultInt = 0},
            {.function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_INT},
            {.function = HRNPQ_GETVALUE, .param = "[1,0]", .resultZ = "1255"},
            {.function = HRNPQ_NFIELDS, .resultInt = 4},
            {.function = HRNPQ_GETISNULL, .param = "[1,1]", .resultInt = 1},
            {.function = HRNPQ_NFIELDS, .resultInt = 4},
            {.function = HRNPQ_GETISNULL, .param = "[1,2]", .resultInt = 0},
            {.function = HRNPQ_FTYPE, .param = "[2]", .resultInt = HRNPQ_TYPE_TEXT},
            {.function = HRNPQ_GETVALUE, .param = "[1,2]", .resultZ = "pg_proc"},
            {.function = HRNPQ_NFIELDS, .resultInt = 4},
            {.function = HRNPQ_GETISNULL, .param = "[1,3]", .resultInt = 0},
            {.function = HRNPQ_FTYPE, .param = "[3]", .resultInt = HRNPQ_TYPE_BOOL},
            {.function = HRNPQ_GETVALUE, .param = "[1,3]", .resultZ = "f"},
            {.function = HRNPQ_NFIELDS, .resultInt = 4},
            {.function = HRNPQ_NTUPLES, .resultInt = 2},

            {.function = HRNPQ_CLEAR},
            {.function = HRNPQ_GETRESULT, .resultNull = true},
            {.function = NULL}
        });
#endif

        query = strNew(
            "select oid, null, relname, relname = 'pg_class' from pg_class where relname in ('pg_class', 'pg_proc')"
                " order by relname");

        TEST_RESULT_STR(
            strPtr(jsonFromVar(varNewVarLst(pgClientQuery(client, query)), 0)),
            "[[1259,null,\"pg_class\",true],[1255,null,\"pg_proc\",false]]", "simple query");

        // Close connection
        // -------------------------------------------------------------------------------------------------------------------------
#ifndef HARNESS_PQ_REAL
        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_FINISH},
            {.function = HRNPQ_GETRESULT, .resultNull = true},
            {.function = NULL}
        });
#endif
        TEST_RESULT_VOID(pgClientClose(client), "close client");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
