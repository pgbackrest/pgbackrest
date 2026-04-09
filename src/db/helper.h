/***********************************************************************************************************************************
Database Helper

Helper functions for getting connections to PostgreSQL.
***********************************************************************************************************************************/
#ifndef DB_HELPER_H
#define DB_HELPER_H

#include <stdbool.h>

#include "db/db.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Get specified cluster(s)
typedef struct DbGetResult
{
    unsigned int primaryIdx;                                        // cfgOptGrpPg index of the primary
    Db *primary;                                                    // Primary db object (NULL if none requested)
    unsigned int standbyIdx;                                        // cfgOptGrpPg index of the standby
    Db *standby;                                                    // Standby db object (NULL if none requested)
} DbGetResult;

FN_EXTERN DbGetResult dbGet(bool primaryOnly, bool primaryRequired, StringId standbyRequired);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_DB_GET_RESULT_TYPE                                                                                            \
    DbGetResult
#define FUNCTION_LOG_DB_GET_RESULT_FORMAT(value, buffer, bufferSize)                                                               \
    objNameToLog(&value, "DbGetResult", buffer, bufferSize)

#endif
