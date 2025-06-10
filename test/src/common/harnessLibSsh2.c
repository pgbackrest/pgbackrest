/***********************************************************************************************************************************
libssh2 Test Harness
***********************************************************************************************************************************/
#include "build.auto.h"

#ifdef HAVE_LIBSSH2

#include <stdio.h>
#include <string.h>

#include "common/type/json.h"
#include "common/type/string.h"
#include "common/type/variantList.h"

#include "common/harnessLibSsh2.h"
#include "common/harnessTest.h"

/***********************************************************************************************************************************
libssh2 shim error prefix
***********************************************************************************************************************************/
#define LIBSSH2_ERROR_PREFIX                                        "LIBSSH2 SHIM ERROR"

/***********************************************************************************************************************************
Script that defines how shim functions operate
***********************************************************************************************************************************/
static HrnLibSsh2 hrnLibSsh2Script[1024];
static bool hrnLibSsh2ScriptDone = true;
static unsigned int hrnLibSsh2ScriptIdx;

// If there is a script failure change the behavior of cleanup functions to return immediately so the real error will be reported
// rather than a bogus scripting error during cleanup
static bool hrnLibSsh2ScriptFail;
static char hrnLibSsh2ScriptError[4096];

/***********************************************************************************************************************************
Set libssh2 script
***********************************************************************************************************************************/
void
hrnLibSsh2ScriptSet(HrnLibSsh2 *hrnLibSsh2ScriptParam)
{
    if (!hrnLibSsh2ScriptDone)
        THROW(AssertError, "previous libssh2 script has not yet completed");

    if (hrnLibSsh2ScriptParam[0].function == NULL)
        THROW(AssertError, "libssh2 script must have entries");

    // Copy records into local storage
    unsigned int copyIdx = 0;

    while (hrnLibSsh2ScriptParam[copyIdx].function != NULL)
    {
        hrnLibSsh2Script[copyIdx] = hrnLibSsh2ScriptParam[copyIdx];
        copyIdx++;
    }

    hrnLibSsh2Script[copyIdx].function = NULL;
    hrnLibSsh2ScriptDone = false;
    hrnLibSsh2ScriptIdx = 0;
}

/***********************************************************************************************************************************
Run libssh2 script
***********************************************************************************************************************************/
static HrnLibSsh2 *
hrnLibSsh2ScriptRun(const char *const function, const VariantList *const param, const HrnLibSsh2 *const parent)
{
    // If an error has already been thrown then throw the same error again
    if (hrnLibSsh2ScriptFail)
        THROW(AssertError, hrnLibSsh2ScriptError);

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
    if (hrnLibSsh2ScriptDone)
    {
        snprintf(
            hrnLibSsh2ScriptError, sizeof(hrnLibSsh2ScriptError), "libssh2 script ended before %s (%s)", function,
            strZ(paramStr));

        TEST_LOG_FMT(LIBSSH2_ERROR_PREFIX ": %s", hrnLibSsh2ScriptError);
        hrnLibSsh2ScriptFail = true;

        THROW(AssertError, hrnLibSsh2ScriptError);
    }

    // Get current script item
    HrnLibSsh2 *result = &hrnLibSsh2Script[hrnLibSsh2ScriptIdx];

    // Check that expected function was called
    if (strcmp(result->function, function) != 0)
    {
        snprintf(
            hrnLibSsh2ScriptError, sizeof(hrnLibSsh2ScriptError),
            "libssh2 script [%u] expected function %s (%s) but got %s (%s)", hrnLibSsh2ScriptIdx, result->function,
            result->param == NULL ? "" : result->param, function, strZ(paramStr));

        TEST_LOG_FMT(LIBSSH2_ERROR_PREFIX ": %s", hrnLibSsh2ScriptError);
        hrnLibSsh2ScriptFail = true;

        THROW(AssertError, hrnLibSsh2ScriptError);
    }

    // Check that parameters match
    if ((param != NULL && result->param == NULL) || (param == NULL && result->param != NULL) ||
        (param != NULL && result->param != NULL && !strEqZ(paramStr, result->param)))
    {
        snprintf(
            hrnLibSsh2ScriptError, sizeof(hrnLibSsh2ScriptError),
            "libssh2 script [%u] function '%s', expects param '%s' but got '%s'",
            hrnLibSsh2ScriptIdx, result->function, result->param ? result->param : "NULL", param ? strZ(paramStr) : "NULL");

        TEST_LOG_FMT(LIBSSH2_ERROR_PREFIX ": %s", hrnLibSsh2ScriptError);
        hrnLibSsh2ScriptFail = true;

        THROW(AssertError, hrnLibSsh2ScriptError);
    }

    // Make sure the session matches with the parent as a sanity check
    if (parent != NULL && result->session != parent->session)
    {
        snprintf(
            hrnLibSsh2ScriptError, sizeof(hrnLibSsh2ScriptError),
            "libssh2 script [%u] function '%s', expects session '%u' but got '%u'",
            hrnLibSsh2ScriptIdx, result->function, result->session, parent->session);

        TEST_LOG_FMT(LIBSSH2_ERROR_PREFIX ": %s", hrnLibSsh2ScriptError);
        hrnLibSsh2ScriptFail = true;

        THROW(AssertError, hrnLibSsh2ScriptError);
    }

    // Sleep if requested
    if (result->sleep > 0)
        sleepMSec(result->sleep);

    hrnLibSsh2ScriptIdx++;

    if (hrnLibSsh2Script[hrnLibSsh2ScriptIdx].function == NULL)
        hrnLibSsh2ScriptDone = true;

    strFree(paramStr);

    return result;
}

/***********************************************************************************************************************************
Shim for libssh2_init
***********************************************************************************************************************************/
int
libssh2_init(int flags)
{
    HrnLibSsh2 *hrnLibSsh2 = hrnLibSsh2ScriptRun(HRNLIBSSH2_INIT, varLstAdd(varLstNew(), varNewInt(flags)), NULL);

    return hrnLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_knownhost_addc
***********************************************************************************************************************************/
int
libssh2_knownhost_addc(
    LIBSSH2_KNOWNHOSTS *hosts, const char *host, const char *salt, const char *key, size_t keylen, const char *comment,
    size_t commentlen, int typemask, struct libssh2_knownhost **store)
{
    // Avoid compiler complaining of unused param
    (void)store;

    if (hosts == NULL)
    {
        snprintf(
            hrnLibSsh2ScriptError, sizeof(hrnLibSsh2ScriptError),
            "libssh2 script function 'libssh2_knownhost_adddc', expects hosts to be not NULL");
        THROW(AssertError, hrnLibSsh2ScriptError);
    }

    HrnLibSsh2 *hrnLibSsh2 = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        hrnLibSsh2 = hrnLibSsh2ScriptRun(
            HRNLIBSSH2_KNOWNHOST_ADDC,
            varLstAdd(
                varLstAdd(
                    varLstAdd(
                        varLstAdd(
                            varLstAdd(
                                varLstAdd(
                                    varLstAdd(
                                        varLstNew(), varNewStrZ(host)),
                                    varNewStrZ(salt)),
                                varNewStrZ(key)),
                            varNewUInt64(keylen)),
                        varNewStrZ(comment)),
                    varNewUInt64(commentlen)),
                varNewInt(typemask)),
            (HrnLibSsh2 *)hosts);
    }
    MEM_CONTEXT_TEMP_END();

    return hrnLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_knownhost_checkp
***********************************************************************************************************************************/
int
libssh2_knownhost_checkp(
    LIBSSH2_KNOWNHOSTS *hosts, const char *host, int port, const char *key, size_t keylen, int typemask,
    struct libssh2_knownhost **knownhost)
{
    // Avoid compiler complaining of unused param
    (void)knownhost;

    if (hosts == NULL)
    {
        snprintf(
            hrnLibSsh2ScriptError, sizeof(hrnLibSsh2ScriptError),
            "libssh2 script function 'libssh2_knownhost_checkp', expects hosts to be not NULL");
        THROW(AssertError, hrnLibSsh2ScriptError);
    }

    HrnLibSsh2 *hrnLibSsh2 = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        hrnLibSsh2 = hrnLibSsh2ScriptRun(
            HRNLIBSSH2_KNOWNHOST_CHECKP,
            varLstAdd(
                varLstAdd(
                    varLstAdd(
                        varLstAdd(
                            varLstAdd(
                                varLstNew(), varNewStrZ(host)),
                            varNewInt(port)),
                        varNewStrZ(key)),
                    varNewUInt64(keylen)),
                varNewInt(typemask)),
            (HrnLibSsh2 *)hosts);
    }
    MEM_CONTEXT_TEMP_END();

    return hrnLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_knownhost_free
***********************************************************************************************************************************/
void
libssh2_knownhost_free(LIBSSH2_KNOWNHOSTS *hosts)
{
    if (hosts == NULL)
    {
        snprintf(
            hrnLibSsh2ScriptError, sizeof(hrnLibSsh2ScriptError),
            "libssh2 script function 'libssh2_session_knownhost_free', expects hosts to be not NULL");
        THROW(AssertError, hrnLibSsh2ScriptError);
    }
}

/***********************************************************************************************************************************
Shim for libssh2_knownhost_init
***********************************************************************************************************************************/
LIBSSH2_KNOWNHOSTS *
libssh2_knownhost_init(LIBSSH2_SESSION *session)
{
    HrnLibSsh2 *hrnLibSsh2 = hrnLibSsh2ScriptRun(HRNLIBSSH2_KNOWNHOST_INIT, NULL, (HrnLibSsh2 *)session);

    return hrnLibSsh2->resultNull ? NULL : (LIBSSH2_KNOWNHOSTS *)hrnLibSsh2;
}

/***********************************************************************************************************************************
Shim for libssh2_knownhost_readfile
***********************************************************************************************************************************/
int
libssh2_knownhost_readfile(LIBSSH2_KNOWNHOSTS *hosts, const char *filename, int type)
{
    HrnLibSsh2 *hrnLibSsh2 = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        hrnLibSsh2 = hrnLibSsh2ScriptRun(
            HRNLIBSSH2_KNOWNHOST_READFILE,
            varLstAdd(
                varLstAdd(
                    varLstNew(), varNewStrZ(filename)),
                varNewInt(type)),
            (HrnLibSsh2 *)hosts);
    }
    MEM_CONTEXT_TEMP_END();

    return hrnLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_knownhost_writefile
***********************************************************************************************************************************/
int
libssh2_knownhost_writefile(LIBSSH2_KNOWNHOSTS *hosts, const char *filename, int type)
{
    HrnLibSsh2 *hrnLibSsh2 = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        hrnLibSsh2 = hrnLibSsh2ScriptRun(
            HRNLIBSSH2_KNOWNHOST_WRITEFILE,
            varLstAdd(
                varLstAdd(
                    varLstNew(), varNewStrZ(filename)),
                varNewInt(type)),
            (HrnLibSsh2 *)hosts);
    }
    MEM_CONTEXT_TEMP_END();

    return hrnLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_session_hostkey
***********************************************************************************************************************************/
const char *
libssh2_session_hostkey(LIBSSH2_SESSION *session, size_t *len, int *type)
{
    HrnLibSsh2 *hrnLibSsh2 = hrnLibSsh2ScriptRun(HRNLIBSSH2_SESSION_HOSTKEY, NULL, (HrnLibSsh2 *)session);

    *len = (size_t)hrnLibSsh2->len;
    *type = (int)hrnLibSsh2->type;

    return hrnLibSsh2->resultNull ? NULL : (const char *)hrnLibSsh2->resultZ;
}

/***********************************************************************************************************************************
Shim for libssh2_session_init
***********************************************************************************************************************************/
LIBSSH2_SESSION *
libssh2_session_init_ex(
    LIBSSH2_ALLOC_FUNC((*myalloc)), LIBSSH2_FREE_FUNC((*myfree)), LIBSSH2_REALLOC_FUNC((*myrealloc)), void *abstract)
{
    // All of these should always be the default NULL
    if (myalloc != NULL && myfree != NULL && myrealloc != NULL && abstract != NULL)
    {
        snprintf(
            hrnLibSsh2ScriptError, sizeof(hrnLibSsh2ScriptError),
            "libssh2 script function 'libssh2_session_init_ex', expects all params to be NULL");
        THROW(AssertError, hrnLibSsh2ScriptError);
    }

    HrnLibSsh2 *hrnLibSsh2 = hrnLibSsh2ScriptRun(
        HRNLIBSSH2_SESSION_INIT_EX,
        varLstAdd(
            varLstAdd(
                varLstAdd(
                    varLstAdd(
                        varLstNew(), varNewStr(NULL)),
                    varNewStr(NULL)),
                varNewStr(NULL)),
            varNewStr(NULL)),
        NULL);

    return hrnLibSsh2->resultNull ? NULL : (LIBSSH2_SESSION *)hrnLibSsh2;
}

/***********************************************************************************************************************************
Shim for libssh2_session_last_error
***********************************************************************************************************************************/
int
libssh2_session_last_error(LIBSSH2_SESSION *session, char **errmsg, int *errmsg_len, int want_buf)
{
    // Avoid compiler complaining of unused params
    (void)want_buf;

    HrnLibSsh2 *hrnLibSsh2 = hrnLibSsh2ScriptRun(HRNLIBSSH2_SESSION_LAST_ERROR, NULL, (HrnLibSsh2 *)session);

    if (hrnLibSsh2->errMsg != NULL)
    {
        // Const error messages are used for testing but the function requires them to be returned non-const. This should be safe
        // because the value is never modified by the calling function. If that changes, there will be a segfault during testing.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
        *errmsg = UNCONSTIFY(char *, hrnLibSsh2->errMsg);
#pragma GCC diagnostic pop
        *errmsg_len = (int)(strlen(hrnLibSsh2->errMsg) + 1);
    }
    else
    {
        *errmsg = NULL;
        *errmsg_len = 0;
    }

    return hrnLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_session_handshake
***********************************************************************************************************************************/
int
libssh2_session_handshake(LIBSSH2_SESSION *session, libssh2_socket_t sock)
{
    return hrnLibSsh2ScriptRun(
        HRNLIBSSH2_SESSION_HANDSHAKE, varLstAdd(varLstNew(), varNewInt(sock)), (HrnLibSsh2 *)session)->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_session_block_directions
***********************************************************************************************************************************/
int
libssh2_session_block_directions(LIBSSH2_SESSION *session)
{
    return hrnLibSsh2ScriptRun(HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, NULL, (HrnLibSsh2 *)session)->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_hostkey_hash
***********************************************************************************************************************************/
const char *
libssh2_hostkey_hash(LIBSSH2_SESSION *session, int hash_type)
{
    HrnLibSsh2 *hrnLibSsh2 = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        hrnLibSsh2 = hrnLibSsh2ScriptRun(
            HRNLIBSSH2_HOSTKEY_HASH, varLstAdd(varLstNew(), varNewInt(hash_type)), (HrnLibSsh2 *)session);
    }
    MEM_CONTEXT_TEMP_END();

    return hrnLibSsh2->resultNull ? NULL : (const char *)hrnLibSsh2->resultZ;
}

/***********************************************************************************************************************************
Shim for libssh2_userauth_publickey_fromfile_ex
***********************************************************************************************************************************/
int
libssh2_userauth_publickey_fromfile_ex(
    LIBSSH2_SESSION *session, const char *username, unsigned int ousername_len, const char *publickey, const char *privatekey,
    const char *passphrase)
{
    HrnLibSsh2 *hrnLibSsh2 = NULL;

    if (privatekey == NULL)
    {
        snprintf(
            hrnLibSsh2ScriptError, sizeof(hrnLibSsh2ScriptError),
            "libssh2 script function 'libssh2_userauth_publickey_fromfile_ex', expects privatekey to be not NULL");
        THROW(AssertError, hrnLibSsh2ScriptError);
    }

    MEM_CONTEXT_TEMP_BEGIN()
    {
        hrnLibSsh2 = hrnLibSsh2ScriptRun(
            HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
            varLstAdd(
                varLstAdd(
                    varLstAdd(
                        varLstAdd(
                            varLstAdd(
                                varLstNew(), varNewStrZ(username)),
                            varNewUInt(ousername_len)),
                        varNewStrZ(publickey)),
                    varNewStrZ(privatekey)),
                varNewStrZ(passphrase)),
            (HrnLibSsh2 *)session);
    }
    MEM_CONTEXT_TEMP_END();

    return hrnLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_init
***********************************************************************************************************************************/
LIBSSH2_SFTP *
libssh2_sftp_init(LIBSSH2_SESSION *session)
{
    HrnLibSsh2 *hrnLibSsh2 = hrnLibSsh2ScriptRun(HRNLIBSSH2_SFTP_INIT, NULL, (HrnLibSsh2 *)session);

    return hrnLibSsh2->resultNull ? NULL : (LIBSSH2_SFTP *)hrnLibSsh2;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_close_handle
***********************************************************************************************************************************/
int
libssh2_sftp_close_handle(LIBSSH2_SFTP_HANDLE *handle)
{
    return hrnLibSsh2ScriptRun(HRNLIBSSH2_SFTP_CLOSE_HANDLE, NULL, (HrnLibSsh2 *)handle)->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_shutdown
***********************************************************************************************************************************/
int
libssh2_sftp_shutdown(LIBSSH2_SFTP *sftp)
{
    return hrnLibSsh2ScriptRun(HRNLIBSSH2_SFTP_SHUTDOWN, NULL, (HrnLibSsh2 *)sftp)->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_session_disconnect_ex
***********************************************************************************************************************************/
int
libssh2_session_disconnect_ex(LIBSSH2_SESSION *session, int reason, const char *description, const char *lang)
{
    HrnLibSsh2 *hrnLibSsh2 = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        hrnLibSsh2 = hrnLibSsh2ScriptRun(
            HRNLIBSSH2_SESSION_DISCONNECT_EX,
            varLstAdd(
                varLstAdd(
                    varLstAdd(
                        varLstNew(), varNewInt(reason)),
                    varNewStrZ(description)),
                varNewStrZ(lang)),
            (HrnLibSsh2 *)session);
    }
    MEM_CONTEXT_TEMP_END();

    return hrnLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for int libssh2_session_free
***********************************************************************************************************************************/
int
libssh2_session_free(LIBSSH2_SESSION *session)
{
    return hrnLibSsh2ScriptRun(HRNLIBSSH2_SESSION_FREE, NULL, (HrnLibSsh2 *)session)->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_stat_ex
***********************************************************************************************************************************/
int
libssh2_sftp_stat_ex(
    LIBSSH2_SFTP *sftp, const char *path, unsigned int path_len, int stat_type, LIBSSH2_SFTP_ATTRIBUTES *attrs)
{
    // Avoid compiler complaining of unused param. Not passing to hrnLibSsh2ScriptRun, as parameter will vary depending on where
    // tests are being run.
    //
    // Could we utilize test.c/build.c to calculate/define this and other length params?
    (void)path_len;

    HrnLibSsh2 *hrnLibSsh2 = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        hrnLibSsh2 = hrnLibSsh2ScriptRun(
            HRNLIBSSH2_SFTP_STAT_EX,
            varLstAdd(
                varLstAdd(
                    varLstNew(), varNewStrZ(path)),
                varNewInt(stat_type)),
            (HrnLibSsh2 *)sftp);
    }
    MEM_CONTEXT_TEMP_END();

    if (attrs == NULL)
        THROW(AssertError, "attrs is NULL");

    attrs->flags = 0;
    attrs->flags |= (unsigned long)hrnLibSsh2->flags;

    attrs->permissions = 0;
    attrs->permissions |= (unsigned long)hrnLibSsh2->attrPerms;

    attrs->mtime = (unsigned long)hrnLibSsh2->mtime;
    attrs->uid = (unsigned long)hrnLibSsh2->uid;
    attrs->gid = (unsigned long)hrnLibSsh2->gid;
    attrs->filesize = hrnLibSsh2->filesize;

    return hrnLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_last_error
***********************************************************************************************************************************/
unsigned long
libssh2_sftp_last_error(LIBSSH2_SFTP *sftp)
{
    return (unsigned long)hrnLibSsh2ScriptRun(HRNLIBSSH2_SFTP_LAST_ERROR, NULL, (HrnLibSsh2 *)sftp)->resultUInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_symlink_ex
***********************************************************************************************************************************/
int
libssh2_sftp_symlink_ex(
    LIBSSH2_SFTP *sftp, const char *path, unsigned int path_len, char *target, unsigned int target_len, int link_type)
{
    // Avoid compiler complaining of unused param. Not passing to hrnLibSsh2ScriptRun, as parameter will vary depending on where
    // tests are being run.
    (void)path_len;
    (void)target_len;

    HrnLibSsh2 *hrnLibSsh2 = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        hrnLibSsh2 = hrnLibSsh2ScriptRun(
            HRNLIBSSH2_SFTP_SYMLINK_EX,
            varLstAdd(
                varLstAdd(
                    varLstAdd(
                        varLstNew(), varNewStrZ(path)),
                    varNewStrZ(target)),
                varNewInt(link_type)),
            (HrnLibSsh2 *)sftp);
    }
    MEM_CONTEXT_TEMP_END();

    int rc;

    switch (link_type)
    {
        case LIBSSH2_SFTP_READLINK:
        case LIBSSH2_SFTP_REALPATH:
            if (hrnLibSsh2->symlinkExTarget != NULL)
            {
                if (strSize(hrnLibSsh2->symlinkExTarget) < PATH_MAX)
                    strncpy(target, strZ(hrnLibSsh2->symlinkExTarget), strSize(hrnLibSsh2->symlinkExTarget));
                else
                    THROW_FMT(AssertError, "symlinkExTarget too large for target buffer");
            }

            rc = hrnLibSsh2->resultInt != 0 ? hrnLibSsh2->resultInt : (int)strSize(hrnLibSsh2->symlinkExTarget);
            break;

        default:
            THROW_FMT(AssertError, "UNKNOWN link_type");
            break;
    }

    return rc;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_open_ex
***********************************************************************************************************************************/
LIBSSH2_SFTP_HANDLE *
libssh2_sftp_open_ex(
    LIBSSH2_SFTP *sftp, const char *filename, unsigned int filename_len, unsigned long flags, long mode, int open_type)
{
    // To avoid compiler complaining of unused param. Not passing to hrnLibSsh2ScriptRun, as parameter will vary depending on where
    // tests are being run.
    (void)filename_len;

    HrnLibSsh2 *hrnLibSsh2 = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        hrnLibSsh2 = hrnLibSsh2ScriptRun(
            HRNLIBSSH2_SFTP_OPEN_EX,
            varLstAdd(
                varLstAdd(
                    varLstAdd(
                        varLstAdd(
                            varLstNew(), varNewStrZ(filename)),
                        varNewUInt64(flags)),
                    varNewInt64(mode)),
                varNewInt(open_type)),
            (HrnLibSsh2 *)sftp);
    }
    MEM_CONTEXT_TEMP_END();

    return hrnLibSsh2->resultNull ? NULL : (LIBSSH2_SFTP_HANDLE *)hrnLibSsh2;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_readdir_ex
***********************************************************************************************************************************/
int
libssh2_sftp_readdir_ex(
    LIBSSH2_SFTP_HANDLE *handle, char *buffer, size_t buffer_maxlen, char *longentry, size_t longentry_maxlen,
    LIBSSH2_SFTP_ATTRIBUTES *attrs)
{
    if (attrs == NULL)
        THROW_FMT(AssertError, "attrs is NULL");

    HrnLibSsh2 *hrnLibSsh2 = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        hrnLibSsh2 = hrnLibSsh2ScriptRun(
            HRNLIBSSH2_SFTP_READDIR_EX,
            varLstAdd(
                varLstAdd(
                    varLstAdd(
                        varLstAdd(
                            varLstNew(), varNewStrZ(buffer)),
                        varNewUInt64(buffer_maxlen)),
                    varNewStrZ(longentry)),
                varNewUInt64(longentry_maxlen)),
            (HrnLibSsh2 *)handle);
    }
    MEM_CONTEXT_TEMP_END();

    if (hrnLibSsh2->fileName != NULL)
        strncpy(buffer, strZ(hrnLibSsh2->fileName), buffer_maxlen);

    return hrnLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_session_last_errno
***********************************************************************************************************************************/
int
libssh2_session_last_errno(LIBSSH2_SESSION *session)
{
    return hrnLibSsh2ScriptRun(HRNLIBSSH2_SESSION_LAST_ERRNO, NULL, (HrnLibSsh2 *)session)->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_fsync
***********************************************************************************************************************************/
int
libssh2_sftp_fsync(LIBSSH2_SFTP_HANDLE *handle)
{
    return hrnLibSsh2ScriptRun(HRNLIBSSH2_SFTP_FSYNC, NULL, (HrnLibSsh2 *)handle)->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_mkdir_ex
***********************************************************************************************************************************/
int
libssh2_sftp_mkdir_ex(LIBSSH2_SFTP *sftp, const char *path, unsigned int path_len, long mode)
{
    // To avoid compiler complaining of unused param. Not passing to hrnLibSsh2ScriptRun, as parameter will vary depending on where
    // tests are being run.
    (void)path_len;

    HrnLibSsh2 *hrnLibSsh2 = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        hrnLibSsh2 = hrnLibSsh2ScriptRun(
            HRNLIBSSH2_SFTP_MKDIR_EX,
            varLstAdd(
                varLstAdd(
                    varLstNew(), varNewStrZ(path)),
                varNewInt64(mode)),
            (HrnLibSsh2 *)sftp);
    }
    MEM_CONTEXT_TEMP_END();

    return hrnLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_read
***********************************************************************************************************************************/
ssize_t
libssh2_sftp_read(LIBSSH2_SFTP_HANDLE *handle, char *buffer, size_t buffer_maxlen)
{
    // We don't pass buffer to hrnLibSsh2ScriptRun. The first call for each invocation passes buffer with random data, which is
    // an issue for sftpTest.c.
    HrnLibSsh2 *hrnLibSsh2 = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        hrnLibSsh2 = hrnLibSsh2ScriptRun(
            HRNLIBSSH2_SFTP_READ,
            varLstAdd(
                varLstNew(), varNewUInt64(buffer_maxlen)),
            (HrnLibSsh2 *)handle);
    }
    MEM_CONTEXT_TEMP_END();

    // copy read into buffer
    if (hrnLibSsh2->readBuffer != NULL)
        strncpy(buffer, strZ(hrnLibSsh2->readBuffer), strSize(hrnLibSsh2->readBuffer));

    // number of bytes populated
    return hrnLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_rename_ex
***********************************************************************************************************************************/
int
libssh2_sftp_rename_ex(
    LIBSSH2_SFTP *sftp, const char *source_filename, unsigned int source_filename_len, const char *dest_filename,
    unsigned int dest_filename_len, long flags)
{
    // To avoid compiler complaining of unused param. Not passing to hrnLibSsh2ScriptRun, as parameter will vary depending on where
    // tests are being run.
    (void)source_filename_len;
    (void)dest_filename_len;

    HrnLibSsh2 *hrnLibSsh2 = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        hrnLibSsh2 = hrnLibSsh2ScriptRun(
            HRNLIBSSH2_SFTP_RENAME_EX,
            varLstAdd(
                varLstAdd(
                    varLstAdd(
                        varLstNew(), varNewStrZ(source_filename)),
                    varNewStrZ(dest_filename)),
                varNewInt64(flags)),
            (HrnLibSsh2 *)sftp);
    }
    MEM_CONTEXT_TEMP_END();

    return hrnLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_rmdir_ex
***********************************************************************************************************************************/
int
libssh2_sftp_rmdir_ex(LIBSSH2_SFTP *sftp, const char *path, unsigned int path_len)
{
    // Avoid compiler complaining of unused param. Not passing to hrnLibSsh2ScriptRun, as parameter will vary depending on where
    // tests are being run.
    (void)path_len;

    HrnLibSsh2 *hrnLibSsh2 = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        hrnLibSsh2 = hrnLibSsh2ScriptRun(
            HRNLIBSSH2_SFTP_RMDIR_EX,
            varLstAdd(
                varLstNew(), varNewStrZ(path)),
            (HrnLibSsh2 *)sftp);
    }
    MEM_CONTEXT_TEMP_END();

    return hrnLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_seek64
***********************************************************************************************************************************/
void
libssh2_sftp_seek64(LIBSSH2_SFTP_HANDLE *handle, libssh2_uint64_t offset)
{
    MEM_CONTEXT_TEMP_BEGIN()
    {
        hrnLibSsh2ScriptRun(
            HRNLIBSSH2_SFTP_SEEK64,
            varLstAdd(
                varLstNew(), varNewUInt64(offset)),
            (HrnLibSsh2 *)handle);
    }
    MEM_CONTEXT_TEMP_END();

    return;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_unlink_ex
***********************************************************************************************************************************/
int
libssh2_sftp_unlink_ex(LIBSSH2_SFTP *sftp, const char *filename, unsigned int filename_len)
{
    // Avoid compiler complaining of unused param. Not passing to hrnLibSsh2ScriptRun, as parameter will vary depending on where
    // tests are being run.
    (void)filename_len;

    HrnLibSsh2 *hrnLibSsh2 = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        hrnLibSsh2 = hrnLibSsh2ScriptRun(
            HRNLIBSSH2_SFTP_UNLINK_EX,
            varLstAdd(
                varLstNew(), varNewStrZ(filename)),
            (HrnLibSsh2 *)sftp);
    }
    MEM_CONTEXT_TEMP_END();

    return hrnLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_write
***********************************************************************************************************************************/
ssize_t
libssh2_sftp_write(LIBSSH2_SFTP_HANDLE *handle, const char *buffer, size_t count)
{
    // We don't pass buffer to hrnLibSsh2ScriptRun. The first call for each invocation passes buffer with random data, which is
    // an issue for sftpTest.c.
    (void)buffer;

    HrnLibSsh2 *hrnLibSsh2 = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        hrnLibSsh2 = hrnLibSsh2ScriptRun(
            HRNLIBSSH2_SFTP_WRITE,
            varLstAdd(
                varLstNew(), varNewUInt64(count)),
            (HrnLibSsh2 *)handle);
    }
    MEM_CONTEXT_TEMP_END();

    // Return number of bytes written
    return hrnLibSsh2->resultInt;
}

#endif // HAVE_LIBSSH2
