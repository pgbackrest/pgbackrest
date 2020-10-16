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
    unsigned int primaryIdx;
    Db *primary;
    unsigned int standbyIdx;
    Db *standby;
} DbGetResult;

DbGetResult dbGet(bool primaryOnly, bool primaryRequired, bool standbyRequired);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_DB_GET_RESULT_TYPE                                                                                            \
    DbGetResult
#define FUNCTION_LOG_DB_GET_RESULT_FORMAT(value, buffer, bufferSize)                                                               \
    objToLog(&value, "DbGetResult", buffer, bufferSize)

#endif
