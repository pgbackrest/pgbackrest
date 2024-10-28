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

#include "common/harnessFd.h"
#include "common/harnessPq.h"
#include "common/harnessTest.h"

/***********************************************************************************************************************************
Pq shim error prefix
***********************************************************************************************************************************/
#define PQ_ERROR_PREFIX                                             "PQ SHIM ERROR"

/***********************************************************************************************************************************
Local variables
***********************************************************************************************************************************/
static struct HrnPqLocal
{
    MemContext *memContext;                                         // Script mem context

    // Script that defines how shim functions operate
    HrnPqScript script[1024];
    unsigned int scriptSize;
    unsigned int scriptIdx;

    // Is PQfinish scripting required?
    bool strict;

    // If there is a script failure change the behavior of cleanup functions to return immediately so the real error will be
    // reported rather than a bogus scripting error during cleanup
    bool scriptFail;
    char scriptError[4096];
} hrnPqLocal;

/**********************************************************************************************************************************/
void
hrnPqScriptSet(const HrnPqScript *const script, const unsigned int scriptSize)
{
    if (hrnPqLocal.scriptSize != 0)
        THROW(AssertError, "previous pq script has not yet completed");

    hrnPqScriptAdd(script, scriptSize);
}

/**********************************************************************************************************************************/
void
hrnPqScriptAdd(const HrnPqScript *const script, const unsigned int scriptSize)
{
    if (scriptSize == 0)
        THROW(AssertError, "pq script must have entries");

    if (hrnPqLocal.scriptSize == 0)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            MEM_CONTEXT_NEW_BEGIN(HrnPqLocal, .childQty = MEM_CONTEXT_QTY_MAX)
            {
                hrnPqLocal.memContext = MEM_CONTEXT_NEW();
            }
            MEM_CONTEXT_NEW_END();
        }
        MEM_CONTEXT_END();

        hrnPqLocal.scriptIdx = 0;
    }

    // Copy records into local storage
    MEM_CONTEXT_BEGIN(hrnPqLocal.memContext)
    {
        for (unsigned int scriptIdx = 0; scriptIdx < scriptSize; scriptIdx++)
        {
            hrnPqLocal.script[hrnPqLocal.scriptSize] = script[scriptIdx];

            if (script[scriptIdx].param != NULL)
                hrnPqLocal.script[hrnPqLocal.scriptSize].param = strZ(strNewZ(script[scriptIdx].param));

            if (script[scriptIdx].resultZ != NULL)
                hrnPqLocal.script[hrnPqLocal.scriptSize].resultZ = strZ(strNewZ(script[scriptIdx].resultZ));

            hrnPqLocal.scriptSize++;
        }
    }
    MEM_CONTEXT_END();
}

/**********************************************************************************************************************************/
void
hrnPqScriptStrictSet(bool strict)
{
    hrnPqLocal.strict = strict;
}

/***********************************************************************************************************************************
Run pq script
***********************************************************************************************************************************/
static HrnPqScript *
hrnPqScriptRun(const char *const function, const VariantList *const param, const HrnPqScript *const parent)
{
    // If an error has already been thrown then throw the same error again
    if (hrnPqLocal.scriptFail)
        THROW(AssertError, hrnPqLocal.scriptError);

    // Convert params to json for comparison and reporting
    String *paramStr = NULL;

    if (param)
    {
        Variant *const varList = varNewVarLst(param);

        paramStr = jsonFromVar(varList);
        varFree(varList);
    }
    else
        paramStr = strNew();

    // Ensure script has not ended
    if (hrnPqLocal.scriptSize == 0)
    {
        snprintf(
            hrnPqLocal.scriptError, sizeof(hrnPqLocal.scriptError), "pq script ended before %s (%s)", function, strZ(paramStr));

        TEST_LOG_FMT(PQ_ERROR_PREFIX ": %s", hrnPqLocal.scriptError);
        hrnPqLocal.scriptFail = true;

        THROW(AssertError, hrnPqLocal.scriptError);
    }

    // Get current script item
    HrnPqScript *const result = &hrnPqLocal.script[hrnPqLocal.scriptIdx];

    // Check that expected function was called
    if (strcmp(result->function, function) != 0)
    {
        snprintf(
            hrnPqLocal.scriptError, sizeof(hrnPqLocal.scriptError), "pq script [%u] expected function %s (%s) but got %s (%s)",
            hrnPqLocal.scriptIdx, result->function, result->param == NULL ? "" : result->param, function, strZ(paramStr));

        TEST_LOG_FMT(PQ_ERROR_PREFIX ": %s", hrnPqLocal.scriptError);
        hrnPqLocal.scriptFail = true;

        // Return without error if closing the connection and an error is currently being thrown. Errors outside of the pq shim can
        // cause the connection to be cleaned up and we don't want to mask those errors. However, the failure is still logged and
        // any subsequent call to the pq shim will result in an error.
        if (strcmp(function, HRN_PQ_FINISH) == 0 && errorType() != NULL)
            return NULL;

        THROW(AssertError, hrnPqLocal.scriptError);
    }

    // Check that parameters match
    if ((param != NULL && result->param == NULL) || (param == NULL && result->param != NULL) ||
        (param != NULL && result->param != NULL && !strEqZ(paramStr, result->param)))
    {
        snprintf(
            hrnPqLocal.scriptError, sizeof(hrnPqLocal.scriptError), "pq script [%u] function '%s', expects param '%s' but got '%s'",
            hrnPqLocal.scriptIdx, result->function, result->param ? result->param : "NULL", param ? strZ(paramStr) : "NULL");

        TEST_LOG_FMT(PQ_ERROR_PREFIX ": %s", hrnPqLocal.scriptError);
        hrnPqLocal.scriptFail = true;

        THROW(AssertError, hrnPqLocal.scriptError);
    }

    // Make sure the session matches with the parent as a sanity check
    if (parent != NULL && result->session != parent->session)
    {
        snprintf(
            hrnPqLocal.scriptError, sizeof(hrnPqLocal.scriptError),
            "pq script [%u] function '%s', expects session '%u' but got '%u'", hrnPqLocal.scriptIdx, result->function,
            result->session, parent->session);

        TEST_LOG_FMT(PQ_ERROR_PREFIX ": %s", hrnPqLocal.scriptError);
        hrnPqLocal.scriptFail = true;

        THROW(AssertError, hrnPqLocal.scriptError);
    }

    strFree(paramStr);

    // Sleep if requested
    if (result->sleep > 0)
        sleepMSec(result->sleep);

    hrnPqLocal.scriptIdx++;

    if (hrnPqLocal.scriptIdx == hrnPqLocal.scriptSize)
    {
        memContextFree(hrnPqLocal.memContext);
        hrnPqLocal.scriptSize = 0;
    }

    return result;
}

/***********************************************************************************************************************************
Shim for PQconnectdb()
***********************************************************************************************************************************/
PGconn *
PQconnectdb(const char *conninfo)
{
    return (PGconn *)hrnPqScriptRun(HRN_PQ_CONNECTDB, varLstAdd(varLstNew(), varNewStrZ(conninfo)), NULL);
}

/***********************************************************************************************************************************
Shim for PQstatus()
***********************************************************************************************************************************/
ConnStatusType
PQstatus(const PGconn *conn)
{
    return (ConnStatusType)hrnPqScriptRun(HRN_PQ_STATUS, NULL, (const HrnPqScript *)conn)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQerrorMessage()
***********************************************************************************************************************************/
char *
PQerrorMessage(const PGconn *conn)
{
    // Const error messages are used for testing but the function requires them to be returned non-const. This should be safe
    // because the value is never modified by the calling function. If that changes, there will be a segfault during testing.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    return UNCONSTIFY(char *, hrnPqScriptRun(HRN_PQ_ERRORMESSAGE, NULL, (const HrnPqScript *)conn)->resultZ);
#pragma GCC diagnostic pop
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
    return hrnPqScriptRun(HRN_PQ_SENDQUERY, varLstAdd(varLstNew(), varNewStrZ(query)), (HrnPqScript *)conn)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQconsumeInput()
***********************************************************************************************************************************/
int
PQconsumeInput(PGconn *conn)
{
    return hrnPqScriptRun(HRN_PQ_CONSUMEINPUT, NULL, (HrnPqScript *)conn)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQisBusy()
***********************************************************************************************************************************/
int
PQisBusy(PGconn *conn)
{
    return hrnPqScriptRun(HRN_PQ_ISBUSY, NULL, (HrnPqScript *)conn)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQsocket()
***********************************************************************************************************************************/
int
PQsocket(const PGconn *conn)
{
    int result = hrnPqScriptRun(HRN_PQ_SOCKET, NULL, (const HrnPqScript *)conn)->resultInt;

    hrnFdReadyShimOne(result == true);

    return result;
}

/***********************************************************************************************************************************
Shim for PQgetCancel()
***********************************************************************************************************************************/
PGcancel *
PQgetCancel(PGconn *conn)
{
    HrnPqScript *const hrnPq = hrnPqScriptRun(HRN_PQ_GETCANCEL, NULL, (HrnPqScript *)conn);
    return hrnPq->resultNull ? NULL : (PGcancel *)hrnPq;
}

/***********************************************************************************************************************************
Shim for PQcancel()
***********************************************************************************************************************************/
int
PQcancel(PGcancel *cancel, char *errbuf, int errbufsize)
{
    HrnPqScript *const hrnPq = hrnPqScriptRun(HRN_PQ_CANCEL, NULL, (HrnPqScript *)cancel);

    if (!hrnPq->resultInt)
    {
        strncpy(errbuf, hrnPq->resultZ, (size_t)errbufsize);
        errbuf[errbufsize - 1] = '\0';
    }

    return hrnPq->resultInt;
}

/***********************************************************************************************************************************
Shim for PQfreeCancel()
***********************************************************************************************************************************/
void
PQfreeCancel(PGcancel *cancel)
{
    hrnPqScriptRun(HRN_PQ_FREECANCEL, NULL, (HrnPqScript *)cancel);
}

/***********************************************************************************************************************************
Shim for PQgetResult()
***********************************************************************************************************************************/
PGresult *
PQgetResult(PGconn *conn)
{
    if (!hrnPqLocal.scriptFail)
    {
        HrnPqScript *const hrnPq = hrnPqScriptRun(HRN_PQ_GETRESULT, NULL, (HrnPqScript *)conn);
        return hrnPq->resultNull ? NULL : (PGresult *)hrnPq;
    }

    return NULL;
}

/***********************************************************************************************************************************
Shim for PQresultStatus()
***********************************************************************************************************************************/
ExecStatusType
PQresultStatus(const PGresult *res)
{
    return (ExecStatusType)hrnPqScriptRun(HRN_PQ_RESULTSTATUS, NULL, (const HrnPqScript *)res)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQresultErrorMessage()
***********************************************************************************************************************************/
char *
PQresultErrorMessage(const PGresult *res)
{
    // Const error messages are used for testing but the function requires them to be returned non-const. This should be safe
    // because the value is never modified by the calling function. If that changes, there will be a segfault during testing.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    return UNCONSTIFY(char *, hrnPqScriptRun(HRN_PQ_RESULTERRORMESSAGE, NULL, (const HrnPqScript *)res)->resultZ);
#pragma GCC diagnostic pop
}

/***********************************************************************************************************************************
Shim for PQntuples()
***********************************************************************************************************************************/
int
PQntuples(const PGresult *res)
{
    return hrnPqScriptRun(HRN_PQ_NTUPLES, NULL, (const HrnPqScript *)res)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQnfields()
***********************************************************************************************************************************/
int
PQnfields(const PGresult *res)
{
    return hrnPqScriptRun(HRN_PQ_NFIELDS, NULL, (const HrnPqScript *)res)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQgetisnull()
***********************************************************************************************************************************/
int
PQgetisnull(const PGresult *res, int tup_num, int field_num)
{
    return hrnPqScriptRun(
        HRN_PQ_GETISNULL, varLstAdd(varLstAdd(varLstNew(), varNewInt(tup_num)), varNewInt(field_num)),
        (const HrnPqScript *)res)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQftype()
***********************************************************************************************************************************/
Oid
PQftype(const PGresult *res, int field_num)
{
    return (Oid)hrnPqScriptRun(HRN_PQ_FTYPE, varLstAdd(varLstNew(), varNewInt(field_num)), (const HrnPqScript *)res)->resultInt;
}

/***********************************************************************************************************************************
Shim for PQgetvalue()
***********************************************************************************************************************************/
char *
PQgetvalue(const PGresult *res, int tup_num, int field_num)
{
    // Const values are used for testing but the function requires them to be returned non-const. This should be safe because the
    // value is never modified by the calling function. If that changes, there will be a segfault during testing.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    return UNCONSTIFY(
        char *,
        hrnPqScriptRun(
            HRN_PQ_GETVALUE, varLstAdd(varLstAdd(varLstNew(), varNewInt(tup_num)), varNewInt(field_num)),
            (const HrnPqScript *)res)->resultZ);
#pragma GCC diagnostic pop
}

/***********************************************************************************************************************************
Shim for PQclear()
***********************************************************************************************************************************/
void
PQclear(PGresult *res)
{
    if (!hrnPqLocal.scriptFail)
        hrnPqScriptRun(HRN_PQ_CLEAR, NULL, (HrnPqScript *)res);
}

/***********************************************************************************************************************************
Shim for PQfinish()
***********************************************************************************************************************************/
void
PQfinish(PGconn *conn)
{
    if (hrnPqLocal.strict && !hrnPqLocal.scriptFail)
        hrnPqScriptRun(HRN_PQ_FINISH, NULL, (HrnPqScript *)conn);
}

#endif // HARNESS_PQ_REAL
