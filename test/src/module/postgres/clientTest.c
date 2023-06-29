/***********************************************************************************************************************************
Test PostgreSQL Client

This test can be run two ways:

1) The default uses a libpq shim to simulate a PostgreSQL connection. This will work with all VM types.

2) Optionally use a real cluster for testing (only works on Debian) add `define: -DHARNESS_PQ_REAL` to the postgres/client test in
define.yaml. THe PostgreSQL version can be adjusted by changing TEST_PG_VERSION.
***********************************************************************************************************************************/
#include "common/type/json.h"

#include "common/harnessPack.h"
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
            pgClientQuery(client, STRDEF(TEST_QUERY), pgClientQueryResultRow), DbQueryError,
            "unable to send query '" TEST_QUERY "': " TEST_PQ_ERROR);

        TEST_RESULT_VOID(pgClientFree(client), "free client");

        #undef TEST_PQ_ERROR
        #undef TEST_QUERY

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("connect");

#ifndef HARNESS_PQ_REAL
        harnessPqScriptSet((HarnessPq [])
        {
            {
                .function = HRNPQ_CONNECTDB,
                .param = "[\"dbname='postgres' port=5432 user='" TEST_USER "' host='/var/run/postgresql'\"]",
            },
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
            pgClientQuery(client, STRDEF(TEST_QUERY), pgClientQueryResultRow), DbQueryError,
            "unable to execute query '" TEST_QUERY "': " TEST_PQ_ERROR);

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

        TEST_ERROR(
            pgClientQuery(client, STRDEF(TEST_QUERY), pgClientQueryResultColumn), DbQueryError,
            "query '" TEST_QUERY "' timed out after 500ms");

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
            pgClientQuery(client, STRDEF(TEST_QUERY), pgClientQueryResultColumn), DbQueryError,
            "unable to cancel query '" TEST_QUERY "': " TEST_PQ_ERROR);

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
            pgClientQuery(client, STRDEF(TEST_QUERY), pgClientQueryResultColumn), DbQueryError,
            "unable to cancel query 'select 1': connection was lost");

        #undef TEST_QUERY
#endif

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when results expected");

        #define TEST_QUERY                                          "set client_encoding = 'UTF8'"

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

        TEST_ERROR(
            pgClientQuery(client, STRDEF(TEST_QUERY), pgClientQueryResultColumn), DbQueryError,
            "result expected from '" TEST_QUERY "'");

        #undef TEST_QUERY

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when no results expected");

        #define TEST_QUERY                                          "select * from pg_class limit 1"

#ifndef HARNESS_PQ_REAL
        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_SENDQUERY, .param = "[\"" TEST_QUERY "\"]", .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT},
            {.function = HRNPQ_ISBUSY},
            {.function = HRNPQ_GETRESULT},
            {.function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},
            {.function = HRNPQ_CLEAR},
            {.function = HRNPQ_GETRESULT, .resultNull = true},
            {.function = NULL}
        });
#endif

        TEST_ERROR(
            pgClientQuery(client, STRDEF(TEST_QUERY), pgClientQueryResultNone), DbQueryError,
            "no result expected from '" TEST_QUERY "'");

        #undef TEST_QUERY

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("set with no results");

        #define TEST_QUERY                                          "set client_encoding = 'UTF8'"

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

        TEST_RESULT_PTR(pgClientQuery(client, STRDEF(TEST_QUERY), pgClientQueryResultAny), NULL, "execute set");

        #undef TEST_QUERY

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

        TEST_RESULT_PTR(pgClientQuery(client, STRDEF(TEST_QUERY), pgClientQueryResultNone), NULL, "execute do block");

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
            pgClientQuery(client, STRDEF(TEST_QUERY), pgClientQueryResultAny), FormatError,
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
            hrnPackToStr(pgClientQuery(client, STRDEF(TEST_QUERY), pgClientQueryResultAny)),
            "1:array:[1:u32:1259, 3:str:pg_class, 4:bool:true], 2:array:[1:u32:1255, 2:str:, 3:str:pg_proc, 4:bool:false]",
            "simple query");

        #undef TEST_QUERY

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when result is not a single row");

        #define TEST_QUERY                                          "select * from pg_class limit 2"

#ifndef HARNESS_PQ_REAL
        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_SENDQUERY, .param = "[\"" TEST_QUERY "\"]", .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT},
            {.function = HRNPQ_ISBUSY},
            {.function = HRNPQ_GETRESULT},
            {.function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},

            {.function = HRNPQ_NTUPLES, .resultInt = 2},
            {.function = HRNPQ_NFIELDS, .resultInt = 1},

            {.function = HRNPQ_CLEAR},
            {.function = HRNPQ_GETRESULT, .resultNull = true},
            {.function = NULL}
        });
#endif

        TEST_ERROR(
            pgClientQuery(client, STRDEF(TEST_QUERY), pgClientQueryResultRow), DbQueryError,
            "expected one row from '" TEST_QUERY "'");

        #undef TEST_QUERY

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("single row result");

        #define TEST_QUERY                                          "select 1259::oid, -9223372036854775807::int8"

#ifndef HARNESS_PQ_REAL
        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_SENDQUERY, .param = "[\"" TEST_QUERY "\"]", .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT},
            {.function = HRNPQ_ISBUSY},
            {.function = HRNPQ_GETRESULT},
            {.function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},

            {.function = HRNPQ_NTUPLES, .resultInt = 1},
            {.function = HRNPQ_NFIELDS, .resultInt = 2},
            {.function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_OID},
            {.function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_INT8},

            {.function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = "1259"},
            {.function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = "-9223372036854775807"},

            {.function = HRNPQ_CLEAR},
            {.function = HRNPQ_GETRESULT, .resultNull = true},
            {.function = NULL}
        });
#endif

        TEST_RESULT_STR_Z(
            hrnPackToStr(pgClientQuery(client, STRDEF(TEST_QUERY), pgClientQueryResultRow)),
            "1:u32:1259, 2:i64:-9223372036854775807", "row result");

        #undef TEST_QUERY

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when result is not a single column");

        #define TEST_QUERY                                          "select * from pg_class limit 1"

#ifndef HARNESS_PQ_REAL
        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_SENDQUERY, .param = "[\"" TEST_QUERY "\"]", .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT},
            {.function = HRNPQ_ISBUSY},
            {.function = HRNPQ_GETRESULT},
            {.function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},

            {.function = HRNPQ_NTUPLES, .resultInt = 1},
            {.function = HRNPQ_NFIELDS, .resultInt = 2},

            {.function = HRNPQ_CLEAR},
            {.function = HRNPQ_GETRESULT, .resultNull = true},
            {.function = NULL}
        });
#endif

        TEST_ERROR(
            pgClientQuery(client, STRDEF(TEST_QUERY), pgClientQueryResultColumn), DbQueryError,
            "expected one column from '" TEST_QUERY "'");

        #undef TEST_QUERY

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("single column result");

        #define TEST_QUERY                                          "select -2147483647::int4"

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
            {.function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_INT4},

            {.function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = "-2147483647"},

            {.function = HRNPQ_CLEAR},
            {.function = HRNPQ_GETRESULT, .resultNull = true},
            {.function = NULL}
        });
#endif

        TEST_RESULT_STR_Z(
            hrnPackToStr(pgClientQuery(client, STRDEF(TEST_QUERY), pgClientQueryResultColumn)), "1:i32:-2147483647",
            "column result");

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
        TEST_RESULT_VOID(pgClientFree(client), "free client");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
