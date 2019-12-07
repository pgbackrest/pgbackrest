/***********************************************************************************************************************************
Pq Test Harness

Scripted testing for PostgreSQL pqlib so exact results can be returned for unit testing.  See PostgreSQL client unit tests for
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
            " (select setting from pg_catalog.pg_settings where name = 'archive_command')::text\"]",                               \
        .resultInt = 1},                                                                                                           \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                       \
    {.session = sessionParam, .function = HRNPQ_NTUPLES, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_NFIELDS, .resultInt = 4},                                                          \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_INT},                               \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[2]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[3]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = STRINGIFY(versionParam)},                   \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = pgPathParam},                               \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,2]", .resultZ = archiveMode == NULL ? "on"                  \
        : archiveMode},                                                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,3]", .resultZ = archiveCommand == NULL ? PROJECT_BIN        \
        : archiveCommand},                                                                                                         \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_SET_APPLICATION_NAME(sessionParam)                                                                             \
    {.session = sessionParam, .function = HRNPQ_SENDQUERY,                                                                         \
        .param = strPtr(strNewFmt("[\"set application_name = '" PROJECT_NAME " [%s]'\"]", cfgCommandName(cfgCommand()))),          \
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

#define HRNPQ_MACRO_CLOSE(sessionParam)                                                                                            \
    {.session = sessionParam, .function = HRNPQ_FINISH}

#define HRNPQ_MACRO_DONE()                                                                                                         \
    {.function = NULL}

/***********************************************************************************************************************************
Macros to simplify dbOpen() for specific database versions
***********************************************************************************************************************************/
#define HRNPQ_MACRO_OPEN_84(sessionParam, connectParam, pgPathParam, archiveMode, archiveCommand)                                  \
    HRNPQ_MACRO_OPEN(sessionParam, connectParam),                                                                                  \
    HRNPQ_MACRO_SET_SEARCH_PATH(sessionParam),                                                                                     \
    HRNPQ_MACRO_VALIDATE_QUERY(sessionParam, PG_VERSION_84, pgPathParam, archiveMode, archiveCommand)

#define HRNPQ_MACRO_OPEN_92(sessionParam, connectParam, pgPathParam, standbyParam, archiveMode, archiveCommand)                    \
    HRNPQ_MACRO_OPEN(sessionParam, connectParam),                                                                                  \
    HRNPQ_MACRO_SET_SEARCH_PATH(sessionParam),                                                                                     \
    HRNPQ_MACRO_VALIDATE_QUERY(sessionParam, PG_VERSION_92, pgPathParam, archiveMode, archiveCommand),                             \
    HRNPQ_MACRO_SET_APPLICATION_NAME(sessionParam),                                                                                \
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
