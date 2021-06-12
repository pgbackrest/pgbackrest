/***********************************************************************************************************************************
Pq Test Harness
***********************************************************************************************************************************/
#ifndef HARNESS_PQ_REAL

#include "build.auto.h"

#include <string.h>

#include <libpq-fe.h>

#include "common/type/json.h"
#include "common/type/string.h"
#include "common/type/variantList.h"

#include "common/harnessPq.h"
#include "common/harnessTest.h"

/***********************************************************************************************************************************
Pq shim error prefix
***********************************************************************************************************************************/
#define PQ_ERROR_PREFIX                                             "PQ SHIM ERROR"

/***********************************************************************************************************************************
Script that defines how shim functions operate
***********************************************************************************************************************************/
HarnessPq harnessPqScript[1024];
bool harnessPqScriptDone = true;
unsigned int harnessPqScriptIdx;

// Is PQfinish scripting required?
bool harnessPqStrict = false;

// If there is a script failure change the behavior of cleanup functions to return immediately so the real error will be reported
// rather than a bogus scripting error during cleanup
bool harnessPqScriptFail;
char harnessPqScriptError[4096];

/***********************************************************************************************************************************
Set pq script
***********************************************************************************************************************************/
void
harnessPqScriptSet(HarnessPq *harnessPqScriptParam)
{
    if (!harnessPqScriptDone)
        THROW(AssertError, "previous pq script has not yet completed");

    if (harnessPqScriptParam[0].function == NULL)
        THROW(AssertError, "pq script must have entries");

    // Copy records into local storage
    unsigned int copyIdx = 0;

    while (harnessPqScriptParam[copyIdx].function != NULL)
    {
        harnessPqScript[copyIdx] = harnessPqScriptParam[copyIdx];
        copyIdx++;
    }

    harnessPqScript[copyIdx].function = NULL;
    harnessPqScriptDone = false;
    harnessPqScriptIdx = 0;
}

/**********************************************************************************************************************************/
void
harnessPqScriptStrictSet(bool strict)
{
    harnessPqStrict = strict;
}

/***********************************************************************************************************************************
Run pq script
***********************************************************************************************************************************/
static HarnessPq *
harnessPqScriptRun(const char *function, const VariantList *param, HarnessPq *parent)
{
    // If an error has already been thrown then throw the same error again
    if (harnessPqScriptFail)
        THROW(AssertError, harnessPqScriptError);

    // Convert params to json for comparison and reporting
    String *paramStr = param ? jsonFromVar(varNewVarLst(param)) : strNew();

    // Ensure script has not ended
    if (harnessPqScriptDone)
    {
        snprintf(harnessPqScriptError, sizeof(harnessPqScriptError), "pq script ended before %s (%s)", function, strZ(paramStr));

        TEST_LOG_FMT(PQ_ERROR_PREFIX ": %s", harnessPqScriptError);
        harnessPqScriptFail = true;

        THROW(AssertError, harnessPqScriptError);
    }

    // Get current script item
    HarnessPq *result = &harnessPqScript[harnessPqScriptIdx];

    // Check that expected function was called
    if (strcmp(result->function, function) != 0)
    {
        snprintf(
            harnessPqScriptError, sizeof(harnessPqScriptError), "pq script [%u] expected function %s (%s) but got %s (%s)",
            harnessPqScriptIdx, result->function, result->param == NULL ? "" : result->param, function, strZ(paramStr));

        TEST_LOG_FMT(PQ_ERROR_PREFIX ": %s", harnessPqScriptError);
        harnessPqScriptFail = true;

        THROW(AssertError, harnessPqScriptError);
    }

    // Check that parameters match
    if ((param != NULL && result->param == NULL) || (param == NULL && result->param != NULL) ||
        (param != NULL && result->param != NULL && !strEqZ(paramStr, result->param)))
    {
        snprintf(
            harnessPqScriptError, sizeof(harnessPqScriptError), "pq script [%u] function '%s', expects param '%s' but got '%s'",
            harnessPqScriptIdx, result->function, result->param ? result->param : "NULL", param ? strZ(paramStr) : "NULL");

        TEST_LOG_FMT(PQ_ERROR_PREFIX ": %s", harnessPqScriptError);
        harnessPqScriptFail = true;

        THROW(AssertError, harnessPqScriptError);
    }

    // Make sure the session matches with the parent as a sanity check
    if (parent != NULL && result->session != parent->session)
    {
        snprintf(
            harnessPqScriptError, sizeof(harnessPqScriptError), "pq script [%u] function '%s', expects session '%u' but got '%u'",
            harnessPqScriptIdx, result->function, result->session, parent->session);

        TEST_LOG_FMT(PQ_ERROR_PREFIX ": %s", harnessPqScriptError);
        harnessPqScriptFail = true;

        THROW(AssertError, harnessPqScriptError);
    }

    // Sleep if requested
    if (result->sleep > 0)
        sleepMSec(result->sleep);

    harnessPqScriptIdx++;

    if (harnessPqScript[harnessPqScriptIdx].function == NULL)
        harnessPqScriptDone = true;

    return result;
}

/***********************************************************************************************************************************
Shim for PQconnectdb()
***********************************************************************************************************************************/
PGconn *PQconnectdb(const char *conninfo)
{
    return (PGconn *)harnessPqScriptRun(HRNPQ_CONNECTDB, varLstAdd(varLstNew(), varNewStrZ(conninfo)), NULL);
}

/***********************************************************************************************************************************
Shim for PQstatus()
***********************************************************************************************************************************/
ConnStatusType PQstatus(const PGconn *conn)
{
    return (ConnStatusType)harnessPqScriptRun(HRNPQ_STATUS, NULL, (HarnessPq *)conn)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQerrorMessage()
***********************************************************************************************************************************/
char *PQerrorMessage(const PGconn *conn)
{
    return (char *)harnessPqScriptRun(HRNPQ_ERRORMESSAGE, NULL, (HarnessPq *)conn)->resultZ;
}

/***********************************************************************************************************************************
Shim for PQsetNoticeProcessor()
***********************************************************************************************************************************/
PQnoticeProcessor
PQsetNoticeProcessor(PGconn *conn, PQnoticeProcessor proc, void *arg)
{
    (void)conn;

    // Call the processor that was passed so we have coverage
    proc(arg, "test notice");
    return NULL;
}

/***********************************************************************************************************************************
Shim for PQsendQuery()
***********************************************************************************************************************************/
int
PQsendQuery(PGconn *conn, const char *query)
{
    return harnessPqScriptRun(HRNPQ_SENDQUERY, varLstAdd(varLstNew(), varNewStrZ(query)), (HarnessPq *)conn)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQconsumeInput()
***********************************************************************************************************************************/
int
PQconsumeInput(PGconn *conn)
{
    return harnessPqScriptRun(HRNPQ_CONSUMEINPUT, NULL, (HarnessPq *)conn)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQisBusy()
***********************************************************************************************************************************/
int
PQisBusy(PGconn *conn)
{
    return harnessPqScriptRun(HRNPQ_ISBUSY, NULL, (HarnessPq *)conn)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQgetCancel()
***********************************************************************************************************************************/
PGcancel *
PQgetCancel(PGconn *conn)
{
    HarnessPq *harnessPq = harnessPqScriptRun(HRNPQ_GETCANCEL, NULL, (HarnessPq *)conn);
    return harnessPq->resultNull ? NULL : (PGcancel *)harnessPq;
}

/***********************************************************************************************************************************
Shim for PQcancel()
***********************************************************************************************************************************/
int
PQcancel(PGcancel *cancel, char *errbuf, int errbufsize)
{
    HarnessPq *harnessPq = harnessPqScriptRun(HRNPQ_CANCEL, NULL, (HarnessPq *)cancel);

    if (!harnessPq->resultInt)
    {
        strncpy(errbuf, harnessPq->resultZ, (size_t)errbufsize);
        errbuf[errbufsize - 1] = '\0';
    }

    return harnessPq->resultInt;
}

/***********************************************************************************************************************************
Shim for PQfreeCancel()
***********************************************************************************************************************************/
void
PQfreeCancel(PGcancel *cancel)
{
    harnessPqScriptRun(HRNPQ_FREECANCEL, NULL, (HarnessPq *)cancel);
}

/***********************************************************************************************************************************
Shim for PQgetResult()
***********************************************************************************************************************************/
PGresult *
PQgetResult(PGconn *conn)
{
    if (!harnessPqScriptFail)
    {
        HarnessPq *harnessPq = harnessPqScriptRun(HRNPQ_GETRESULT, NULL, (HarnessPq *)conn);
        return harnessPq->resultNull ? NULL : (PGresult *)harnessPq;
    }

    return NULL;
}

/***********************************************************************************************************************************
Shim for PQresultStatus()
***********************************************************************************************************************************/
ExecStatusType
PQresultStatus(const PGresult *res)
{
    return (ExecStatusType)harnessPqScriptRun(HRNPQ_RESULTSTATUS, NULL, (HarnessPq *)res)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQresultErrorMessage()
***********************************************************************************************************************************/
char *
PQresultErrorMessage(const PGresult *res)
{
    return (char *)harnessPqScriptRun(HRNPQ_RESULTERRORMESSAGE, NULL, (HarnessPq *)res)->resultZ;
}

/***********************************************************************************************************************************
Shim for PQntuples()
***********************************************************************************************************************************/
int
PQntuples(const PGresult *res)
{
    return harnessPqScriptRun(HRNPQ_NTUPLES, NULL, (HarnessPq *)res)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQnfields()
***********************************************************************************************************************************/
int
PQnfields(const PGresult *res)
{
    return harnessPqScriptRun(HRNPQ_NFIELDS, NULL, (HarnessPq *)res)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQgetisnull()
***********************************************************************************************************************************/
int
PQgetisnull(const PGresult *res, int tup_num, int field_num)
{
    return harnessPqScriptRun(
        HRNPQ_GETISNULL, varLstAdd(varLstAdd(varLstNew(), varNewInt(tup_num)), varNewInt(field_num)), (HarnessPq *)res)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQftype()
***********************************************************************************************************************************/
Oid
PQftype(const PGresult *res, int field_num)
{
    return (Oid)harnessPqScriptRun(HRNPQ_FTYPE, varLstAdd(varLstNew(), varNewInt(field_num)), (HarnessPq *)res)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQgetvalue()
***********************************************************************************************************************************/
char *
PQgetvalue(const PGresult *res, int tup_num, int field_num)
{
    return (char *)harnessPqScriptRun(
        HRNPQ_GETVALUE, varLstAdd(varLstAdd(varLstNew(), varNewInt(tup_num)), varNewInt(field_num)), (HarnessPq *)res)->resultZ;
}

/***********************************************************************************************************************************
Shim for PQclear()
***********************************************************************************************************************************/
void
PQclear(PGresult *res)
{
    if (!harnessPqScriptFail)
        harnessPqScriptRun(HRNPQ_CLEAR, NULL, (HarnessPq *)res);
}

/***********************************************************************************************************************************
Shim for PQfinish()
***********************************************************************************************************************************/
void PQfinish(PGconn *conn)
{
    // If currently processing an error then assume this is the client destructor running which won't be in the script
    if (errorType() != NULL)
        return;

    if (harnessPqStrict && !harnessPqScriptFail)
        harnessPqScriptRun(HRNPQ_FINISH, NULL, (HarnessPq *)conn);
}

#endif // HARNESS_PQ_REAL
