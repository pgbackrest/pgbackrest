/***********************************************************************************************************************************
Test PostgreSQL Client

This test can be run two ways:

1) The default uses a libpq shim to simulate a PostgreSQL connection.  This will work with all VM types.

2) Optionally use a real cluster for testing (only works with debian/pg11).  The test Makefile must be manually updated with the
-DHARNESS_PQ_REAL flag and -lpq must be added to the libs list.  This method does not have 100% coverage but is very close.
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

    // PQfinish() is strictly checked
    harnessPqScriptStrictSet(true);

    // *****************************************************************************************************************************
    if (testBegin("pgClient"))
    {
        // Create and start the test database
        // -------------------------------------------------------------------------------------------------------------------------
#ifdef HARNESS_PQ_REAL
        HRN_SYSTEM("sudo pg_createcluster 11 test");
        HRN_SYSTEM("sudo pg_ctlcluster 11 test start");
        HRN_SYSTEM("sudo -u postgres psql -c 'create user " TEST_USER " superuser'");
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

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(client, pgClientNew(NULL, 5433, STRDEF("postg '\\res"), NULL, 3000), "new client");
            TEST_RESULT_VOID(pgClientMove(client, memContextPrior()), "move client");
            TEST_RESULT_VOID(pgClientMove(NULL, memContextPrior()), "move null client");
        }
        MEM_CONTEXT_TEMP_END();

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

        TEST_ASSIGN(client, pgClientOpen(pgClientNew(NULL, 5432, STRDEF("postgres"), NULL, 3000)), "new client");

#ifdef HARNESS_PQ_REAL
        PQsendQuery(client->connection, "select bogus from pg_class");
#endif

        const String *query = STRDEF("select bogus from pg_class");

        TEST_ERROR(
            pgClientQuery(client, query), DbQueryError,
            "unable to send query 'select bogus from pg_class': another command is already in progress");

        TEST_RESULT_VOID(pgClientFree(client), "free client");

        // Connect
        // -------------------------------------------------------------------------------------------------------------------------
#ifndef HARNESS_PQ_REAL
        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_CONNECTDB,
                .param = "[\"dbname='postgres' port=5432 user='" TEST_USER "' host='/var/run/postgresql'\"]"},
            {.function = HRNPQ_STATUS, .resultInt = CONNECTION_OK},
            {.function = NULL}
        });
#endif

        TEST_ASSIGN(
            client, pgClientOpen(pgClientNew(STRDEF("/var/run/postgresql"), 5432, STRDEF("postgres"), TEST_USER_STR, 500)),
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
            {.function = HRNPQ_RESULTERRORMESSAGE, .resultZ =
                "ERROR:  column \"bogus\" does not exist\n"
                    "LINE 1: select bogus from pg_class\n"
                    "               ^                 \n"},
            {.function = HRNPQ_CLEAR},
            {.function = HRNPQ_GETRESULT, .resultNull = true},
            {.function = NULL}
        });
#endif

        query = STRDEF("select bogus from pg_class");

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

        query = STRDEF("select pg_sleep(3000)");

        TEST_ERROR(pgClientQuery(client, query), DbQueryError, "query 'select pg_sleep(3000)' timed out after 500ms");

        // Cancel error (can only be run with the scripted tests
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
            {.function = HRNPQ_CANCEL, .resultInt = 0, .resultZ = "test error"},
            {.function = HRNPQ_FREECANCEL},
            {.function = NULL}
        });

        query = STRDEF("select pg_sleep(3000)");

        TEST_ERROR(pgClientQuery(client, query), DbQueryError, "unable to cancel query 'select pg_sleep(3000)': test error");
#endif

        // -------------------------------------------------------------------------------------------------------------------------
#ifndef HARNESS_PQ_REAL
        TEST_TITLE("PQgetCancel() returns NULL");

        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_SENDQUERY, .param = "[\"select 1\"]", .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT, .sleep = 600},
            {.function = HRNPQ_ISBUSY, .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT},
            {.function = HRNPQ_ISBUSY, .resultInt = 1},
            {.function = HRNPQ_GETCANCEL, .resultNull = true},
            {.function = NULL}
        });

        TEST_ERROR(
            pgClientQuery(client, STRDEF("select 1")), DbQueryError, "unable to cancel query 'select 1': connection was lost");
#endif

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

        query = STRDEF("do $$ begin raise notice 'mememe'; end $$");

        TEST_RESULT_PTR(pgClientQuery(client, query), NULL, "execute do block");

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
            {.function = HRNPQ_NTUPLES, .resultInt = 1},
            {.function = HRNPQ_NFIELDS, .resultInt = 1},
            {.function = HRNPQ_FTYPE, .param = "[0]", .resultInt = 1184},
            {.function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = "2019-07-25 12:06:09.000282+00"},
            {.function = HRNPQ_CLEAR},
            {.function = HRNPQ_GETRESULT, .resultNull = true},
            {.function = NULL}
        });
#endif

        query = STRDEF("select clock_timestamp()");

        TEST_ERROR(
            pgClientQuery(client, query), FormatError,
            "unable to parse type 1184 in column 0 for query 'select clock_timestamp()'");

        // Successful query
        // -------------------------------------------------------------------------------------------------------------------------
#ifndef HARNESS_PQ_REAL
        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_SENDQUERY, .param =
                "[\"select oid, case when relname = 'pg_class' then null::text else '' end, relname, relname = 'pg_class'"
                    "  from pg_class where relname in ('pg_class', 'pg_proc')"
                    " order by relname\"]",
                .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT},
            {.function = HRNPQ_ISBUSY},
            {.function = HRNPQ_GETRESULT},
            {.function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},

            {.function = HRNPQ_NTUPLES, .resultInt = 2},
            {.function = HRNPQ_NFIELDS, .resultInt = 4},
            {.function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_INT},
            {.function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_TEXT},
            {.function = HRNPQ_FTYPE, .param = "[2]", .resultInt = HRNPQ_TYPE_TEXT},
            {.function = HRNPQ_FTYPE, .param = "[3]", .resultInt = HRNPQ_TYPE_BOOL},

            {.function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = "1259"},
            {.function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = ""},
            {.function = HRNPQ_GETISNULL, .param = "[0,1]", .resultInt = 1},
            {.function = HRNPQ_GETVALUE, .param = "[0,2]", .resultZ = "pg_class"},
            {.function = HRNPQ_GETVALUE, .param = "[0,3]", .resultZ = "t"},

            {.function = HRNPQ_GETVALUE, .param = "[1,0]", .resultZ = "1255"},
            {.function = HRNPQ_GETVALUE, .param = "[1,1]", .resultZ = ""},
            {.function = HRNPQ_GETISNULL, .param = "[1,1]", .resultInt = 0},
            {.function = HRNPQ_GETVALUE, .param = "[1,2]", .resultZ = "pg_proc"},
            {.function = HRNPQ_GETVALUE, .param = "[1,3]", .resultZ = "f"},

            {.function = HRNPQ_CLEAR},
            {.function = HRNPQ_GETRESULT, .resultNull = true},
            {.function = NULL}
        });
#endif

        query = STRDEF(
            "select oid, case when relname = 'pg_class' then null::text else '' end, relname, relname = 'pg_class'"
            "  from pg_class where relname in ('pg_class', 'pg_proc')"
            " order by relname");

        TEST_RESULT_STR_Z(
            jsonFromVar(varNewVarLst(pgClientQuery(client, query))),
            "[[1259,null,\"pg_class\",true],[1255,\"\",\"pg_proc\",false]]", "simple query");

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
        TEST_RESULT_VOID(pgClientClose(client), "close client again");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
