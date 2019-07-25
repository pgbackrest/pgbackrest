/***********************************************************************************************************************************
Pq Test Harness

Scripted testing for PostgreSQL pqlib so exact results can be returned for unit testing.  See PostgreSQL client unit tests for
usage examples.
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_PQ_H
#define TEST_COMMON_HARNESS_PQ_H

#ifndef HARNESS_PQ_REAL

#include "common/time.h"

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
    unsigned int session;                                           // Session number when mutliple sessions are run concurrently
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

#endif // HARNESS_PQ_REAL

#endif
