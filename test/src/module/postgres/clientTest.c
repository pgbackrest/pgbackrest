/***********************************************************************************************************************************
Test PostgreSQL Client

This test can be run two ways:

1) The default uses a libpq shim to simulate a PostgreSQL connection. This will work with all VM types.

2) Optionally use a real cluster for testing (only works on Debian) add `define: -DHARNESS_PQ_REAL` to the postgres/client test in
define.yaml. THe PostgreSQL version can be adjusted by changing TEST_PG_VERSION.
***********************************************************************************************************************************/
#include "common/type/json.h"

#include "common/harnessPq.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // PQfinish() is strictly checked
#ifndef HARNESS_PQ_REAL
    harnessPqScriptStrictSet(true);
#endif

    // *****************************************************************************************************************************
    if (testBegin("pgClient"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
#ifdef HARNESS_PQ_REAL
        TEST_TITLE("create/start test database");

        #define TEST_PG_VERSION                                     "14"

        HRN_SYSTEM("sudo pg_createcluster " TEST_PG_VERSION " test");
        HRN_SYSTEM("sudo pg_ctlcluster " TEST_PG_VERSION " test start");
        HRN_SYSTEM("cd /var/lib/postgresql && sudo -u postgres psql -c 'create user " TEST_USER " superuser'");
#endif

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("connection error");

        #define TEST_PQ_ERROR                                                                                                      \
            "connection to server on socket \"/var/run/postgresql/.s.PGSQL.5433\" failed: No such file or directory\n"             \
            "\tIs the server running locally and accepting connections on that socket?"

#ifndef HARNESS_PQ_REAL
        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_CONNECTDB, .param = "[\"dbname='postg \\\\'\\\\\\\\res' port=5433\"]"},
            {.function = HRNPQ_STATUS, .resultInt = CONNECTION_BAD},
            {.function = HRNPQ_ERRORMESSAGE, .resultZ = TEST_PQ_ERROR},
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
            pgClientOpen(client), DbConnectError, "unable to connect to 'dbname='postg \\'\\\\res' port=5433': " TEST_PQ_ERROR);
        TEST_RESULT_VOID(pgClientFree(client), "free client");

        #undef TEST_PQ_ERROR

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("send error");

        #define TEST_PQ_ERROR                                       "another command is already in progress"
        #define TEST_QUERY                                          "select bogus from pg_class"

#ifndef HARNESS_PQ_REAL
        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_CONNECTDB, .param = "[\"dbname='postgres' port=5432\"]"},
            {.function = HRNPQ_STATUS, .resultInt = CONNECTION_OK},
            {.function = HRNPQ_SENDQUERY, .param = "[\"" TEST_QUERY "\"]", .resultInt = 0},
            {.function = HRNPQ_ERRORMESSAGE, .resultZ = TEST_PQ_ERROR "\n"},
            {.function = HRNPQ_FINISH},
            {.function = NULL}
        });
#endif

        TEST_ASSIGN(client, pgClientOpen(pgClientNew(NULL, 5432, STRDEF("postgres"), NULL, 3000)), "new client");

#ifdef HARNESS_PQ_REAL
        PQsendQuery(client->connection, TEST_QUERY);
#endif

        TEST_ERROR(
            pgClientQuery(client, STRDEF(TEST_QUERY)), DbQueryError, "unable to send query '" TEST_QUERY "': " TEST_PQ_ERROR);

        TEST_RESULT_VOID(pgClientFree(client), "free client");

        #undef TEST_PQ_ERROR
        #undef TEST_QUERY

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("connect");

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

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid query");

        #define TEST_PQ_ERROR                                                                                                      \
            "ERROR:  column \"bogus\" does not exist\n"                                                                            \
            "LINE 1: select bogus from pg_class\n"                                                                                 \
            "               ^"
        #define TEST_QUERY                                          "select bogus from pg_class"

#ifndef HARNESS_PQ_REAL
        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_SENDQUERY, .param = "[\"" TEST_QUERY "\"]", .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT},
            {.function = HRNPQ_ISBUSY},
            {.function = HRNPQ_GETRESULT},
            {.function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_FATAL_ERROR},
            {.function = HRNPQ_RESULTERRORMESSAGE, .resultZ = TEST_PQ_ERROR "                 \n"},
            {.function = HRNPQ_CLEAR},
            {.function = HRNPQ_GETRESULT, .resultNull = true},
            {.function = NULL}
        });
#endif

        TEST_ERROR(
            pgClientQuery(client, STRDEF(TEST_QUERY)), DbQueryError, "unable to execute query '" TEST_QUERY "': " TEST_PQ_ERROR);

        #undef TEST_PQ_ERROR
        #undef TEST_QUERY

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("query timeout");

        #define TEST_QUERY                                          "select pg_sleep(3000)"

#ifndef HARNESS_PQ_REAL
        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_SENDQUERY, .param = "[\"" TEST_QUERY "\"]", .resultInt = 1},
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

        TEST_ERROR(pgClientQuery(client, STRDEF(TEST_QUERY)), DbQueryError, "query '" TEST_QUERY "' timed out after 500ms");

        #undef TEST_QUERY

        // -------------------------------------------------------------------------------------------------------------------------
#ifndef HARNESS_PQ_REAL
        TEST_TITLE("cancel error");

        #define TEST_PQ_ERROR                                       "test error"
        #define TEST_QUERY                                          "select pg_sleep(3000)"

        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_SENDQUERY, .param = "[\"" TEST_QUERY "\"]", .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT, .sleep = 600},
            {.function = HRNPQ_ISBUSY, .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT},
            {.function = HRNPQ_ISBUSY, .resultInt = 1},
            {.function = HRNPQ_GETCANCEL},
            {.function = HRNPQ_CANCEL, .resultInt = 0, .resultZ = TEST_PQ_ERROR},
            {.function = HRNPQ_FREECANCEL},
            {.function = NULL}
        });

        TEST_ERROR(
            pgClientQuery(client, STRDEF(TEST_QUERY)), DbQueryError, "unable to cancel query '" TEST_QUERY "': " TEST_PQ_ERROR);

        #undef TEST_PQ_ERROR
        #undef TEST_QUERY
#endif

        // -------------------------------------------------------------------------------------------------------------------------
#ifndef HARNESS_PQ_REAL
        TEST_TITLE("PQgetCancel() returns NULL");

        #define TEST_QUERY                                          "select 1"

        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_SENDQUERY, .param = "[\"" TEST_QUERY "\"]", .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT, .sleep = 600},
            {.function = HRNPQ_ISBUSY, .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT},
            {.function = HRNPQ_ISBUSY, .resultInt = 1},
            {.function = HRNPQ_GETCANCEL, .resultNull = true},
            {.function = NULL}
        });

        TEST_ERROR(
            pgClientQuery(client, STRDEF(TEST_QUERY)), DbQueryError,
            "unable to cancel query '" TEST_QUERY "': connection was lost");

        #undef TEST_QUERY
#endif

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("execute do block and raise notice");

        #define TEST_QUERY                                          "do $$ begin raise notice 'mememe'; end $$"

#ifndef HARNESS_PQ_REAL
        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_SENDQUERY, .param = "[\"" TEST_QUERY "\"]", .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT},
            {.function = HRNPQ_ISBUSY},
            {.function = HRNPQ_GETRESULT},
            {.function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},
            {.function = HRNPQ_CLEAR},
            {.function = HRNPQ_GETRESULT, .resultNull = true},
            {.function = NULL}
        });
#endif

        TEST_RESULT_PTR(pgClientQuery(client, STRDEF(TEST_QUERY)), NULL, "execute do block");

        #undef TEST_QUERY

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("unsupported type");

        #define TEST_QUERY                                          "select clock_timestamp()"

#ifndef HARNESS_PQ_REAL
        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_SENDQUERY, .param = "[\"" TEST_QUERY "\"]", .resultInt = 1},
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

        TEST_ERROR(
            pgClientQuery(client, STRDEF(TEST_QUERY)), FormatError,
            "unable to parse type 1184 in column 0 for query '" TEST_QUERY "'");

        #undef TEST_QUERY

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("successful query");

        #define TEST_QUERY                                                                                                         \
            "select oid, case when relname = 'pg_class' then null::text else '' end, relname, relname = 'pg_class'"                \
            "  from pg_class where relname in ('pg_class', 'pg_proc')"                                                             \
            " order by relname"

#ifndef HARNESS_PQ_REAL
        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_SENDQUERY, .param = "[\"" TEST_QUERY "\"]", .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT},
            {.function = HRNPQ_ISBUSY},
            {.function = HRNPQ_GETRESULT},
            {.function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},

            {.function = HRNPQ_NTUPLES, .resultInt = 2},
            {.function = HRNPQ_NFIELDS, .resultInt = 4},
            {.function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_OID},
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

        TEST_RESULT_STR_Z(
            jsonFromVar(varNewVarLst(pgClientQuery(client, STRDEF(TEST_QUERY)))),
            "[[1259,null,\"pg_class\",true],[1255,\"\",\"pg_proc\",false]]", "simple query");

        #undef TEST_QUERY

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("close connection");

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
