/***********************************************************************************************************************************
Test Database
***********************************************************************************************************************************/
#include "common/harnessConfig.h"
#include "common/harnessLog.h"
#include "common/harnessPq.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("dbGet()"))
    {
        DbGetResult result = {0};

        // Error connecting to primary
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--repo1-retention-full=1");
        strLstAddZ(argList, "--pg1-path=/path/to/pg");
        strLstAddZ(argList, "backup");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_CONNECTDB, .param = "[\"dbname='postgres' port=5432\"]"},
            {.function = HRNPQ_STATUS, .resultInt = CONNECTION_BAD},
            {.function = HRNPQ_ERRORMESSAGE, .resultZ = "error"},
            {.function = HRNPQ_FINISH},
            {.function = NULL}
        });

        TEST_ERROR(dbGet(true), DbConnectError, "unable to find primary cluster - cannot proceed");
        harnessLogResult(
            "P00   WARN: unable to check pg-1: [DbConnectError] unable to connect to 'dbname='postgres' port=5432': error");

        // Only cluster is a standby
        // -------------------------------------------------------------------------------------------------------------------------
        harnessPqScriptSet((HarnessPq [])
        {
            // pg-1 connect
            {.function = HRNPQ_CONNECTDB, .param = "[\"dbname='postgres' port=5432\"]"},
            {.function = HRNPQ_STATUS, .resultInt = CONNECTION_OK},

            // pg-1 set search_path
            {.function = HRNPQ_SENDQUERY, .param = "[\"set search_path = 'pg_catalog'\"]", .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT},
            {.function = HRNPQ_ISBUSY},
            {.function = HRNPQ_GETRESULT},
            {.function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},
            {.function = HRNPQ_CLEAR},
            {.function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-1 validate query
            {.function = HRNPQ_SENDQUERY, .param =
                "[\"select (select setting from pg_settings where name = 'server_version_num')::int4,"
                    " (select setting from pg_settings where name = 'data_directory')::text\"]",
                .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT},
            {.function = HRNPQ_ISBUSY},
            {.function = HRNPQ_GETRESULT},
            {.function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},
            {.function = HRNPQ_NTUPLES, .resultInt = 1},
            {.function = HRNPQ_NFIELDS, .resultInt = 2},
            {.function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_INT},
            {.function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_TEXT},
            {.function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = "90123"},
            {.function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = "/pgdata"},
            {.function = HRNPQ_CLEAR},
            {.function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-1 set application_name
            {.function = HRNPQ_SENDQUERY, .param = "[\"set application_name = 'dude'\"]", .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT},
            {.function = HRNPQ_ISBUSY},
            {.function = HRNPQ_GETRESULT},
            {.function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},
            {.function = HRNPQ_CLEAR},
            {.function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-1 standby query
            {.function = HRNPQ_SENDQUERY, .param = "[\"select pg_catalog.pg_is_in_recovery()\"]", .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT},
            {.function = HRNPQ_ISBUSY},
            {.function = HRNPQ_GETRESULT},
            {.function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},
            {.function = HRNPQ_NTUPLES, .resultInt = 1},
            {.function = HRNPQ_NFIELDS, .resultInt = 1},
            {.function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_BOOL},
            {.function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = "t"},
            {.function = HRNPQ_CLEAR},
            {.function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-1 disconnect
            {.function = HRNPQ_FINISH},
            {.function = NULL}
        });

        TEST_ERROR(dbGet(true), DbConnectError, "unable to find primary cluster - cannot proceed");

        // Primary cluster found
        // -------------------------------------------------------------------------------------------------------------------------
        harnessPqScriptSet((HarnessPq [])
        {
            // pg-1 connect
            {.function = HRNPQ_CONNECTDB, .param = "[\"dbname='postgres' port=5432\"]"},
            {.function = HRNPQ_STATUS, .resultInt = CONNECTION_OK},

            // pg-1 set search_path
            {.function = HRNPQ_SENDQUERY, .param = "[\"set search_path = 'pg_catalog'\"]", .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT},
            {.function = HRNPQ_ISBUSY},
            {.function = HRNPQ_GETRESULT},
            {.function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},
            {.function = HRNPQ_CLEAR},
            {.function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-1 validate query
            {.function = HRNPQ_SENDQUERY, .param =
                "[\"select (select setting from pg_settings where name = 'server_version_num')::int4,"
                    " (select setting from pg_settings where name = 'data_directory')::text\"]",
                .resultInt = 1},
            {.function = HRNPQ_CONSUMEINPUT},
            {.function = HRNPQ_ISBUSY},
            {.function = HRNPQ_GETRESULT},
            {.function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},
            {.function = HRNPQ_NTUPLES, .resultInt = 1},
            {.function = HRNPQ_NFIELDS, .resultInt = 2},
            {.function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_INT},
            {.function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_TEXT},
            {.function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = "80417"},
            {.function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = "/pgdata"},
            {.function = HRNPQ_CLEAR},
            {.function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-1 disconnect
            {.function = HRNPQ_FINISH},
            {.function = NULL}
        });

        TEST_ASSIGN(result, dbGet(true), "get primary only");

        TEST_RESULT_INT(result.primaryId, 1, "    check primary id");
        TEST_RESULT_BOOL(result.primary != NULL, true, "    check primary");
        TEST_RESULT_INT(result.standbyId, 0, "    check standby id");
        TEST_RESULT_BOOL(result.standby == NULL, true, "    check standby");

        TEST_RESULT_VOID(dbFree(result.primary), "free primary");

        // More than one primary found
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--repo1-retention-full=1");
        strLstAddZ(argList, "--pg1-path=/path/to/pg1");
        strLstAddZ(argList, "--pg8-path=/path/to/pg2");
        strLstAddZ(argList, "--pg8-port=5433");
        strLstAddZ(argList, "backup");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        harnessPqScriptSet((HarnessPq [])
        {
            // pg-1 connect
            {.session = 1, .function = HRNPQ_CONNECTDB, .param = "[\"dbname='postgres' port=5432\"]"},
            {.session = 1, .function = HRNPQ_STATUS, .resultInt = CONNECTION_OK},

            // pg-1 set search_path
            {.session = 1, .function = HRNPQ_SENDQUERY, .param = "[\"set search_path = 'pg_catalog'\"]", .resultInt = 1},
            {.session = 1, .function = HRNPQ_CONSUMEINPUT},
            {.session = 1, .function = HRNPQ_ISBUSY},
            {.session = 1, .function = HRNPQ_GETRESULT},
            {.session = 1, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},
            {.session = 1, .function = HRNPQ_CLEAR},
            {.session = 1, .function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-1 validate query
            {.session = 1, .function = HRNPQ_SENDQUERY, .param =
                "[\"select (select setting from pg_settings where name = 'server_version_num')::int4,"
                    " (select setting from pg_settings where name = 'data_directory')::text\"]",
                .resultInt = 1},
            {.session = 1, .function = HRNPQ_CONSUMEINPUT},
            {.session = 1, .function = HRNPQ_ISBUSY},
            {.session = 1, .function = HRNPQ_GETRESULT},
            {.session = 1, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},
            {.session = 1, .function = HRNPQ_NTUPLES, .resultInt = 1},
            {.session = 1, .function = HRNPQ_NFIELDS, .resultInt = 2},
            {.session = 1, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_INT},
            {.session = 1, .function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_TEXT},
            {.session = 1, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = "80417"},
            {.session = 1, .function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = "/pgdata"},
            {.session = 1, .function = HRNPQ_CLEAR},
            {.session = 1, .function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-8 connect
            {.session = 8, .function = HRNPQ_CONNECTDB, .param = "[\"dbname='postgres' port=5433\"]"},
            {.session = 8, .function = HRNPQ_STATUS, .resultInt = CONNECTION_OK},

            // pg-8 set search_path
            {.session = 8, .function = HRNPQ_SENDQUERY, .param = "[\"set search_path = 'pg_catalog'\"]", .resultInt = 1},
            {.session = 8, .function = HRNPQ_CONSUMEINPUT},
            {.session = 8, .function = HRNPQ_ISBUSY},
            {.session = 8, .function = HRNPQ_GETRESULT},
            {.session = 8, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},
            {.session = 8, .function = HRNPQ_CLEAR},
            {.session = 8, .function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-8 validate query
            {.session = 8, .function = HRNPQ_SENDQUERY, .param =
                "[\"select (select setting from pg_settings where name = 'server_version_num')::int4,"
                    " (select setting from pg_settings where name = 'data_directory')::text\"]",
                .resultInt = 1},
            {.session = 8, .function = HRNPQ_CONSUMEINPUT},
            {.session = 8, .function = HRNPQ_ISBUSY},
            {.session = 8, .function = HRNPQ_GETRESULT},
            {.session = 8, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},
            {.session = 8, .function = HRNPQ_NTUPLES, .resultInt = 1},
            {.session = 8, .function = HRNPQ_NFIELDS, .resultInt = 2},
            {.session = 8, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_INT},
            {.session = 8, .function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_TEXT},
            {.session = 8, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = "80417"},
            {.session = 8, .function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = "/pgdata"},
            {.session = 8, .function = HRNPQ_CLEAR},
            {.session = 8, .function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-1 disconnect
            {.session = 1, .function = HRNPQ_FINISH},

            // pg-8 disconnect
            {.session = 8, .function = HRNPQ_FINISH},

            {.function = NULL}
        });

        TEST_ERROR(dbGet(true), DbConnectError, "more than one primary cluster found");

        // Two standbys found but no primary
        // -------------------------------------------------------------------------------------------------------------------------
        harnessPqScriptSet((HarnessPq [])
        {
            // pg-1 connect
            {.session = 1, .function = HRNPQ_CONNECTDB, .param = "[\"dbname='postgres' port=5432\"]"},
            {.session = 1, .function = HRNPQ_STATUS, .resultInt = CONNECTION_OK},

            // pg-1 set search_path
            {.session = 1, .function = HRNPQ_SENDQUERY, .param = "[\"set search_path = 'pg_catalog'\"]", .resultInt = 1},
            {.session = 1, .function = HRNPQ_CONSUMEINPUT},
            {.session = 1, .function = HRNPQ_ISBUSY},
            {.session = 1, .function = HRNPQ_GETRESULT},
            {.session = 1, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},
            {.session = 1, .function = HRNPQ_CLEAR},
            {.session = 1, .function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-1 validate query
            {.session = 1, .function = HRNPQ_SENDQUERY, .param =
                "[\"select (select setting from pg_settings where name = 'server_version_num')::int4,"
                    " (select setting from pg_settings where name = 'data_directory')::text\"]",
                .resultInt = 1},
            {.session = 1, .function = HRNPQ_CONSUMEINPUT},
            {.session = 1, .function = HRNPQ_ISBUSY},
            {.session = 1, .function = HRNPQ_GETRESULT},
            {.session = 1, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},
            {.session = 1, .function = HRNPQ_NTUPLES, .resultInt = 1},
            {.session = 1, .function = HRNPQ_NFIELDS, .resultInt = 2},
            {.session = 1, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_INT},
            {.session = 1, .function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_TEXT},
            {.session = 1, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = "90417"},
            {.session = 1, .function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = "/pgdata"},
            {.session = 1, .function = HRNPQ_CLEAR},
            {.session = 1, .function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-1 set application_name
            {.session = 1, .function = HRNPQ_SENDQUERY, .param = "[\"set application_name = 'dude'\"]", .resultInt = 1},
            {.session = 1, .function = HRNPQ_CONSUMEINPUT},
            {.session = 1, .function = HRNPQ_ISBUSY},
            {.session = 1, .function = HRNPQ_GETRESULT},
            {.session = 1, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},
            {.session = 1, .function = HRNPQ_CLEAR},
            {.session = 1, .function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-1 standby query
            {.session = 1, .function = HRNPQ_SENDQUERY, .param = "[\"select pg_catalog.pg_is_in_recovery()\"]", .resultInt = 1},
            {.session = 1, .function = HRNPQ_CONSUMEINPUT},
            {.session = 1, .function = HRNPQ_ISBUSY},
            {.session = 1, .function = HRNPQ_GETRESULT},
            {.session = 1, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},
            {.session = 1, .function = HRNPQ_NTUPLES, .resultInt = 1},
            {.session = 1, .function = HRNPQ_NFIELDS, .resultInt = 1},
            {.session = 1, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_BOOL},
            {.session = 1, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = "t"},
            {.session = 1, .function = HRNPQ_CLEAR},
            {.session = 1, .function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-8 connect
            {.session = 8, .function = HRNPQ_CONNECTDB, .param = "[\"dbname='postgres' port=5433\"]"},
            {.session = 8, .function = HRNPQ_STATUS, .resultInt = CONNECTION_OK},

            // pg-8 set search_path
            {.session = 8, .function = HRNPQ_SENDQUERY, .param = "[\"set search_path = 'pg_catalog'\"]", .resultInt = 1},
            {.session = 8, .function = HRNPQ_CONSUMEINPUT},
            {.session = 8, .function = HRNPQ_ISBUSY},
            {.session = 8, .function = HRNPQ_GETRESULT},
            {.session = 8, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},
            {.session = 8, .function = HRNPQ_CLEAR},
            {.session = 8, .function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-8 validate query
            {.session = 8, .function = HRNPQ_SENDQUERY, .param =
                "[\"select (select setting from pg_settings where name = 'server_version_num')::int4,"
                    " (select setting from pg_settings where name = 'data_directory')::text\"]",
                .resultInt = 1},
            {.session = 8, .function = HRNPQ_CONSUMEINPUT},
            {.session = 8, .function = HRNPQ_ISBUSY},
            {.session = 8, .function = HRNPQ_GETRESULT},
            {.session = 8, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},
            {.session = 8, .function = HRNPQ_NTUPLES, .resultInt = 1},
            {.session = 8, .function = HRNPQ_NFIELDS, .resultInt = 2},
            {.session = 8, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_INT},
            {.session = 8, .function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_TEXT},
            {.session = 8, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = "90417"},
            {.session = 8, .function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = "/pgdata"},
            {.session = 8, .function = HRNPQ_CLEAR},
            {.session = 8, .function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-8 set application_name
            {.session = 8, .function = HRNPQ_SENDQUERY, .param = "[\"set application_name = 'dude'\"]", .resultInt = 1},
            {.session = 8, .function = HRNPQ_CONSUMEINPUT},
            {.session = 8, .function = HRNPQ_ISBUSY},
            {.session = 8, .function = HRNPQ_GETRESULT},
            {.session = 8, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},
            {.session = 8, .function = HRNPQ_CLEAR},
            {.session = 8, .function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-8 standby query
            {.session = 8, .function = HRNPQ_SENDQUERY, .param = "[\"select pg_catalog.pg_is_in_recovery()\"]", .resultInt = 1},
            {.session = 8, .function = HRNPQ_CONSUMEINPUT},
            {.session = 8, .function = HRNPQ_ISBUSY},
            {.session = 8, .function = HRNPQ_GETRESULT},
            {.session = 8, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},
            {.session = 8, .function = HRNPQ_NTUPLES, .resultInt = 1},
            {.session = 8, .function = HRNPQ_NFIELDS, .resultInt = 1},
            {.session = 8, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_BOOL},
            {.session = 8, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = "t"},
            {.session = 8, .function = HRNPQ_CLEAR},
            {.session = 8, .function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-8 disconnect
            {.session = 8, .function = HRNPQ_FINISH},

            // pg-1 disconnect
            {.session = 1, .function = HRNPQ_FINISH},

            {.function = NULL}
        });

        TEST_ERROR(dbGet(false), DbConnectError, "unable to find primary cluster - cannot proceed");

        // Primary and standby found
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--repo1-retention-full=1");
        strLstAddZ(argList, "--pg1-path=/path/to/pg1");
        strLstAddZ(argList, "--pg4-path=/path/to/pg4");
        strLstAddZ(argList, "--pg4-port=5433");
        strLstAddZ(argList, "--pg5-host=localhost");
        strLstAdd(argList, strNewFmt("--pg5-host-user=%s", testUser()));
        strLstAddZ(argList, "--pg5-path=/path/to/pg5");
        strLstAddZ(argList, "--pg8-path=/path/to/pg8");
        strLstAddZ(argList, "--pg8-port=5434");
        strLstAddZ(argList, "backup");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        harnessPqScriptSet((HarnessPq [])
        {
            // pg-1 connect
            {.session = 1, .function = HRNPQ_CONNECTDB, .param = "[\"dbname='postgres' port=5432\"]"},
            {.session = 1, .function = HRNPQ_STATUS, .resultInt = CONNECTION_OK},

            // pg-1 set search_path
            {.session = 1, .function = HRNPQ_SENDQUERY, .param = "[\"set search_path = 'pg_catalog'\"]", .resultInt = 1},
            {.session = 1, .function = HRNPQ_CONSUMEINPUT},
            {.session = 1, .function = HRNPQ_ISBUSY},
            {.session = 1, .function = HRNPQ_GETRESULT},
            {.session = 1, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},
            {.session = 1, .function = HRNPQ_CLEAR},
            {.session = 1, .function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-1 validate query
            {.session = 1, .function = HRNPQ_SENDQUERY, .param =
                "[\"select (select setting from pg_settings where name = 'server_version_num')::int4,"
                    " (select setting from pg_settings where name = 'data_directory')::text\"]",
                .resultInt = 1},
            {.session = 1, .function = HRNPQ_CONSUMEINPUT},
            {.session = 1, .function = HRNPQ_ISBUSY},
            {.session = 1, .function = HRNPQ_GETRESULT},
            {.session = 1, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},
            {.session = 1, .function = HRNPQ_NTUPLES, .resultInt = 1},
            {.session = 1, .function = HRNPQ_NFIELDS, .resultInt = 2},
            {.session = 1, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_INT},
            {.session = 1, .function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_TEXT},
            {.session = 1, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = "90417"},
            {.session = 1, .function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = "/pgdata"},
            {.session = 1, .function = HRNPQ_CLEAR},
            {.session = 1, .function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-1 set application_name
            {.session = 1, .function = HRNPQ_SENDQUERY, .param = "[\"set application_name = 'dude'\"]", .resultInt = 1},
            {.session = 1, .function = HRNPQ_CONSUMEINPUT},
            {.session = 1, .function = HRNPQ_ISBUSY},
            {.session = 1, .function = HRNPQ_GETRESULT},
            {.session = 1, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},
            {.session = 1, .function = HRNPQ_CLEAR},
            {.session = 1, .function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-1 standby query
            {.session = 1, .function = HRNPQ_SENDQUERY, .param = "[\"select pg_catalog.pg_is_in_recovery()\"]", .resultInt = 1},
            {.session = 1, .function = HRNPQ_CONSUMEINPUT},
            {.session = 1, .function = HRNPQ_ISBUSY},
            {.session = 1, .function = HRNPQ_GETRESULT},
            {.session = 1, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},
            {.session = 1, .function = HRNPQ_NTUPLES, .resultInt = 1},
            {.session = 1, .function = HRNPQ_NFIELDS, .resultInt = 1},
            {.session = 1, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_BOOL},
            {.session = 1, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = "t"},
            {.session = 1, .function = HRNPQ_CLEAR},
            {.session = 1, .function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-4 error
            {.session = 4, .function = HRNPQ_CONNECTDB, .param = "[\"dbname='postgres' port=5433\"]"},
            {.session = 4, .function = HRNPQ_STATUS, .resultInt = CONNECTION_BAD},
            {.session = 4, .function = HRNPQ_ERRORMESSAGE, .resultZ = "error"},
            {.session = 4, .function = HRNPQ_FINISH},

            // pg-8 connect
            {.session = 8, .function = HRNPQ_CONNECTDB, .param = "[\"dbname='postgres' port=5434\"]"},
            {.session = 8, .function = HRNPQ_STATUS, .resultInt = CONNECTION_OK},

            // pg-8 set search_path
            {.session = 8, .function = HRNPQ_SENDQUERY, .param = "[\"set search_path = 'pg_catalog'\"]", .resultInt = 1},
            {.session = 8, .function = HRNPQ_CONSUMEINPUT},
            {.session = 8, .function = HRNPQ_ISBUSY},
            {.session = 8, .function = HRNPQ_GETRESULT},
            {.session = 8, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},
            {.session = 8, .function = HRNPQ_CLEAR},
            {.session = 8, .function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-8 validate query
            {.session = 8, .function = HRNPQ_SENDQUERY, .param =
                "[\"select (select setting from pg_settings where name = 'server_version_num')::int4,"
                    " (select setting from pg_settings where name = 'data_directory')::text\"]",
                .resultInt = 1},
            {.session = 8, .function = HRNPQ_CONSUMEINPUT},
            {.session = 8, .function = HRNPQ_ISBUSY},
            {.session = 8, .function = HRNPQ_GETRESULT},
            {.session = 8, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},
            {.session = 8, .function = HRNPQ_NTUPLES, .resultInt = 1},
            {.session = 8, .function = HRNPQ_NFIELDS, .resultInt = 2},
            {.session = 8, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_INT},
            {.session = 8, .function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_TEXT},
            {.session = 8, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = "90417"},
            {.session = 8, .function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = "/pgdata"},
            {.session = 8, .function = HRNPQ_CLEAR},
            {.session = 8, .function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-8 set application_name
            {.session = 8, .function = HRNPQ_SENDQUERY, .param = "[\"set application_name = 'dude'\"]", .resultInt = 1},
            {.session = 8, .function = HRNPQ_CONSUMEINPUT},
            {.session = 8, .function = HRNPQ_ISBUSY},
            {.session = 8, .function = HRNPQ_GETRESULT},
            {.session = 8, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},
            {.session = 8, .function = HRNPQ_CLEAR},
            {.session = 8, .function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-8 standby query
            {.session = 8, .function = HRNPQ_SENDQUERY, .param = "[\"select pg_catalog.pg_is_in_recovery()\"]", .resultInt = 1},
            {.session = 8, .function = HRNPQ_CONSUMEINPUT},
            {.session = 8, .function = HRNPQ_ISBUSY},
            {.session = 8, .function = HRNPQ_GETRESULT},
            {.session = 8, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},
            {.session = 8, .function = HRNPQ_NTUPLES, .resultInt = 1},
            {.session = 8, .function = HRNPQ_NFIELDS, .resultInt = 1},
            {.session = 8, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_BOOL},
            {.session = 8, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = "f"},
            {.session = 8, .function = HRNPQ_CLEAR},
            {.session = 8, .function = HRNPQ_GETRESULT, .resultNull = true},

            // pg-8 disconnect
            {.session = 8, .function = HRNPQ_FINISH},

            // pg-1 disconnect
            {.session = 1, .function = HRNPQ_FINISH},

            {.function = NULL}
        });

        TEST_ASSIGN(result, dbGet(false), "get primary and standy");
        harnessLogResultRegExp(
            "P00   WARN: unable to check pg-4: \\[DbConnectError\\] unable to connect to 'dbname='postgres' port=5433': error\n"
            "P00   WARN: unable to check pg-5: \\[DbConnectError\\] raised from remote-0 protocol on 'localhost':"
                " unable to connect to 'dbname='postgres' port=5432': could not connect to server: No such file or directory.*");

        TEST_RESULT_INT(result.primaryId, 8, "    check primary id");
        TEST_RESULT_BOOL(result.primary != NULL, true, "    check primary");
        TEST_RESULT_INT(result.standbyId, 1, "    check standby id");
        TEST_RESULT_BOOL(result.standby != NULL, true, "    check standby");

        TEST_RESULT_VOID(dbFree(result.primary), "free primary");
        TEST_RESULT_VOID(dbFree(result.standby), "free standby");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
