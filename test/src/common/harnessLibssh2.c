/***********************************************************************************************************************************
libssh2 Test Harness
***********************************************************************************************************************************/
//#ifndef HARNESS_LIBSSH2_REAL

#include "build.auto.h"

#include <stdio.h>
#include <string.h>

#include <libssh2.h>
#include <libssh2_sftp.h>

#include "common/type/json.h"
#include "common/type/string.h"
#include "common/type/variantList.h"

#include "common/harnessLibssh2.h"
#include "common/harnessTest.h"

/***********************************************************************************************************************************
libssh2 shim error prefix
***********************************************************************************************************************************/
#define LIBSSH2_ERROR_PREFIX                                             "LIBSSH2 SHIM ERROR"

/***********************************************************************************************************************************
Script that defines how shim functions operate
***********************************************************************************************************************************/
HarnessLibssh2 harnessLibssh2Script[1024];
bool harnessLibssh2ScriptDone = true;
unsigned int harnessLibssh2ScriptIdx;

// If there is a script failure change the behavior of cleanup functions to return immediately so the real error will be reported
// rather than a bogus scripting error during cleanup
bool harnessLibssh2ScriptFail;
char harnessLibssh2ScriptError[4096];

/***********************************************************************************************************************************
Set libssh2 script
***********************************************************************************************************************************/
void
harnessLibssh2ScriptSet(HarnessLibssh2 *harnessLibssh2ScriptParam)
{
    if (!harnessLibssh2ScriptDone)
        THROW(AssertError, "previous libssh2 script has not yet completed");

    if (harnessLibssh2ScriptParam[0].function == NULL)
        THROW(AssertError, "libssh2 script must have entries");

    // Copy records into local storage
    unsigned int copyIdx = 0;

    while (harnessLibssh2ScriptParam[copyIdx].function != NULL)
    {
        harnessLibssh2Script[copyIdx] = harnessLibssh2ScriptParam[copyIdx];
        copyIdx++;
    }

    harnessLibssh2Script[copyIdx].function = NULL;
    harnessLibssh2ScriptDone = false;
    harnessLibssh2ScriptIdx = 0;
}

/***********************************************************************************************************************************
Run libssh2 script
***********************************************************************************************************************************/
static HarnessLibssh2 *
harnessLibssh2ScriptRun(const char *const function, const VariantList *const param, const HarnessLibssh2 *const parent)
{
    // If an error has already been thrown then throw the same error again
    if (harnessLibssh2ScriptFail)
        THROW(AssertError, harnessLibssh2ScriptError);

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
    if (harnessLibssh2ScriptDone)
    {
        snprintf(
            harnessLibssh2ScriptError, sizeof(harnessLibssh2ScriptError), "libssh2 script ended before %s (%s)", function,
            strZ(paramStr));

        TEST_LOG_FMT(LIBSSH2_ERROR_PREFIX ": %s", harnessLibssh2ScriptError);
        harnessLibssh2ScriptFail = true;

        THROW(AssertError, harnessLibssh2ScriptError);
    }

    // Get current script item
    HarnessLibssh2 *result = &harnessLibssh2Script[harnessLibssh2ScriptIdx];

    // Check that expected function was called
    if (strcmp(result->function, function) != 0)
    {
        snprintf(
            harnessLibssh2ScriptError, sizeof(harnessLibssh2ScriptError),
            "libssh2 script [%u] expected function %s (%s) but got %s (%s)", harnessLibssh2ScriptIdx, result->function,
            result->param == NULL ? "" : result->param, function, strZ(paramStr));

        TEST_LOG_FMT(LIBSSH2_ERROR_PREFIX ": %s", harnessLibssh2ScriptError);
        harnessLibssh2ScriptFail = true;

        // Return without error if closing the connection and an error is currently being thrown. Errors outside of the libssh2 shim
        // can cause the connection to be cleaned up and we don't want to mask those errors. However, the failure is still logged
        // and any subsequent call to the libssh2 shim will result in an error.
//        if (strcmp(function, HRNPQ_FINISH) == 0 && errorType() != NULL)
//            return NULL;

        THROW(AssertError, harnessLibssh2ScriptError);
    }

    // Check that parameters match
    if ((param != NULL && result->param == NULL) || (param == NULL && result->param != NULL) ||
        (param != NULL && result->param != NULL && !strEqZ(paramStr, result->param)))
    {
        snprintf(
            harnessLibssh2ScriptError, sizeof(harnessLibssh2ScriptError),
            "libssh2 script [%u] function '%s', expects param '%s' but got '%s'",
            harnessLibssh2ScriptIdx, result->function, result->param ? result->param : "NULL", param ? strZ(paramStr) : "NULL");

        TEST_LOG_FMT(LIBSSH2_ERROR_PREFIX ": %s", harnessLibssh2ScriptError);
        harnessLibssh2ScriptFail = true;

        THROW(AssertError, harnessLibssh2ScriptError);
    }

    // Make sure the session matches with the parent as a sanity check
    if (parent != NULL && result->session != parent->session)
    {
        snprintf(
            harnessLibssh2ScriptError, sizeof(harnessLibssh2ScriptError),
            "libssh2 script [%u] function '%s', expects session '%u' but got '%u'",
            harnessLibssh2ScriptIdx, result->function, result->session, parent->session);

        TEST_LOG_FMT(LIBSSH2_ERROR_PREFIX ": %s", harnessLibssh2ScriptError);
        harnessLibssh2ScriptFail = true;

        THROW(AssertError, harnessLibssh2ScriptError);
    }

    // Sleep if requested
    if (result->sleep > 0)
        sleepMSec(result->sleep);

    harnessLibssh2ScriptIdx++;

    if (harnessLibssh2Script[harnessLibssh2ScriptIdx].function == NULL)
        harnessLibssh2ScriptDone = true;

    strFree(paramStr);

    return result;
}

/***********************************************************************************************************************************
Shim for libssh2_init
***********************************************************************************************************************************/
int libssh2_init(int flags)
{
    HarnessLibssh2 *harnessLibssh2 = harnessLibssh2ScriptRun(HRNLIBSSH2_INIT, varLstAdd(varLstNew(), varNewInt(flags)), NULL);

    return harnessLibssh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_session_init
***********************************************************************************************************************************/
// !!! jrt this needs to be rewritten to function correctly
LIBSSH2_SESSION *libssh2_session_init_ex(
    LIBSSH2_ALLOC_FUNC((*myalloc)), LIBSSH2_FREE_FUNC((*myfree)), LIBSSH2_REALLOC_FUNC((*myrealloc)), void *abstract)
{
    // All of these should always be the default NULL
    if (myalloc != NULL && myfree != NULL && myrealloc != NULL && abstract != NULL)
    {
        snprintf(
            harnessLibssh2ScriptError, sizeof(harnessLibssh2ScriptError),
            "libssh2 script function 'libssh2_session_init_ex', expects all params to be NULL");
        THROW(AssertError, harnessLibssh2ScriptError);
    }

    HarnessLibssh2 *harnessLibssh2 = harnessLibssh2ScriptRun(
            HRNLIBSSH2_SESSION_INIT_EX,
            varLstAdd(
                varLstAdd(
                    varLstAdd(
                        varLstAdd(varLstNew(), varNewStr(NULL)),
                        varNewStr(NULL)),
                    varNewStr(NULL)),
                varNewStr(NULL)),
            NULL);

    return harnessLibssh2->resultNull ? NULL : (LIBSSH2_SESSION *)harnessLibssh2;
}

/***********************************************************************************************************************************
Shim for libssh2_session_set_blocking
***********************************************************************************************************************************/
void libssh2_session_set_blocking(LIBSSH2_SESSION* session, int blocking)
{
    if (session == NULL)
    {
        snprintf(
            harnessLibssh2ScriptError, sizeof(harnessLibssh2ScriptError),
            "libssh2 script function 'libssh2_session_set_blocking', expects session to be not NULL");
        THROW(AssertError, harnessLibssh2ScriptError);
    }

    if (!(INT_MIN <= blocking && blocking <= INT_MAX))
    {
        snprintf(
            harnessLibssh2ScriptError, sizeof(harnessLibssh2ScriptError),
            "libssh2 script function 'libssh2_session_set_blocking', expects blocking to be an integer value");
        THROW(AssertError, harnessLibssh2ScriptError);
    }

    return;
}

/***********************************************************************************************************************************
Shim for libssh2_session_handshake
***********************************************************************************************************************************/
int libssh2_session_handshake(LIBSSH2_SESSION *session, libssh2_socket_t sock)
{
    return harnessLibssh2ScriptRun(
            HRNLIBSSH2_SESSION_HANDSHAKE, varLstAdd(varLstNew(), varNewInt(sock)), (HarnessLibssh2 *)session)->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_hostkey_hash
***********************************************************************************************************************************/
const char *libssh2_hostkey_hash(LIBSSH2_SESSION *session, int hash_type)
{
    HarnessLibssh2 *harnessLibssh2 = harnessLibssh2ScriptRun(
        HRNLIBSSH2_HOSTKEY_HASH, varLstAdd(varLstNew(), varNewInt(hash_type)), (HarnessLibssh2 *)session);
    return harnessLibssh2->resultNull ? NULL : (const char *)harnessLibssh2->resultZ;
}

/***********************************************************************************************************************************
Shim for libssh2_userauth_publickey_fromfile_ex
***********************************************************************************************************************************/
int libssh2_userauth_publickey_fromfile_ex(
    LIBSSH2_SESSION *session, const char *username, unsigned int ousername_len, const char *publickey, const char *privatekey,
    const char *passphrase)
{
    HarnessLibssh2 *harnessLibssh2 = harnessLibssh2ScriptRun(
        HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
            varLstAdd(
                varLstAdd(
                    varLstAdd(
                        varLstAdd(
                            varLstAdd(varLstNew(), varNewStrZ(username)),
                            varNewUInt(ousername_len)),
                        varNewStrZ(publickey)),
                    varNewStrZ(privatekey)),
                varNewStrZ(passphrase)),
            (HarnessLibssh2 *)session);

    return harnessLibssh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_init
***********************************************************************************************************************************/
LIBSSH2_SFTP *libssh2_sftp_init(LIBSSH2_SESSION *session)
{
    HarnessLibssh2 *harnessLibssh2 = harnessLibssh2ScriptRun(HRNLIBSSH2_SFTP_INIT, NULL, (HarnessLibssh2 *)session);
    return harnessLibssh2->resultNull ? NULL : (LIBSSH2_SFTP *)harnessLibssh2;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_close_handle
***********************************************************************************************************************************/
int libssh2_sftp_close_handle(LIBSSH2_SFTP_HANDLE *handle)
{
    return harnessLibssh2ScriptRun(HRNLIBSSH2_SFTP_CLOSE_HANDLE, NULL, (HarnessLibssh2 *)handle)->resultInt;

}

/***********************************************************************************************************************************
Shim for libssh2_sftp_shutdown
***********************************************************************************************************************************/
int libssh2_sftp_shutdown(LIBSSH2_SFTP *sftp)
{
    return harnessLibssh2ScriptRun(HRNLIBSSH2_SFTP_CLOSE_HANDLE, NULL, (HarnessLibssh2 *)sftp)->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_session_disconnect_ex
***********************************************************************************************************************************/
int libssh2_session_disconnect_ex(LIBSSH2_SESSION *session, int reason, const char *description, const char *lang)
{
    HarnessLibssh2 *harnessLibssh2 = harnessLibssh2ScriptRun(
        HRNLIBSSH2_SESSION_DISCONNECT_EX,
        varLstAdd(
            varLstAdd(
                varLstAdd(
                    varLstNew(), varNewInt(reason)),
                varNewStrZ(description)),
            varNewStrZ(lang)),
        (HarnessLibssh2 *)session);

    return harnessLibssh2->resultInt;
}

/***********************************************************************************************************************************
Shim for int libssh2_session_free
***********************************************************************************************************************************/
int libssh2_session_free(LIBSSH2_SESSION *session)
{
    return harnessLibssh2ScriptRun(HRNLIBSSH2_SFTP_CLOSE_HANDLE, NULL, (HarnessLibssh2 *)session)->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_exit(void)
***********************************************************************************************************************************/
void libssh2_exit(void)
{
    return;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_stat_ex
***********************************************************************************************************************************/
int libssh2_sftp_stat_ex(
    LIBSSH2_SFTP *sftp, const char *path, unsigned int path_len, int stat_type, LIBSSH2_SFTP_ATTRIBUTES *attrs)
{
    HarnessLibssh2 *harnessLibssh2 = harnessLibssh2ScriptRun(
        HRNLIBSSH2_SFTP_STAT_EX,
        varLstAdd(
            varLstAdd(
                varLstAdd(
                    varLstNew(), varNewStrZ(path)),
                varNewUInt(path_len)),
            varNewInt(stat_type)),
        (HarnessLibssh2 *)sftp);

    if (attrs == NULL)
        THROW(AssertError, "attrs is NULL");

    attrs->permissions = 0;
    attrs->permissions |= harnessLibssh2->attrPerms;

    return harnessLibssh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_last_error
***********************************************************************************************************************************/
unsigned long libssh2_sftp_last_error(LIBSSH2_SFTP *sftp)
{
    return harnessLibssh2ScriptRun(HRNLIBSSH2_SFTP_LAST_ERROR, NULL, (HarnessLibssh2 *)sftp)->resultUInt;
}

/***********************************************************************************************************************************
Shim for PQconnectdb()
**********************************************************************************************************************************
PGconn *PQconnectdb(const char *conninfo)
{
    return (PGconn *)harnessLibssh2ScriptRun(HRNPQ_CONNECTDB, varLstAdd(varLstNew(), varNewStrZ(conninfo)), NULL);
}
*/
/***********************************************************************************************************************************
Shim for PQstatus()
**********************************************************************************************************************************
ConnStatusType PQstatus(const PGconn *conn)
{
    return (ConnStatusType)harnessLibssh2ScriptRun(HRNPQ_STATUS, NULL, (HarnessLibssh2 *)conn)->resultInt;
}
*/
/***********************************************************************************************************************************
Shim for PQerrorMessage()
**********************************************************************************************************************************
char *PQerrorMessage(const PGconn *conn)
{
    return (char *)harnessLibssh2ScriptRun(HRNPQ_ERRORMESSAGE, NULL, (HarnessLibssh2 *)conn)->resultZ;
}
*/
/***********************************************************************************************************************************
Shim for PQsetNoticeProcessor()
**********************************************************************************************************************************
PQnoticeProcessor
PQsetNoticeProcessor(PGconn *conn, PQnoticeProcessor proc, void *arg)
{
    (void)conn;

    // Call the processor that was passed so we have coverage
    proc(arg, "test notice");
    return NULL;
}
*/
/***********************************************************************************************************************************
Shim for PQsendQuery()
**********************************************************************************************************************************
int
PQsendQuery(PGconn *conn, const char *query)
{
    return harnessLibssh2ScriptRun(HRNPQ_SENDQUERY, varLstAdd(varLstNew(), varNewStrZ(query)), (HarnessLibssh2 *)conn)->resultInt;
}
*/
/***********************************************************************************************************************************
Shim for PQconsumeInput()
**********************************************************************************************************************************
int
PQconsumeInput(PGconn *conn)
{
    return harnessLibssh2ScriptRun(HRNPQ_CONSUMEINPUT, NULL, (HarnessLibssh2 *)conn)->resultInt;
}
*/
/***********************************************************************************************************************************
Shim for PQisBusy()
**********************************************************************************************************************************
int
PQisBusy(PGconn *conn)
{
    return harnessLibssh2ScriptRun(HRNPQ_ISBUSY, NULL, (HarnessLibssh2 *)conn)->resultInt;
}
*/
/***********************************************************************************************************************************
Shim for PQgetCancel()
**********************************************************************************************************************************
PGcancel *
PQgetCancel(PGconn *conn)
{
    HarnessLibssh2 *harnessLibssh2 = harnessLibssh2ScriptRun(HRNPQ_GETCANCEL, NULL, (HarnessLibssh2 *)conn);
    return harnessLibssh2->resultNull ? NULL : (PGcancel *)harnessLibssh2;
}
*/
/***********************************************************************************************************************************
Shim for PQcancel()
**********************************************************************************************************************************
int
PQcancel(PGcancel *cancel, char *errbuf, int errbufsize)
{
    HarnessLibssh2 *harnessLibssh2 = harnessLibssh2ScriptRun(HRNPQ_CANCEL, NULL, (HarnessLibssh2 *)cancel);

    if (!harnessLibssh2->resultInt)
    {
        strncpy(errbuf, harnessLibssh2->resultZ, (size_t)errbufsize);
        errbuf[errbufsize - 1] = '\0';
    }

    return harnessLibssh2->resultInt;
}
*/
/***********************************************************************************************************************************
Shim for PQfreeCancel()
**********************************************************************************************************************************
void
PQfreeCancel(PGcancel *cancel)
{
    harnessLibssh2ScriptRun(HRNPQ_FREECANCEL, NULL, (HarnessLibssh2 *)cancel);
}
*/
/***********************************************************************************************************************************
Shim for PQgetResult()
**********************************************************************************************************************************
PGresult *
PQgetResult(PGconn *conn)
{
    if (!harnessLibssh2ScriptFail)
    {
        HarnessLibssh2 *harnessLibssh2 = harnessLibssh2ScriptRun(HRNPQ_GETRESULT, NULL, (HarnessLibssh2 *)conn);
        return harnessLibssh2->resultNull ? NULL : (PGresult *)harnessLibssh2;
    }

    return NULL;
}
*/
/***********************************************************************************************************************************
Shim for PQresultStatus()
**********************************************************************************************************************************
ExecStatusType
PQresultStatus(const PGresult *res)
{
    return (ExecStatusType)harnessLibssh2ScriptRun(HRNPQ_RESULTSTATUS, NULL, (HarnessLibssh2 *)res)->resultInt;
}
*/
/***********************************************************************************************************************************
Shim for PQresultErrorMessage()
**********************************************************************************************************************************
char *
PQresultErrorMessage(const PGresult *res)
{
    return (char *)harnessLibssh2ScriptRun(HRNPQ_RESULTERRORMESSAGE, NULL, (HarnessLibssh2 *)res)->resultZ;
}
*/
/***********************************************************************************************************************************
Shim for PQntuples()
**********************************************************************************************************************************
int
PQntuples(const PGresult *res)
{
    return harnessLibssh2ScriptRun(HRNPQ_NTUPLES, NULL, (HarnessLibssh2 *)res)->resultInt;
}
*/
/***********************************************************************************************************************************
Shim for PQnfields()
**********************************************************************************************************************************
int
PQnfields(const PGresult *res)
{
    return harnessLibssh2ScriptRun(HRNPQ_NFIELDS, NULL, (HarnessLibssh2 *)res)->resultInt;
}
*/
/***********************************************************************************************************************************
Shim for PQgetisnull()
**********************************************************************************************************************************
int
PQgetisnull(const PGresult *res, int tup_num, int field_num)
{
    return harnessLibssh2ScriptRun(
        HRNPQ_GETISNULL, varLstAdd(varLstAdd(varLstNew(), varNewInt(tup_num)), varNewInt(field_num)), (HarnessLibssh2 *)res)->resultInt;
}
*/
/***********************************************************************************************************************************
Shim for PQftype()
**********************************************************************************************************************************
Oid
PQftype(const PGresult *res, int field_num)
{
    return (Oid)harnessLibssh2ScriptRun(HRNPQ_FTYPE, varLstAdd(varLstNew(), varNewInt(field_num)), (HarnessLibssh2 *)res)->resultInt;
}
*/
/***********************************************************************************************************************************
Shim for PQgetvalue()
**********************************************************************************************************************************
char *
PQgetvalue(const PGresult *res, int tup_num, int field_num)
{
    return (char *)harnessLibssh2ScriptRun(
        HRNPQ_GETVALUE, varLstAdd(varLstAdd(varLstNew(), varNewInt(tup_num)), varNewInt(field_num)), (HarnessLibssh2 *)res)->resultZ;
}
*/
/***********************************************************************************************************************************
Shim for PQclear()
**********************************************************************************************************************************
void
PQclear(PGresult *res)
{
    if (!harnessLibssh2ScriptFail)
        harnessLibssh2ScriptRun(HRNPQ_CLEAR, NULL, (HarnessLibssh2 *)res);
}
*/
//#endif // HARNESS_LIBSSH2_REAL
