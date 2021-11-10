/***********************************************************************************************************************************
Pq Test Harness

Scripted testing for PostgreSQL libpq so exact results can be returned for unit testing.  See PostgreSQL client unit tests for
usage examples.
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
#define HRNPQ_CANCEL                                                "PQcancel"
#define HRNPQ_CLEAR                                                 "PQclear"
#define HRNPQ_CONNECTDB                                             "PQconnectdb"
#define HRNPQ_CONSUMEINPUT                                          "PQconsumeInput"
#define HRNPQ_ERRORMESSAGE                                          "PQerrorMessage"
#define HRNPQ_FINISH                                                "PQfinish"
#define HRNPQ_FREECANCEL                                            "PQfreeCancel"
#define HRNPQ_FTYPE                                                 "PQftype"
#define HRNPQ_GETCANCEL                                             "PQgetCancel"
#define HRNPQ_GETISNULL                                             "PQgetisnull"
#define HRNPQ_GETRESULT                                             "PQgetResult"
#define HRNPQ_GETVALUE                                              "PQgetvalue"
#define HRNPQ_ISBUSY                                                "PQisbusy"
#define HRNPQ_NFIELDS                                               "PQnfields"
#define HRNPQ_NTUPLES                                               "PQntuples"
#define HRNPQ_RESULTERRORMESSAGE                                    "PQresultErrorMessage"
#define HRNPQ_RESULTSTATUS                                          "PQresultStatus"
#define HRNPQ_SENDQUERY                                             "PQsendQuery"
#define HRNPQ_STATUS                                                "PQstatus"

/***********************************************************************************************************************************
Macros for defining groups of functions that implement various queries and commands
***********************************************************************************************************************************/
#define HRNPQ_MACRO_OPEN(sessionParam, connectParam)                                                                               \
    {.session = sessionParam, .function = HRNPQ_CONNECTDB, .param = "[\"" connectParam "\"]"},                                     \
    {.session = sessionParam, .function = HRNPQ_STATUS, .resultInt = CONNECTION_OK}

#define HRNPQ_MACRO_SET_CLIENT_ENCODING(sessionParam)                                                                              \
    {.session = sessionParam, .function = HRNPQ_SENDQUERY, .param = "[\"set client_encoding = 'UTF8'\"]", .resultInt = 1},         \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},                                      \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_SET_SEARCH_PATH(sessionParam)                                                                                  \
    {.session = sessionParam, .function = HRNPQ_SENDQUERY, .param = "[\"set search_path = 'pg_catalog'\"]", .resultInt = 1},       \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},                                      \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_VALIDATE_QUERY(sessionParam, versionParam, pgPathParam, archiveMode, archiveCommand)                           \
    {.session = sessionParam, .function = HRNPQ_SENDQUERY, .param =                                                                \
        "[\"select (select setting from pg_catalog.pg_settings where name = 'server_version_num')::int4,"                          \
            " (select setting from pg_catalog.pg_settings where name = 'data_directory')::text,"                                   \
            " (select setting from pg_catalog.pg_settings where name = 'archive_mode')::text,"                                     \
            " (select setting from pg_catalog.pg_settings where name = 'archive_command')::text,"                                  \
            " (select setting from pg_catalog.pg_settings where name = 'checkpoint_timeout')::int4\"]",                            \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                       \
    {.session = sessionParam, .function = HRNPQ_NTUPLES, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_NFIELDS, .resultInt = 5},                                                          \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_INT},                               \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[2]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[3]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[4]", .resultInt = HRNPQ_TYPE_INT},                               \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = STRINGIFY(versionParam)},                   \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = pgPathParam},                               \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,2]", .resultZ = archiveMode == NULL ? "on"                  \
        : archiveMode},                                                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,3]", .resultZ = archiveCommand == NULL ? PROJECT_BIN        \
        : archiveCommand},                                                                                                         \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,4]", .resultZ = STRINGIFY(versionParam)},                   \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_SET_APPLICATION_NAME(sessionParam)                                                                             \
    {.session = sessionParam, .function = HRNPQ_SENDQUERY,                                                                         \
        .param = strZ(strNewFmt("[\"set application_name = '" PROJECT_NAME " [%s]'\"]", cfgCommandName())),                        \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},                                      \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_SET_MAX_PARALLEL_WORKERS_PER_GATHER(sessionParam)                                                              \
    {.session = sessionParam, .function = HRNPQ_SENDQUERY, .param = "[\"set max_parallel_workers_per_gather = 0\"]",               \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},                                      \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_IS_STANDBY_QUERY(sessionParam, standbyParam)                                                                   \
    {.session = sessionParam, .function = HRNPQ_SENDQUERY, .param = "[\"select pg_catalog.pg_is_in_recovery()\"]", .resultInt = 1},\
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                       \
    {.session = sessionParam, .function = HRNPQ_NTUPLES, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_NFIELDS, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_BOOL},                              \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = STRINGIFY(standbyParam)},                   \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_CREATE_RESTORE_POINT(sessionParam, lsnParam)                                                                   \
    {.session = sessionParam,                                                                                                      \
        .function = HRNPQ_SENDQUERY, .param = "[\"select pg_catalog.pg_create_restore_point('pgBackRest Archive Check')::text\"]", \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                       \
    {.session = sessionParam, .function = HRNPQ_NTUPLES, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_NFIELDS, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = lsnParam},                                  \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_WAL_SWITCH(sessionParam, walNameParam, walFileNameParam)                                                       \
    {.session = sessionParam, .function = HRNPQ_SENDQUERY,                                                                         \
        .param = "[\"select pg_catalog.pg_" walNameParam "file_name(pg_catalog.pg_switch_" walNameParam "())::text\"]",            \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                       \
    {.session = sessionParam, .function = HRNPQ_NTUPLES, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_NFIELDS, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = walFileNameParam},                          \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_TIME_QUERY(sessionParam, timeParam)                                                                            \
    {.session = sessionParam,                                                                                                      \
        .function = HRNPQ_SENDQUERY, .param = "[\"select (extract(epoch from clock_timestamp()) * 1000)::bigint\"]",               \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                       \
    {.session = sessionParam, .function = HRNPQ_NTUPLES, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_NFIELDS, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_INT},                               \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,0]",                                                        \
        .resultZ = strZ(strNewFmt("%" PRId64, (int64_t)(timeParam)))},                                                             \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_ADVISORY_LOCK(sessionParam, lockAcquiredParam)                                                                 \
    {.session = sessionParam,                                                                                                      \
        .function = HRNPQ_SENDQUERY, .param = "[\"select pg_catalog.pg_try_advisory_lock(12340078987004321)::bool\"]",             \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                       \
    {.session = sessionParam, .function = HRNPQ_NTUPLES, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_NFIELDS, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_BOOL},                              \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = cvtBoolToConstZ(lockAcquiredParam)},        \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_IS_IN_BACKUP(sessionParam, inBackupParam)                                                                      \
    {.session = sessionParam,                                                                                                      \
        .function = HRNPQ_SENDQUERY, .param = "[\"select pg_catalog.pg_is_in_backup()::bool\"]",                                   \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                       \
    {.session = sessionParam, .function = HRNPQ_NTUPLES, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_NFIELDS, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_BOOL},                              \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = cvtBoolToConstZ(inBackupParam)},            \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_START_BACKUP_83(sessionParam, lsnParam, walSegmentNameParam)                                                   \
    {.session = sessionParam,                                                                                                      \
        .function = HRNPQ_SENDQUERY,                                                                                               \
        .param =                                                                                                                   \
            "[\"select lsn::text as lsn,\\n"                                                                                       \
            "       pg_catalog.pg_xlogfile_name(lsn)::text as wal_segment_name\\n"                                                 \
            "  from pg_catalog.pg_start_backup('pgBackRest backup started at ' || current_timestamp) as lsn\"]",                   \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                       \
    {.session = sessionParam, .function = HRNPQ_NTUPLES, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_NFIELDS, .resultInt = 2},                                                          \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = lsnParam},                                  \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = walSegmentNameParam},                       \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_START_BACKUP_84_95(sessionParam, startFastParam, lsnParam, walSegmentNameParam)                                \
    {.session = sessionParam,                                                                                                      \
        .function = HRNPQ_SENDQUERY,                                                                                               \
        .param = strZ(strNewFmt(                                                                                                   \
            "[\"select lsn::text as lsn,\\n"                                                                                       \
            "       pg_catalog.pg_xlogfile_name(lsn)::text as wal_segment_name\\n"                                                 \
            "  from pg_catalog.pg_start_backup('pgBackRest backup started at ' || current_timestamp, %s) as lsn\"]",               \
            cvtBoolToConstZ(startFastParam))),                                                                                     \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                       \
    {.session = sessionParam, .function = HRNPQ_NTUPLES, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_NFIELDS, .resultInt = 2},                                                          \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = lsnParam},                                  \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = walSegmentNameParam},                       \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_START_BACKUP_96(sessionParam, startFastParam, lsnParam, walSegmentNameParam)                                   \
    {.session = sessionParam,                                                                                                      \
        .function = HRNPQ_SENDQUERY,                                                                                               \
        .param = strZ(strNewFmt(                                                                                                   \
            "[\"select lsn::text as lsn,\\n"                                                                                       \
            "       pg_catalog.pg_xlogfile_name(lsn)::text as wal_segment_name\\n"                                                 \
            "  from pg_catalog.pg_start_backup('pgBackRest backup started at ' || current_timestamp, %s, false) as lsn\"]",        \
            cvtBoolToConstZ(startFastParam))),                                                                                     \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                       \
    {.session = sessionParam, .function = HRNPQ_NTUPLES, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_NFIELDS, .resultInt = 2},                                                          \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = lsnParam},                                  \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = walSegmentNameParam},                       \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_START_BACKUP_GE_10(sessionParam, startFastParam, lsnParam, walSegmentNameParam)                                \
    {.session = sessionParam,                                                                                                      \
        .function = HRNPQ_SENDQUERY,                                                                                               \
        .param = strZ(strNewFmt(                                                                                                   \
            "[\"select lsn::text as lsn,\\n"                                                                                       \
            "       pg_catalog.pg_walfile_name(lsn)::text as wal_segment_name\\n"                                                  \
            "  from pg_catalog.pg_start_backup('pgBackRest backup started at ' || current_timestamp, %s, false) as lsn\"]",        \
            cvtBoolToConstZ(startFastParam))),                                                                                     \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                       \
    {.session = sessionParam, .function = HRNPQ_NTUPLES, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_NFIELDS, .resultInt = 2},                                                          \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = lsnParam},                                  \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = walSegmentNameParam},                       \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_STOP_BACKUP_LE_95(sessionParam, lsnParam, walSegmentNameParam)                                                 \
    {.session = sessionParam,                                                                                                      \
        .function = HRNPQ_SENDQUERY,                                                                                               \
        .param =                                                                                                                   \
            "[\"select lsn::text as lsn,\\n"                                                                                       \
            "       pg_catalog.pg_xlogfile_name(lsn)::text as wal_segment_name\\n"                                                 \
            "  from pg_catalog.pg_stop_backup() as lsn\"]",                                                                               \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                       \
    {.session = sessionParam, .function = HRNPQ_NTUPLES, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_NFIELDS, .resultInt = 2},                                                          \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = lsnParam},                                  \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = walSegmentNameParam},                       \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_STOP_BACKUP_96(sessionParam, lsnParam, walSegmentNameParam, tablespaceMapParam)                                \
    {.session = sessionParam,                                                                                                      \
        .function = HRNPQ_SENDQUERY,                                                                                               \
        .param =                                                                                                                   \
            "[\"select lsn::text as lsn,\\n"                                                                                       \
            "       pg_catalog.pg_xlogfile_name(lsn)::text as wal_segment_name,\\n"                                                \
            "       labelfile::text as backuplabel_file,\\n"                                                                       \
            "       spcmapfile::text as tablespacemap_file\\n"                                                                     \
            "  from pg_catalog.pg_stop_backup(false)\"]",                                                                          \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                       \
    {.session = sessionParam, .function = HRNPQ_NTUPLES, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_NFIELDS, .resultInt = 4},                                                          \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[2]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[3]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = lsnParam},                                  \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = walSegmentNameParam},                       \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,2]", .resultZ = "BACKUP_LABEL_DATA"},                       \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,3]",                                                        \
        .resultZ = tablespaceMapParam ? "TABLESPACE_MAP_DATA" : "\n"},                                                             \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_STOP_BACKUP_GE_10(sessionParam, lsnParam, walSegmentNameParam, tablespaceMapParam)                             \
    {.session = sessionParam,                                                                                                      \
        .function = HRNPQ_SENDQUERY,                                                                                               \
        .param =                                                                                                                   \
            "[\"select lsn::text as lsn,\\n"                                                                                       \
            "       pg_catalog.pg_walfile_name(lsn)::text as wal_segment_name,\\n"                                                 \
            "       labelfile::text as backuplabel_file,\\n"                                                                       \
            "       spcmapfile::text as tablespacemap_file\\n"                                                                     \
            "  from pg_catalog.pg_stop_backup(false, false)\"]",                                                                   \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                       \
    {.session = sessionParam, .function = HRNPQ_NTUPLES, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_NFIELDS, .resultInt = 4},                                                          \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[2]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[3]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = lsnParam},                                  \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = walSegmentNameParam},                       \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,2]", .resultZ = "BACKUP_LABEL_DATA"},                       \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,3]",                                                        \
        .resultZ = tablespaceMapParam ? "TABLESPACE_MAP_DATA" : "\n"},                                                             \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_DATABASE_LIST_1(sessionParam, databaseNameParam)                                                               \
    {.session = sessionParam,                                                                                                      \
        .function = HRNPQ_SENDQUERY,                                                                                               \
        .param = "[\"select oid::oid, datname::text, datlastsysoid::oid from pg_catalog.pg_database\"]",                           \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                       \
    {.session = sessionParam, .function = HRNPQ_NTUPLES, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_NFIELDS, .resultInt = 3},                                                          \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_INT},                               \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[2]", .resultInt = HRNPQ_TYPE_INT},                               \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = STRINGIFY(16384)},                          \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = databaseNameParam},                         \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,2]", .resultZ = STRINGIFY(13777)},                          \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_TABLESPACE_LIST_0(sessionParam)                                                                                \
    {.session = sessionParam,                                                                                                      \
        .function = HRNPQ_SENDQUERY, .param = "[\"select oid::oid, spcname::text from pg_catalog.pg_tablespace\"]",                \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                       \
    {.session = sessionParam, .function = HRNPQ_NTUPLES, .resultInt = 0},                                                          \
    {.session = sessionParam, .function = HRNPQ_NFIELDS, .resultInt = 2},                                                          \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_INT},                               \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_TABLESPACE_LIST_1(sessionParam, id1Param, name1Param)                                                          \
    {.session = sessionParam,                                                                                                      \
        .function = HRNPQ_SENDQUERY, .param = "[\"select oid::oid, spcname::text from pg_catalog.pg_tablespace\"]",                \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                       \
    {.session = sessionParam, .function = HRNPQ_NTUPLES, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_NFIELDS, .resultInt = 2},                                                          \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_INT},                               \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = strZ(strNewFmt("%d", id1Param))},           \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = name1Param},                                \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_CHECKPOINT(sessionParam)                                                                                       \
    {.session = sessionParam, .function = HRNPQ_SENDQUERY, .param = "[\"checkpoint\"]", .resultInt = 1},                           \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_COMMAND_OK},                                      \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_REPLAY_TARGET_REACHED(                                                                                         \
    sessionParam, walNameParam, lsnNameParam, targetLsnParam, targetReachedParam, replayLsnParam)                                  \
    {.session = sessionParam,                                                                                                      \
        .function = HRNPQ_SENDQUERY,                                                                                               \
        .param =  strZ(strNewFmt(                                                                                                  \
            "[\"select replayLsn::text,\\n"                                                                                        \
            "       (replayLsn > '%s')::bool as targetReached\\n"                                                                  \
            "  from pg_catalog.pg_last_" walNameParam "_replay_" lsnNameParam "() as replayLsn\"]", targetLsnParam)),              \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                       \
    {.session = sessionParam, .function = HRNPQ_NTUPLES, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_NFIELDS, .resultInt = 2},                                                          \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_BOOL},                              \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = replayLsnParam},                            \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = cvtBoolToConstZ(targetReachedParam)},       \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_REPLAY_TARGET_REACHED_LE_96(sessionParam, targetLsnParam, targetReachedParam, reachedLsnParam)                 \
    HRNPQ_MACRO_REPLAY_TARGET_REACHED(sessionParam, "xlog", "location", targetLsnParam, targetReachedParam, reachedLsnParam)

#define HRNPQ_MACRO_REPLAY_TARGET_REACHED_GE_10(sessionParam, targetLsnParam, targetReachedParam, reachedLsnParam)                 \
    HRNPQ_MACRO_REPLAY_TARGET_REACHED(sessionParam, "wal", "lsn", targetLsnParam, targetReachedParam, reachedLsnParam)

#define HRNPQ_MACRO_CHECKPOINT_TARGET_REACHED(                                                                                     \
    sessionParam, lsnNameParam, targetLsnParam, targetReachedParam, checkpointLsnParam, sleepParam)                                \
    {.session = sessionParam,                                                                                                      \
        .function = HRNPQ_SENDQUERY,                                                                                               \
        .param = strZ(strNewFmt(                                                                                                   \
            "[\"select (checkpoint_" lsnNameParam " > '%s')::bool as targetReached,\\n"                                            \
            "       checkpoint_" lsnNameParam "::text as checkpointLsn\\n"                                                         \
            "  from pg_catalog.pg_control_checkpoint()\"]", targetLsnParam)),                                                      \
        .resultInt = 1, .sleep = sleepParam},                                                                                      \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                       \
    {.session = sessionParam, .function = HRNPQ_NTUPLES, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_NFIELDS, .resultInt = 2},                                                          \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_BOOL},                              \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = cvtBoolToConstZ(targetReachedParam)},       \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = checkpointLsnParam},                        \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_CHECKPOINT_TARGET_REACHED_96(sessionParam, targetLsnParam, targetReachedParam, checkpointLsnParam, sleepParam) \
    HRNPQ_MACRO_CHECKPOINT_TARGET_REACHED(                                                                                         \
        sessionParam, "location", targetLsnParam, targetReachedParam, checkpointLsnParam, sleepParam)

#define HRNPQ_MACRO_CHECKPOINT_TARGET_REACHED_GE_10(                                                                               \
    sessionParam, targetLsnParam, targetReachedParam, checkpointLsnParam, sleepParam)                                              \
    HRNPQ_MACRO_CHECKPOINT_TARGET_REACHED(sessionParam, "lsn", targetLsnParam, targetReachedParam, checkpointLsnParam, sleepParam)

#define HRNPQ_MACRO_REPLAY_WAIT_LE_95(sessionParam, targetLsnParam)                                                                \
    HRNPQ_MACRO_REPLAY_TARGET_REACHED_LE_96(sessionParam, targetLsnParam, true, "X/X"),                                            \
    HRNPQ_MACRO_CHECKPOINT(sessionParam)

#define HRNPQ_MACRO_REPLAY_WAIT_96(sessionParam, targetLsnParam)                                                                   \
    HRNPQ_MACRO_REPLAY_TARGET_REACHED_LE_96(sessionParam, targetLsnParam, true, "X/X"),                                            \
    HRNPQ_MACRO_CHECKPOINT(sessionParam),                                                                                          \
    HRNPQ_MACRO_CHECKPOINT_TARGET_REACHED_96(sessionParam, targetLsnParam, true, "X/X", 0)

#define HRNPQ_MACRO_REPLAY_WAIT_GE_10(sessionParam, targetLsnParam)                                                                \
    HRNPQ_MACRO_REPLAY_TARGET_REACHED_GE_10(sessionParam, targetLsnParam, true, "X/X"),                                            \
    HRNPQ_MACRO_CHECKPOINT(sessionParam),                                                                                          \
    HRNPQ_MACRO_CHECKPOINT_TARGET_REACHED_GE_10(sessionParam, targetLsnParam, true, "X/X", 0)

#define HRNPQ_MACRO_CLOSE(sessionParam)                                                                                            \
    {.session = sessionParam, .function = HRNPQ_FINISH}

#define HRNPQ_MACRO_DONE()                                                                                                         \
    {.function = NULL}

/***********************************************************************************************************************************
Macros to simplify dbOpen() for specific database versions
***********************************************************************************************************************************/
#define HRNPQ_MACRO_OPEN_LE_91(sessionParam, connectParam, pgVersion, pgPathParam, archiveMode, archiveCommand)                    \
    HRNPQ_MACRO_OPEN(sessionParam, connectParam),                                                                                  \
    HRNPQ_MACRO_SET_SEARCH_PATH(sessionParam),                                                                                     \
    HRNPQ_MACRO_SET_CLIENT_ENCODING(sessionParam),                                                                                 \
    HRNPQ_MACRO_VALIDATE_QUERY(sessionParam, pgVersion, pgPathParam, archiveMode, archiveCommand)

#define HRNPQ_MACRO_OPEN_GE_92(sessionParam, connectParam, pgVersion, pgPathParam, standbyParam, archiveMode, archiveCommand)      \
    HRNPQ_MACRO_OPEN(sessionParam, connectParam),                                                                                  \
    HRNPQ_MACRO_SET_SEARCH_PATH(sessionParam),                                                                                     \
    HRNPQ_MACRO_SET_CLIENT_ENCODING(sessionParam),                                                                                 \
    HRNPQ_MACRO_VALIDATE_QUERY(sessionParam, pgVersion, pgPathParam, archiveMode, archiveCommand),                                 \
    HRNPQ_MACRO_SET_APPLICATION_NAME(sessionParam),                                                                                \
    HRNPQ_MACRO_IS_STANDBY_QUERY(sessionParam, standbyParam)

#define HRNPQ_MACRO_OPEN_GE_96(sessionParam, connectParam, pgVersion, pgPathParam, standbyParam, archiveMode, archiveCommand)      \
    HRNPQ_MACRO_OPEN(sessionParam, connectParam),                                                                                  \
    HRNPQ_MACRO_SET_SEARCH_PATH(sessionParam),                                                                                     \
    HRNPQ_MACRO_SET_CLIENT_ENCODING(sessionParam),                                                                                 \
    HRNPQ_MACRO_VALIDATE_QUERY(sessionParam, pgVersion, pgPathParam, archiveMode, archiveCommand),                                 \
    HRNPQ_MACRO_SET_APPLICATION_NAME(sessionParam),                                                                                \
    HRNPQ_MACRO_SET_MAX_PARALLEL_WORKERS_PER_GATHER(sessionParam),                                                                 \
    HRNPQ_MACRO_IS_STANDBY_QUERY(sessionParam, standbyParam)

/***********************************************************************************************************************************
Data type constants
***********************************************************************************************************************************/
#define HRNPQ_TYPE_BOOL                                             16
#define HRNPQ_TYPE_INT                                              20
#define HRNPQ_TYPE_TEXT                                             25

/***********************************************************************************************************************************
Structure for scripting pq responses
***********************************************************************************************************************************/
typedef struct HarnessPq
{
    unsigned int session;                                           // Session number when multiple sessions are run concurrently
    const char *function;                                           // Function call expected
    const char *param;                                              // Params expected by the function for verification
    int resultInt;                                                  // Int result value
    const char *resultZ;                                            // Zero-terminated result value
    bool resultNull;                                                // Return null from function that normally returns a struct ptr
    TimeMSec sleep;                                                 // Sleep specified milliseconds before returning from function
} HarnessPq;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void harnessPqScriptSet(HarnessPq *harnessPqScriptParam);

// Are we strict about requiring PQfinish()?  Strict is a good idea for low-level testing of Pq code but is a nuissance for
// higher-level testing since it can mask other errors.  When not strict, PGfinish() is allowed at any time and does not need to be
// scripted.
void harnessPqScriptStrictSet(bool strict);

#endif // HARNESS_PQ_REAL

#endif
