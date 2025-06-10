/***********************************************************************************************************************************
Pq Test Harness

Scripted testing for PostgreSQL libpq so exact results can be returned for unit testing. See PostgreSQL client unit tests for usage
examples.
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_PQ_H
#define TEST_COMMON_HARNESS_PQ_H

#ifndef HARNESS_PQ_REAL

#include <libpq-fe.h>

#include "common/macro.h"
#include "common/time.h"
#include "version.h"

/***********************************************************************************************************************************
Function constants
***********************************************************************************************************************************/
#define HRN_PQ_CANCEL                                               "PQcancel"
#define HRN_PQ_CLEAR                                                "PQclear"
#define HRN_PQ_CONNECTDB                                            "PQconnectdb"
#define HRN_PQ_CONSUMEINPUT                                         "PQconsumeInput"
#define HRN_PQ_ERRORMESSAGE                                         "PQerrorMessage"
#define HRN_PQ_FINISH                                               "PQfinish"
#define HRN_PQ_FREECANCEL                                           "PQfreeCancel"
#define HRN_PQ_FTYPE                                                "PQftype"
#define HRN_PQ_GETCANCEL                                            "PQgetCancel"
#define HRN_PQ_GETISNULL                                            "PQgetisnull"
#define HRN_PQ_GETRESULT                                            "PQgetResult"
#define HRN_PQ_GETVALUE                                             "PQgetvalue"
#define HRN_PQ_ISBUSY                                               "PQisbusy"
#define HRN_PQ_NFIELDS                                              "PQnfields"
#define HRN_PQ_NTUPLES                                              "PQntuples"
#define HRN_PQ_RESULTERRORMESSAGE                                   "PQresultErrorMessage"
#define HRN_PQ_RESULTSTATUS                                         "PQresultStatus"
#define HRN_PQ_SENDQUERY                                            "PQsendQuery"
#define HRN_PQ_SOCKET                                               "PQsocket"
#define HRN_PQ_STATUS                                               "PQstatus"

/***********************************************************************************************************************************
Data type constants
***********************************************************************************************************************************/
#define HRN_PQ_TYPE_BOOL                                            16
#define HRN_PQ_TYPE_INT4                                            23
#define HRN_PQ_TYPE_INT8                                            20
#define HRN_PQ_TYPE_OID                                             26
#define HRN_PQ_TYPE_TEXT                                            25

/***********************************************************************************************************************************
Macros for defining groups of functions that implement various queries and commands
***********************************************************************************************************************************/
#define HRN_PQ_SCRIPT_OPEN(sessionParam, connectParam)                                                                             \
    {.session = sessionParam, .function = HRN_PQ_CONNECTDB, .param = "[\"" connectParam "\"]"},                                    \
    {.session = sessionParam, .function = HRN_PQ_STATUS, .resultInt = CONNECTION_OK}

#define HRN_PQ_SCRIPT_SET_CLIENT_ENCODING(sessionParam)                                                                            \
    {.session = sessionParam, .function = HRN_PQ_SENDQUERY, .param = "[\"set client_encoding = 'UTF8'\"]", .resultInt = 1},        \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},                                     \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_SET_SEARCH_PATH(sessionParam)                                                                                \
    {.session = sessionParam, .function = HRN_PQ_SENDQUERY, .param = "[\"set search_path = 'pg_catalog'\"]", .resultInt = 1},      \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},                                     \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_VALIDATE_QUERY(sessionParam, versionParam, pgPathParam, archiveMode, archiveCommand)                         \
    {.session = sessionParam, .function = HRN_PQ_SENDQUERY, .param =                                                               \
        "[\"select (select setting from pg_catalog.pg_settings where name = 'server_version_num')::int4,"                          \
            " (select setting from pg_catalog.pg_settings where name = 'data_directory')::text,"                                   \
            " (select setting from pg_catalog.pg_settings where name = 'archive_mode')::text,"                                     \
            " (select setting from pg_catalog.pg_settings where name = 'archive_command')::text,"                                  \
            " (select setting from pg_catalog.pg_settings where name = 'checkpoint_timeout')::int4\"]",                            \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                      \
    {.session = sessionParam, .function = HRN_PQ_NTUPLES, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_NFIELDS, .resultInt = 5},                                                         \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_INT4},                            \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[1]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[2]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[3]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[4]", .resultInt = HRN_PQ_TYPE_INT4},                            \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,0]", .resultZ = zNewFmt("%d", (int)versionParam)},         \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,1]", .resultZ = pgPathParam},                              \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,2]", .resultZ = archiveMode == NULL ? "on"                 \
        : archiveMode},                                                                                                            \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,3]", .resultZ = archiveCommand == NULL ? PROJECT_BIN       \
        : archiveCommand},                                                                                                         \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,4]", .resultZ = STRINGIFY(300)},                           \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_SET_APPLICATION_NAME(sessionParam)                                                                           \
    {.session = sessionParam, .function = HRN_PQ_SENDQUERY,                                                                        \
        .param = zNewFmt("[\"set application_name = '" PROJECT_NAME " [%s]'\"]", cfgCommandName()),                                \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},                                     \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_SET_MAX_PARALLEL_WORKERS_PER_GATHER(sessionParam)                                                            \
    {.session = sessionParam, .function = HRN_PQ_SENDQUERY, .param = "[\"set max_parallel_workers_per_gather = 0\"]",              \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},                                     \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_IS_STANDBY_QUERY(sessionParam, standbyParam)                                                                 \
    {.session = sessionParam, .function = HRN_PQ_SENDQUERY, .param = "[\"select pg_catalog.pg_is_in_recovery()\"]",                \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                      \
    {.session = sessionParam, .function = HRN_PQ_NTUPLES, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_NFIELDS, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_BOOL},                            \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,0]", .resultZ = STRINGIFY(standbyParam)},                  \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_CREATE_RESTORE_POINT(sessionParam, lsnParam)                                                                 \
    {.session = sessionParam,                                                                                                      \
        .function = HRN_PQ_SENDQUERY, .param = "[\"select pg_catalog.pg_create_restore_point('pgBackRest Archive Check')::text\"]",\
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                      \
    {.session = sessionParam, .function = HRN_PQ_NTUPLES, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_NFIELDS, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,0]", .resultZ = lsnParam},                                 \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_CURRENT_WAL_LE_96(sessionParam, walSegmentParam)                                                             \
    {.session = sessionParam,                                                                                                      \
        .function = HRN_PQ_SENDQUERY,                                                                                              \
        .param = "[\"select pg_catalog.pg_xlogfile_name(pg_catalog.pg_current_xlog_insert_location())::text\"]", .resultInt = 1},  \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                      \
    {.session = sessionParam, .function = HRN_PQ_NTUPLES, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_NFIELDS, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,0]", .resultZ = walSegmentParam},                          \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_CURRENT_WAL_GE_10(sessionParam, walSegmentParam)                                                             \
    {.session = sessionParam,                                                                                                      \
        .function = HRN_PQ_SENDQUERY,                                                                                              \
        .param = "[\"select pg_catalog.pg_walfile_name(pg_catalog.pg_current_wal_insert_lsn())::text\"]", .resultInt = 1},         \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                      \
    {.session = sessionParam, .function = HRN_PQ_NTUPLES, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_NFIELDS, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,0]", .resultZ = walSegmentParam},                          \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_WAL_SWITCH(sessionParam, walNameParam, walFileNameParam)                                                     \
    {.session = sessionParam, .function = HRN_PQ_SENDQUERY,                                                                        \
        .param = "[\"select pg_catalog.pg_" walNameParam "file_name(pg_catalog.pg_switch_" walNameParam "())::text\"]",            \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                      \
    {.session = sessionParam, .function = HRN_PQ_NTUPLES, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_NFIELDS, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,0]", .resultZ = walFileNameParam},                         \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_TIME_QUERY(sessionParam, timeParam)                                                                          \
    {.session = sessionParam,                                                                                                      \
        .function = HRN_PQ_SENDQUERY, .param = "[\"select (extract(epoch from clock_timestamp()) * 1000)::bigint\"]",              \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                      \
    {.session = sessionParam, .function = HRN_PQ_NTUPLES, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_NFIELDS, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_INT8},                            \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,0]",                                                       \
        .resultZ = zNewFmt("%" PRId64, (int64_t)(timeParam))},                                                                     \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_ADVISORY_LOCK(sessionParam, lockAcquiredParam)                                                               \
    {.session = sessionParam,                                                                                                      \
        .function = HRN_PQ_SENDQUERY, .param = "[\"select pg_catalog.pg_try_advisory_lock(12340078987004321)::bool\"]",            \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                      \
    {.session = sessionParam, .function = HRN_PQ_NTUPLES, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_NFIELDS, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_BOOL},                            \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,0]", .resultZ = cvtBoolToConstZ(lockAcquiredParam)},       \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_IS_IN_BACKUP(sessionParam, inBackupParam)                                                                    \
    {.session = sessionParam,                                                                                                      \
        .function = HRN_PQ_SENDQUERY, .param = "[\"select pg_catalog.pg_is_in_backup()::bool\"]",                                  \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                      \
    {.session = sessionParam, .function = HRN_PQ_NTUPLES, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_NFIELDS, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_BOOL},                            \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,0]", .resultZ = cvtBoolToConstZ(inBackupParam)},           \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_START_BACKUP_LE_95(sessionParam, startFastParam, lsnParam, walSegmentNameParam)                              \
    {.session = sessionParam,                                                                                                      \
        .function = HRN_PQ_SENDQUERY,                                                                                              \
        .param = zNewFmt(                                                                                                          \
            "[\"select lsn::text as lsn,\\n"                                                                                       \
            "       pg_catalog.pg_xlogfile_name(lsn)::text as wal_segment_name\\n"                                                 \
            "  from pg_catalog.pg_start_backup('pgBackRest backup started at ' || current_timestamp, %s) as lsn\"]",               \
            cvtBoolToConstZ(startFastParam)),                                                                                      \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                      \
    {.session = sessionParam, .function = HRN_PQ_NTUPLES, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_NFIELDS, .resultInt = 2},                                                         \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[1]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,0]", .resultZ = lsnParam},                                 \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,1]", .resultZ = walSegmentNameParam},                      \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_START_BACKUP_96(sessionParam, startFastParam, lsnParam, walSegmentNameParam)                                 \
    {.session = sessionParam,                                                                                                      \
        .function = HRN_PQ_SENDQUERY,                                                                                              \
        .param = zNewFmt(                                                                                                          \
            "[\"select lsn::text as lsn,\\n"                                                                                       \
            "       pg_catalog.pg_xlogfile_name(lsn)::text as wal_segment_name\\n"                                                 \
            "  from pg_catalog.pg_start_backup('pgBackRest backup started at ' || current_timestamp, %s, false) as lsn\"]",        \
            cvtBoolToConstZ(startFastParam)),                                                                                      \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                      \
    {.session = sessionParam, .function = HRN_PQ_NTUPLES, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_NFIELDS, .resultInt = 2},                                                         \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[1]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,0]", .resultZ = lsnParam},                                 \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,1]", .resultZ = walSegmentNameParam},                      \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_START_BACKUP_GE_10(sessionParam, startFastParam, lsnParam, walSegmentNameParam)                              \
    {.session = sessionParam,                                                                                                      \
        .function = HRN_PQ_SENDQUERY,                                                                                              \
        .param = zNewFmt(                                                                                                          \
            "[\"select lsn::text as lsn,\\n"                                                                                       \
            "       pg_catalog.pg_walfile_name(lsn)::text as wal_segment_name\\n"                                                  \
            "  from pg_catalog.pg_start_backup('pgBackRest backup started at ' || current_timestamp, %s, false) as lsn\"]",        \
            cvtBoolToConstZ(startFastParam)),                                                                                      \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                      \
    {.session = sessionParam, .function = HRN_PQ_NTUPLES, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_NFIELDS, .resultInt = 2},                                                         \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[1]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,0]", .resultZ = lsnParam},                                 \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,1]", .resultZ = walSegmentNameParam},                      \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_START_BACKUP_GE_15(sessionParam, startFastParam, lsnParam, walSegmentNameParam)                              \
    {.session = sessionParam,                                                                                                      \
        .function = HRN_PQ_SENDQUERY,                                                                                              \
        .param = zNewFmt(                                                                                                          \
            "[\"select lsn::text as lsn,\\n"                                                                                       \
            "       pg_catalog.pg_walfile_name(lsn)::text as wal_segment_name\\n"                                                  \
            "  from pg_catalog.pg_backup_start('pgBackRest backup started at ' || current_timestamp, %s) as lsn\"]",               \
            cvtBoolToConstZ(startFastParam)),                                                                                      \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                      \
    {.session = sessionParam, .function = HRN_PQ_NTUPLES, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_NFIELDS, .resultInt = 2},                                                         \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[1]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,0]", .resultZ = lsnParam},                                 \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,1]", .resultZ = walSegmentNameParam},                      \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_STOP_BACKUP_LE_95(sessionParam, lsnParam, walSegmentNameParam)                                               \
    {.session = sessionParam,                                                                                                      \
        .function = HRN_PQ_SENDQUERY,                                                                                              \
        .param =                                                                                                                   \
            "[\"select lsn::text as lsn,\\n"                                                                                       \
            "       pg_catalog.pg_xlogfile_name(lsn)::text as wal_segment_name\\n"                                                 \
            "  from pg_catalog.pg_stop_backup() as lsn\"]",                                                                        \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                      \
    {.session = sessionParam, .function = HRN_PQ_NTUPLES, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_NFIELDS, .resultInt = 2},                                                         \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[1]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,0]", .resultZ = lsnParam},                                 \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,1]", .resultZ = walSegmentNameParam},                      \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_STOP_BACKUP_96(sessionParam, lsnParam, walSegmentNameParam, tablespaceMapParam)                              \
    {.session = sessionParam,                                                                                                      \
        .function = HRN_PQ_SENDQUERY,                                                                                              \
        .param =                                                                                                                   \
            "[\"select lsn::text as lsn,\\n"                                                                                       \
            "       pg_catalog.pg_xlogfile_name(lsn)::text as wal_segment_name,\\n"                                                \
            "       labelfile::text as backuplabel_file,\\n"                                                                       \
            "       spcmapfile::text as tablespacemap_file\\n"                                                                     \
            "  from pg_catalog.pg_stop_backup(false)\"]",                                                                          \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                      \
    {.session = sessionParam, .function = HRN_PQ_NTUPLES, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_NFIELDS, .resultInt = 4},                                                         \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[1]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[2]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[3]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,0]", .resultZ = lsnParam},                                 \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,1]", .resultZ = walSegmentNameParam},                      \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,2]", .resultZ = "BACKUP_LABEL_DATA"},                      \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,3]",                                                       \
        .resultZ = tablespaceMapParam ? "TABLESPACE_MAP_DATA" : "\n"},                                                             \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_STOP_BACKUP_GE_10(sessionParam, lsnParam, walSegmentNameParam, tablespaceMapParam)                           \
    {.session = sessionParam,                                                                                                      \
        .function = HRN_PQ_SENDQUERY,                                                                                              \
        .param =                                                                                                                   \
            "[\"select lsn::text as lsn,\\n"                                                                                       \
            "       pg_catalog.pg_walfile_name(lsn)::text as wal_segment_name,\\n"                                                 \
            "       labelfile::text as backuplabel_file,\\n"                                                                       \
            "       spcmapfile::text as tablespacemap_file\\n"                                                                     \
            "  from pg_catalog.pg_stop_backup(false, false)\"]",                                                                   \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                      \
    {.session = sessionParam, .function = HRN_PQ_NTUPLES, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_NFIELDS, .resultInt = 4},                                                         \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[1]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[2]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[3]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,0]", .resultZ = lsnParam},                                 \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,1]", .resultZ = walSegmentNameParam},                      \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,2]", .resultZ = "BACKUP_LABEL_DATA"},                      \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,3]",                                                       \
        .resultZ = tablespaceMapParam ? "TABLESPACE_MAP_DATA" : "\n"},                                                             \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_STOP_BACKUP_GE_15(sessionParam, lsnParam, walSegmentNameParam, tablespaceMapParam)                           \
    {.session = sessionParam,                                                                                                      \
        .function = HRN_PQ_SENDQUERY,                                                                                              \
        .param =                                                                                                                   \
            "[\"select lsn::text as lsn,\\n"                                                                                       \
            "       pg_catalog.pg_walfile_name(lsn)::text as wal_segment_name,\\n"                                                 \
            "       labelfile::text as backuplabel_file,\\n"                                                                       \
            "       spcmapfile::text as tablespacemap_file\\n"                                                                     \
            "  from pg_catalog.pg_backup_stop(false)\"]",                                                                          \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                      \
    {.session = sessionParam, .function = HRN_PQ_NTUPLES, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_NFIELDS, .resultInt = 4},                                                         \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[1]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[2]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[3]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,0]", .resultZ = lsnParam},                                 \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,1]", .resultZ = walSegmentNameParam},                      \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,2]", .resultZ = "BACKUP_LABEL_DATA"},                      \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,3]",                                                       \
        .resultZ = tablespaceMapParam ? "TABLESPACE_MAP_DATA" : "\n"},                                                             \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_DATABASE_LIST_1(sessionParam, databaseNameParam)                                                             \
    {.session = sessionParam,                                                                                                      \
        .function = HRN_PQ_SENDQUERY,                                                                                              \
        .param =                                                                                                                   \
            "[\"select oid::oid, datname::text, (select oid::oid from pg_catalog.pg_database where datname = 'template0')"         \
            " as datlastsysoid from pg_catalog.pg_database\"]",                                                                    \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                      \
    {.session = sessionParam, .function = HRN_PQ_NTUPLES, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_NFIELDS, .resultInt = 3},                                                         \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_OID},                             \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[1]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[2]", .resultInt = HRN_PQ_TYPE_OID},                             \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,0]", .resultZ = STRINGIFY(16384)},                         \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,1]", .resultZ = databaseNameParam},                        \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,2]", .resultZ = STRINGIFY(13777)},                         \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_TABLESPACE_LIST_0(sessionParam)                                                                              \
    {.session = sessionParam,                                                                                                      \
        .function = HRN_PQ_SENDQUERY, .param = "[\"select oid::oid, spcname::text from pg_catalog.pg_tablespace\"]",               \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                      \
    {.session = sessionParam, .function = HRN_PQ_NTUPLES, .resultInt = 0},                                                         \
    {.session = sessionParam, .function = HRN_PQ_NFIELDS, .resultInt = 2},                                                         \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_OID},                             \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[1]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_TABLESPACE_LIST_1(sessionParam, id1Param, name1Param)                                                        \
    {.session = sessionParam,                                                                                                      \
        .function = HRN_PQ_SENDQUERY, .param = "[\"select oid::oid, spcname::text from pg_catalog.pg_tablespace\"]",               \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                      \
    {.session = sessionParam, .function = HRN_PQ_NTUPLES, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_NFIELDS, .resultInt = 2},                                                         \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_OID},                             \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[1]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,0]", .resultZ = zNewFmt("%d", id1Param)},                  \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,1]", .resultZ = name1Param},                               \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_CHECKPOINT(sessionParam)                                                                                     \
    {.session = sessionParam, .function = HRN_PQ_SENDQUERY, .param = "[\"checkpoint\"]", .resultInt = 1},                          \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},                                     \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define                                                                                                                            \
    HRN_PQ_SCRIPT_REPLAY_TARGET_REACHED(                                                                                           \
        sessionParam, walNameParam, lsnNameParam, targetLsnParam, targetReachedParam, replayLsnParam)                              \
    {.session = sessionParam,                                                                                                      \
        .function = HRN_PQ_SENDQUERY,                                                                                              \
        .param = zNewFmt(                                                                                                          \
            "[\"select replayLsn::text,\\n"                                                                                        \
            "       (replayLsn > '%s')::bool as targetReached\\n"                                                                  \
            "  from pg_catalog.pg_last_" walNameParam "_replay_" lsnNameParam "() as replayLsn\"]", targetLsnParam),               \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                      \
    {.session = sessionParam, .function = HRN_PQ_NTUPLES, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_NFIELDS, .resultInt = 2},                                                         \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[1]", .resultInt = HRN_PQ_TYPE_BOOL},                            \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,0]", .resultZ = replayLsnParam},                           \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,1]", .resultZ = cvtBoolToConstZ(targetReachedParam)},      \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define HRN_PQ_SCRIPT_REPLAY_TARGET_REACHED_LE_96(sessionParam, targetLsnParam, targetReachedParam, reachedLsnParam)               \
    HRN_PQ_SCRIPT_REPLAY_TARGET_REACHED(sessionParam, "xlog", "location", targetLsnParam, targetReachedParam, reachedLsnParam)

#define HRN_PQ_SCRIPT_REPLAY_TARGET_REACHED_GE_10(sessionParam, targetLsnParam, targetReachedParam, reachedLsnParam)               \
    HRN_PQ_SCRIPT_REPLAY_TARGET_REACHED(sessionParam, "wal", "lsn", targetLsnParam, targetReachedParam, reachedLsnParam)

#define                                                                                                                            \
    HRN_PQ_SCRIPT_CHECKPOINT_TARGET_REACHED(                                                                                       \
        sessionParam, lsnNameParam, targetLsnParam, targetReachedParam, checkpointLsnParam, sleepParam)                            \
    {.session = sessionParam,                                                                                                      \
        .function = HRN_PQ_SENDQUERY,                                                                                              \
        .param = zNewFmt(                                                                                                          \
            "[\"select (checkpoint_" lsnNameParam " >= '%s')::bool as targetReached,\\n"                                           \
            "       checkpoint_" lsnNameParam "::text as checkpointLsn\\n"                                                         \
            "  from pg_catalog.pg_control_checkpoint()\"]", targetLsnParam),                                                       \
        .resultInt = 1, .sleep = sleepParam},                                                                                      \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                      \
    {.session = sessionParam, .function = HRN_PQ_NTUPLES, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_NFIELDS, .resultInt = 2},                                                         \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_BOOL},                            \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[1]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,0]", .resultZ = cvtBoolToConstZ(targetReachedParam)},      \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,1]", .resultZ = checkpointLsnParam},                       \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define                                                                                                                            \
    HRN_PQ_SCRIPT_CHECKPOINT_TARGET_REACHED_96(                                                                                    \
        sessionParam, targetLsnParam, targetReachedParam, checkpointLsnParam, sleepParam)                                          \
    HRN_PQ_SCRIPT_CHECKPOINT_TARGET_REACHED(                                                                                       \
        sessionParam, "location", targetLsnParam, targetReachedParam, checkpointLsnParam, sleepParam)

#define                                                                                                                            \
    HRN_PQ_SCRIPT_CHECKPOINT_TARGET_REACHED_GE_10(                                                                                 \
        sessionParam, targetLsnParam, targetReachedParam, checkpointLsnParam, sleepParam)                                          \
    HRN_PQ_SCRIPT_CHECKPOINT_TARGET_REACHED(                                                                                       \
        sessionParam, "lsn", targetLsnParam, targetReachedParam, checkpointLsnParam, sleepParam)

#define HRN_PQ_SCRIPT_REPLAY_WAIT_LE_95(sessionParam, targetLsnParam)                                                              \
    HRN_PQ_SCRIPT_REPLAY_TARGET_REACHED_LE_96(sessionParam, targetLsnParam, true, "X/X"),                                          \
    HRN_PQ_SCRIPT_CHECKPOINT(sessionParam)

#define HRN_PQ_SCRIPT_REPLAY_WAIT_96(sessionParam, targetLsnParam)                                                                 \
    HRN_PQ_SCRIPT_REPLAY_TARGET_REACHED_LE_96(sessionParam, targetLsnParam, true, "X/X"),                                          \
    HRN_PQ_SCRIPT_CHECKPOINT(sessionParam),                                                                                        \
    HRN_PQ_SCRIPT_CHECKPOINT_TARGET_REACHED_96(sessionParam, targetLsnParam, true, "X/X", 0)

#define HRN_PQ_SCRIPT_REPLAY_WAIT_GE_10(sessionParam, targetLsnParam)                                                              \
    HRN_PQ_SCRIPT_REPLAY_TARGET_REACHED_GE_10(sessionParam, targetLsnParam, true, "X/X"),                                          \
    HRN_PQ_SCRIPT_CHECKPOINT(sessionParam),                                                                                        \
    HRN_PQ_SCRIPT_CHECKPOINT_TARGET_REACHED_GE_10(sessionParam, targetLsnParam, true, "X/X", 0)

#define HRN_PQ_SCRIPT_CLOSE(sessionParam)                                                                                          \
    {.session = sessionParam, .function = HRN_PQ_FINISH}

/***********************************************************************************************************************************
Macros to simplify dbOpen() for specific database versions
***********************************************************************************************************************************/
#define HRN_PQ_SCRIPT_OPEN_GE_93(sessionParam, connectParam, pgVersion, pgPathParam, standbyParam, archiveMode, archiveCommand)    \
    HRN_PQ_SCRIPT_OPEN(sessionParam, connectParam),                                                                                \
    HRN_PQ_SCRIPT_SET_SEARCH_PATH(sessionParam),                                                                                   \
    HRN_PQ_SCRIPT_SET_CLIENT_ENCODING(sessionParam),                                                                               \
    HRN_PQ_SCRIPT_VALIDATE_QUERY(sessionParam, pgVersion, pgPathParam, archiveMode, archiveCommand),                               \
    HRN_PQ_SCRIPT_SET_APPLICATION_NAME(sessionParam),                                                                              \
    HRN_PQ_SCRIPT_IS_STANDBY_QUERY(sessionParam, standbyParam)

#define HRN_PQ_SCRIPT_OPEN_GE_96(sessionParam, connectParam, pgVersion, pgPathParam, standbyParam, archiveMode, archiveCommand)    \
    HRN_PQ_SCRIPT_OPEN(sessionParam, connectParam),                                                                                \
    HRN_PQ_SCRIPT_SET_SEARCH_PATH(sessionParam),                                                                                   \
    HRN_PQ_SCRIPT_SET_CLIENT_ENCODING(sessionParam),                                                                               \
    HRN_PQ_SCRIPT_VALIDATE_QUERY(sessionParam, pgVersion, pgPathParam, archiveMode, archiveCommand),                               \
    HRN_PQ_SCRIPT_SET_APPLICATION_NAME(sessionParam),                                                                              \
    HRN_PQ_SCRIPT_SET_MAX_PARALLEL_WORKERS_PER_GATHER(sessionParam),                                                               \
    HRN_PQ_SCRIPT_IS_STANDBY_QUERY(sessionParam, standbyParam)

/***********************************************************************************************************************************
Structure for scripting pq responses
***********************************************************************************************************************************/
typedef struct HrnPqScript
{
    unsigned int session;                                           // Session number when multiple sessions are run concurrently
    const char *function;                                           // Function call expected
    const char *param;                                              // Params expected by the function for verification
    int resultInt;                                                  // Int result value
    const char *resultZ;                                            // Zero-terminated result value
    bool resultNull;                                                // Return null from function that normally returns a struct ptr
    TimeMSec sleep;                                                 // Sleep specified milliseconds before returning from function
} HrnPqScript;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Set a pq script. The prior script must have completed.
#define HRN_PQ_SCRIPT_SET(...)                                                                                                     \
    do                                                                                                                             \
    {                                                                                                                              \
        const HrnPqScript script[] = {__VA_ARGS__};                                                                                \
        hrnPqScriptSet(script, LENGTH_OF(script));                                                                                 \
    }                                                                                                                              \
    while (0)

void hrnPqScriptSet(const HrnPqScript *script, unsigned int scriptSize);

// Add to an existing pq script
#define HRN_PQ_SCRIPT_ADD(...)                                                                                                     \
    do                                                                                                                             \
    {                                                                                                                              \
        const HrnPqScript script[] = {__VA_ARGS__};                                                                                \
        hrnPqScriptAdd(script, LENGTH_OF(script));                                                                                 \
    }                                                                                                                              \
    while (0)

void hrnPqScriptAdd(const HrnPqScript *script, unsigned int scriptSize);

// Are we strict about requiring PQfinish()? Strict is a good idea for low-level testing of Pq code but is a nuisance for
// higher-level testing since it can mask other errors. When not strict, PGfinish() is allowed at any time and does not need to be
// scripted.
void hrnPqScriptStrictSet(bool strict);

#endif // HARNESS_PQ_REAL

#endif
