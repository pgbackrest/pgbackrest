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
HarnessLibSsh2 harnessLibSsh2Script[1024];
bool harnessLibSsh2ScriptDone = true;
unsigned int harnessLibSsh2ScriptIdx;

// If there is a script failure change the behavior of cleanup functions to return immediately so the real error will be reported
// rather than a bogus scripting error during cleanup
bool harnessLibSsh2ScriptFail;
char harnessLibSsh2ScriptError[4096];

/***********************************************************************************************************************************
Set libssh2 script
***********************************************************************************************************************************/
void
harnessLibSsh2ScriptSet(HarnessLibSsh2 *harnessLibSsh2ScriptParam)
{
    if (!harnessLibSsh2ScriptDone)
        THROW(AssertError, "previous libssh2 script has not yet completed");

    if (harnessLibSsh2ScriptParam[0].function == NULL)
        THROW(AssertError, "libssh2 script must have entries");

    // Copy records into local storage
    unsigned int copyIdx = 0;

    while (harnessLibSsh2ScriptParam[copyIdx].function != NULL)
    {
        harnessLibSsh2Script[copyIdx] = harnessLibSsh2ScriptParam[copyIdx];
        copyIdx++;
    }

    harnessLibSsh2Script[copyIdx].function = NULL;
    harnessLibSsh2ScriptDone = false;
    harnessLibSsh2ScriptIdx = 0;
}

/***********************************************************************************************************************************
Run libssh2 script
***********************************************************************************************************************************/
static HarnessLibSsh2 *
harnessLibSsh2ScriptRun(const char *const function, const VariantList *const param, const HarnessLibSsh2 *const parent)
{
    // If an error has already been thrown then throw the same error again
    if (harnessLibSsh2ScriptFail)
        THROW(AssertError, harnessLibSsh2ScriptError);

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
    if (harnessLibSsh2ScriptDone)
    {
        snprintf(
            harnessLibSsh2ScriptError, sizeof(harnessLibSsh2ScriptError), "libssh2 script ended before %s (%s)", function,
            strZ(paramStr));

        TEST_LOG_FMT(LIBSSH2_ERROR_PREFIX ": %s", harnessLibSsh2ScriptError);
        harnessLibSsh2ScriptFail = true;

        THROW(AssertError, harnessLibSsh2ScriptError);
    }

    // Get current script item
    HarnessLibSsh2 *result = &harnessLibSsh2Script[harnessLibSsh2ScriptIdx];

    // Check that expected function was called
    if (strcmp(result->function, function) != 0)
    {
        snprintf(
            harnessLibSsh2ScriptError, sizeof(harnessLibSsh2ScriptError),
            "libssh2 script [%u] expected function %s (%s) but got %s (%s)", harnessLibSsh2ScriptIdx, result->function,
            result->param == NULL ? "" : result->param, function, strZ(paramStr));

        TEST_LOG_FMT(LIBSSH2_ERROR_PREFIX ": %s", harnessLibSsh2ScriptError);
        harnessLibSsh2ScriptFail = true;

        THROW(AssertError, harnessLibSsh2ScriptError);
    }

    // Check that parameters match
    if ((param != NULL && result->param == NULL) || (param == NULL && result->param != NULL) ||
        (param != NULL && result->param != NULL && !strEqZ(paramStr, result->param)))
    {
        snprintf(
            harnessLibSsh2ScriptError, sizeof(harnessLibSsh2ScriptError),
            "libssh2 script [%u] function '%s', expects param '%s' but got '%s'",
            harnessLibSsh2ScriptIdx, result->function, result->param ? result->param : "NULL", param ? strZ(paramStr) : "NULL");

        TEST_LOG_FMT(LIBSSH2_ERROR_PREFIX ": %s", harnessLibSsh2ScriptError);
        harnessLibSsh2ScriptFail = true;

        THROW(AssertError, harnessLibSsh2ScriptError);
    }

    // Make sure the session matches with the parent as a sanity check
    if (parent != NULL && result->session != parent->session)
    {
        snprintf(
            harnessLibSsh2ScriptError, sizeof(harnessLibSsh2ScriptError),
            "libssh2 script [%u] function '%s', expects session '%u' but got '%u'",
            harnessLibSsh2ScriptIdx, result->function, result->session, parent->session);

        TEST_LOG_FMT(LIBSSH2_ERROR_PREFIX ": %s", harnessLibSsh2ScriptError);
        harnessLibSsh2ScriptFail = true;

        THROW(AssertError, harnessLibSsh2ScriptError);
    }

    // Sleep if requested
    if (result->sleep > 0)
        sleepMSec(result->sleep);

    harnessLibSsh2ScriptIdx++;

    if (harnessLibSsh2Script[harnessLibSsh2ScriptIdx].function == NULL)
        harnessLibSsh2ScriptDone = true;

    strFree(paramStr);

    return result;
}

/***********************************************************************************************************************************
Shim for libssh2_init
***********************************************************************************************************************************/
int libssh2_init(int flags)
{
    HarnessLibSsh2 *harnessLibSsh2 = harnessLibSsh2ScriptRun(HRNLIBSSH2_INIT, varLstAdd(varLstNew(), varNewInt(flags)), NULL);

    return harnessLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_session_init
***********************************************************************************************************************************/
LIBSSH2_SESSION *libssh2_session_init_ex(
    LIBSSH2_ALLOC_FUNC((*myalloc)), LIBSSH2_FREE_FUNC((*myfree)), LIBSSH2_REALLOC_FUNC((*myrealloc)), void *abstract)
{
    // All of these should always be the default NULL
    if (myalloc != NULL && myfree != NULL && myrealloc != NULL && abstract != NULL)
    {
        snprintf(
            harnessLibSsh2ScriptError, sizeof(harnessLibSsh2ScriptError),
            "libssh2 script function 'libssh2_session_init_ex', expects all params to be NULL");
        THROW(AssertError, harnessLibSsh2ScriptError);
    }

    HarnessLibSsh2 *harnessLibSsh2 = harnessLibSsh2ScriptRun(
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

    return harnessLibSsh2->resultNull ? NULL : (LIBSSH2_SESSION *)harnessLibSsh2;
}

/***********************************************************************************************************************************
Shim for libssh2_session_set_blocking
***********************************************************************************************************************************/
void libssh2_session_set_blocking(LIBSSH2_SESSION* session, int blocking)
{
    if (session == NULL)
    {
        snprintf(
            harnessLibSsh2ScriptError, sizeof(harnessLibSsh2ScriptError),
            "libssh2 script function 'libssh2_session_set_blocking', expects session to be not NULL");
        THROW(AssertError, harnessLibSsh2ScriptError);
    }

    if (!(INT_MIN <= blocking && blocking <= INT_MAX))
    {
        snprintf(
            harnessLibSsh2ScriptError, sizeof(harnessLibSsh2ScriptError),
            "libssh2 script function 'libssh2_session_set_blocking', expects blocking to be an integer value");
        THROW(AssertError, harnessLibSsh2ScriptError);
    }

    return;
}

/***********************************************************************************************************************************
Shim for libssh2_session_handshake
***********************************************************************************************************************************/
int libssh2_session_handshake(LIBSSH2_SESSION *session, libssh2_socket_t sock)
{
    return harnessLibSsh2ScriptRun(
            HRNLIBSSH2_SESSION_HANDSHAKE, varLstAdd(varLstNew(), varNewInt(sock)), (HarnessLibSsh2 *)session)->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_hostkey_hash
***********************************************************************************************************************************/
const char *libssh2_hostkey_hash(LIBSSH2_SESSION *session, int hash_type)
{
    HarnessLibSsh2 *harnessLibSsh2 = harnessLibSsh2ScriptRun(
        HRNLIBSSH2_HOSTKEY_HASH, varLstAdd(varLstNew(), varNewInt(hash_type)), (HarnessLibSsh2 *)session);

    return harnessLibSsh2->resultNull ? NULL : (const char *)harnessLibSsh2->resultZ;
}

/***********************************************************************************************************************************
Shim for libssh2_userauth_publickey_fromfile_ex
***********************************************************************************************************************************/
int libssh2_userauth_publickey_fromfile_ex(
    LIBSSH2_SESSION *session, const char *username, unsigned int ousername_len, const char *publickey, const char *privatekey,
    const char *passphrase)
{
    HarnessLibSsh2 *harnessLibSsh2 = harnessLibSsh2ScriptRun(
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
        (HarnessLibSsh2 *)session);

    return harnessLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_init
***********************************************************************************************************************************/
LIBSSH2_SFTP *libssh2_sftp_init(LIBSSH2_SESSION *session)
{
    HarnessLibSsh2 *harnessLibSsh2 = harnessLibSsh2ScriptRun(HRNLIBSSH2_SFTP_INIT, NULL, (HarnessLibSsh2 *)session);

    return harnessLibSsh2->resultNull ? NULL : (LIBSSH2_SFTP *)harnessLibSsh2;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_close_handle
***********************************************************************************************************************************/
int libssh2_sftp_close_handle(LIBSSH2_SFTP_HANDLE *handle)
{
    return harnessLibSsh2ScriptRun(HRNLIBSSH2_SFTP_CLOSE_HANDLE, NULL, (HarnessLibSsh2 *)handle)->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_shutdown
***********************************************************************************************************************************/
int libssh2_sftp_shutdown(LIBSSH2_SFTP *sftp)
{
    return harnessLibSsh2ScriptRun(HRNLIBSSH2_SFTP_SHUTDOWN, NULL, (HarnessLibSsh2 *)sftp)->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_session_disconnect_ex
***********************************************************************************************************************************/
int libssh2_session_disconnect_ex(LIBSSH2_SESSION *session, int reason, const char *description, const char *lang)
{
    HarnessLibSsh2 *harnessLibSsh2 = harnessLibSsh2ScriptRun(
        HRNLIBSSH2_SESSION_DISCONNECT_EX,
        varLstAdd(
            varLstAdd(
                varLstAdd(
                    varLstNew(), varNewInt(reason)),
            varNewStrZ(description)),
        varNewStrZ(lang)),
        (HarnessLibSsh2 *)session);

    return harnessLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for int libssh2_session_free
***********************************************************************************************************************************/
int libssh2_session_free(LIBSSH2_SESSION *session)
{
    return harnessLibSsh2ScriptRun(HRNLIBSSH2_SESSION_FREE, NULL, (HarnessLibSsh2 *)session)->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_stat_ex
***********************************************************************************************************************************/
int libssh2_sftp_stat_ex(
    LIBSSH2_SFTP *sftp, const char *path, unsigned int path_len, int stat_type, LIBSSH2_SFTP_ATTRIBUTES *attrs)
{
    // To avoid compiler complaining of unused param.
    // Not passing to harnessLibSsh2ScriptRun as parameter will vary depending on where tests are being run.
    // Could we utilize test.c/build.c to calculate/define this and other length params?
    if (path_len)
    {
        // do nothing
    }

    HarnessLibSsh2 *harnessLibSsh2 = harnessLibSsh2ScriptRun(
        HRNLIBSSH2_SFTP_STAT_EX,
        varLstAdd(
            varLstAdd(
                varLstNew(), varNewStrZ(path)),
        varNewInt(stat_type)),
        (HarnessLibSsh2 *)sftp);

    if (attrs == NULL)
        THROW(AssertError, "attrs is NULL");

    attrs->flags = 0;
    attrs->flags |= harnessLibSsh2->flags;

    attrs->permissions = 0;
    attrs->permissions |= harnessLibSsh2->attrPerms;

    attrs->mtime = harnessLibSsh2->mtime;
    attrs->uid = harnessLibSsh2->uid;
    attrs->gid = harnessLibSsh2->gid;
    attrs->filesize = harnessLibSsh2->filesize;

    return harnessLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_last_error
***********************************************************************************************************************************/
unsigned long libssh2_sftp_last_error(LIBSSH2_SFTP *sftp)
{
    return harnessLibSsh2ScriptRun(HRNLIBSSH2_SFTP_LAST_ERROR, NULL, (HarnessLibSsh2 *)sftp)->resultUInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_symlink_ex
    LIBSSH2_SFTP_SYMLINK
        path is file to link to, target is symlink to create
    LIBSSH2_SFTP_READLINK
    LIBSSH2_SFTP_REALPATH
        path is file to retrive data from, target is populated with the data
***********************************************************************************************************************************/
int libssh2_sftp_symlink_ex(
    LIBSSH2_SFTP *sftp, const char *path, unsigned int path_len, char *target, unsigned int target_len, int link_type)
{
    // To avoid compiler complaining of unused param.
    // Not passing to harnessLibSsh2ScriptRun as parameter will vary depending on where tests are being run.
    if (path_len)
    {
        // do nothing
    }

    if (target_len)
    {
        // do nothing
    }

    HarnessLibSsh2 *harnessLibSsh2 = harnessLibSsh2ScriptRun(
        HRNLIBSSH2_SFTP_SYMLINK_EX,
        varLstAdd(
            varLstAdd(
                varLstAdd(
                    varLstNew(), varNewStrZ(path)),
            varNewStrZ(target)),
        varNewInt(link_type)),
        (HarnessLibSsh2 *)sftp);

    int rc = 0;
    /* from the doc page https://www.libssh2.org/libssh2_sftp_symlink_ex.html
     *
     * !!! the driver is using LIBSSH2_SFTP_READLINK to resolve linkDestination, should it use LIBSSH2_SFTP_REALPATH, see below
     *
    target - a pointer to a buffer. The buffer has different uses depending what the link_type argument is set to.
    LIBSSH2_SFTP_SYMLINK: Remote filesystem object to link to.
    LIBSSH2_SFTP_READLINK: Pre-allocated buffer to resolve symlink target into.
    LIBSSH2_SFTP_REALPATH: Pre-allocated buffer to resolve realpath target into.
    ...snip...
    libssh2_sftp_symlink(3) : Create a symbolic link between two filesystem objects.
    libssh2_sftp_readlink(3) : Resolve a symbolic link filesystem object to its next target.
    libssh2_sftp_realpath(3) : Resolve a complex, relative, or symlinked filepath to its effective target.

    */
    /*
    LIBSSH2_SFTP_SYMLINK 0
        path is file to link to, target is symlink to create
    LIBSSH2_SFTP_READLINK 1
    LIBSSH2_SFTP_REALPATH 2
        path is file to retrive data from, target is populated with the data
    */

    switch (link_type)
    {
        case LIBSSH2_SFTP_READLINK:
        case LIBSSH2_SFTP_REALPATH:
            if (harnessLibSsh2->symlinkExTarget != NULL)
            {
                if (strSize(harnessLibSsh2->symlinkExTarget) < PATH_MAX)
                    strncpy(target, strZ(harnessLibSsh2->symlinkExTarget), strSize(harnessLibSsh2->symlinkExTarget));
                else
                    THROW_FMT(AssertError, "harnessLibSsh2.c %s ERROR: symlinkExTarget too large for target buffer", __FUNCTION__);
            }

            rc = harnessLibSsh2->resultInt != 0 ? harnessLibSsh2->resultInt : (int)strSize(harnessLibSsh2->symlinkExTarget);
            break;

        case LIBSSH2_SFTP_SYMLINK:
            if (harnessLibSsh2->symlinkExTarget != NULL)
            {
                if (strSize(harnessLibSsh2->symlinkExTarget) < PATH_MAX)
                    strncpy(target, strZ(harnessLibSsh2->symlinkExTarget), strSize(harnessLibSsh2->symlinkExTarget));
                else
                    THROW_FMT(AssertError, "harnessLibSsh2.c %s ERROR: symlinkExTarget too large for target buffer", __FUNCTION__);
            }

            rc = harnessLibSsh2->resultInt;
            break;

        default:
            THROW_FMT(AssertError, "harnessLibSsh2.c %s ERROR: UNKNOWN link_type", __FUNCTION__);
            break;
    }

    return rc;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_open_ex
***********************************************************************************************************************************/
LIBSSH2_SFTP_HANDLE * libssh2_sftp_open_ex(
    LIBSSH2_SFTP *sftp, const char *filename, unsigned int filename_len, unsigned long flags, long mode, int open_type)
{
    // To avoid compiler complaining of unused param.
    // Not passing to harnessLibSsh2ScriptRun as parameter will vary depending on where tests are being run.
    if (filename_len)
    {
        // do nothing
    }

    HarnessLibSsh2 *harnessLibSsh2 = harnessLibSsh2ScriptRun(
        HRNLIBSSH2_SFTP_OPEN_EX,
        varLstAdd(
            varLstAdd(
                varLstAdd(
                    varLstAdd(
                        varLstNew(), varNewStrZ(filename)),
                varNewUInt64(flags)),
            varNewInt64(mode)),
        varNewInt(open_type)),
        (HarnessLibSsh2 *)sftp);

    return harnessLibSsh2->resultNull ? NULL : (LIBSSH2_SFTP_HANDLE *)harnessLibSsh2;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_readdir_ex
***********************************************************************************************************************************/
int libssh2_sftp_readdir_ex(
    LIBSSH2_SFTP_HANDLE *handle, char *buffer, size_t buffer_maxlen, char *longentry, size_t longentry_maxlen,
    LIBSSH2_SFTP_ATTRIBUTES *attrs)
{
    if (attrs == NULL)
        THROW_FMT(AssertError, "%s attrs is NULL", __FUNCTION__);

    HarnessLibSsh2 *harnessLibSsh2 = harnessLibSsh2ScriptRun(
        HRNLIBSSH2_SFTP_READDIR_EX,
        varLstAdd(
            varLstAdd(
                varLstAdd(
                    varLstAdd(
                        varLstNew(), varNewStrZ(buffer)),
                varNewUInt64(buffer_maxlen)),
            varNewStrZ(longentry)),
        varNewUInt64(longentry_maxlen)),
        (HarnessLibSsh2 *)handle);

    if (harnessLibSsh2->fileName != NULL)
        strncpy(buffer, strZ(harnessLibSsh2->fileName), buffer_maxlen);

    return harnessLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_session_last_errno
***********************************************************************************************************************************/
int libssh2_session_last_errno(LIBSSH2_SESSION *session)
{
    return harnessLibSsh2ScriptRun(
            HRNLIBSSH2_SESSION_LAST_ERRNO, NULL, (HarnessLibSsh2 *)session)->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_fstat_ex
***********************************************************************************************************************************/
int libssh2_sftp_fstat_ex(LIBSSH2_SFTP_HANDLE *handle, LIBSSH2_SFTP_ATTRIBUTES *attrs, int setstat)
{
    if (attrs == NULL)
        THROW_FMT(AssertError, "%s attrs is NULL", __FUNCTION__);

    HarnessLibSsh2 *harnessLibSsh2 = harnessLibSsh2ScriptRun(
        HRNLIBSSH2_SFTP_FSTAT_EX, varLstAdd(varLstNew(), varNewInt(setstat)), (HarnessLibSsh2 *)handle);

    // populate attrs
    attrs->flags = 0;
    attrs->flags |= harnessLibSsh2->flags;

    attrs->permissions = 0;
    attrs->permissions |= harnessLibSsh2->attrPerms;

    attrs->mtime = harnessLibSsh2->mtime;
    attrs->uid = harnessLibSsh2->uid;
    attrs->gid = harnessLibSsh2->gid;
    attrs->filesize = harnessLibSsh2->filesize;

    return harnessLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_fsync
***********************************************************************************************************************************/
int libssh2_sftp_fsync(LIBSSH2_SFTP_HANDLE *handle)
{
    return harnessLibSsh2ScriptRun(HRNLIBSSH2_SFTP_FSYNC, NULL, (HarnessLibSsh2 *)handle)->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_mkdir_ex
***********************************************************************************************************************************/
int libssh2_sftp_mkdir_ex(LIBSSH2_SFTP *sftp, const char *path, unsigned int path_len, long mode)
{
    // To avoid compiler complaining of unused param.
    // Not passing to harnessLibSsh2ScriptRun as parameter will vary depending on where tests are being run.
    if (path_len)
    {
        // do nothing
    }

    HarnessLibSsh2 *harnessLibSsh2 = harnessLibSsh2ScriptRun(
        HRNLIBSSH2_SFTP_MKDIR_EX,
        varLstAdd(
            varLstAdd(
                varLstNew(), varNewStrZ(path)),
        varNewInt64(mode)),
        (HarnessLibSsh2 *)sftp);

    return harnessLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_read
***********************************************************************************************************************************/
ssize_t libssh2_sftp_read(LIBSSH2_SFTP_HANDLE *handle, char *buffer, size_t buffer_maxlen)
{
    // We don't pass buffer to harnessLibSsh2ScriptRun. The first call for each invocation passes buffer with random data, which is
    // an issue for sftpTest.c.
    HarnessLibSsh2 *harnessLibSsh2 = harnessLibSsh2ScriptRun(
        HRNLIBSSH2_SFTP_READ,
        varLstAdd(
            varLstNew(), varNewUInt64(buffer_maxlen)),
        (HarnessLibSsh2 *)handle);

    // copy read into buffer
    if (harnessLibSsh2->readBuffer != NULL)
        strncpy(buffer, strZ(harnessLibSsh2->readBuffer), strSize(harnessLibSsh2->readBuffer));

    // number of bytes populated
    return harnessLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_rename_ex
***********************************************************************************************************************************/
int libssh2_sftp_rename_ex(
    LIBSSH2_SFTP *sftp, const char *source_filename, unsigned int source_filename_len, const char *dest_filename,
    unsigned int dest_filename_len, long flags)
{
    // To avoid compiler complaining of unused param.
    // Not passing to harnessLibSsh2ScriptRun as parameter will vary depending on where tests are being run.
    if (source_filename_len)
    {
        // do nothing
    }

    if (dest_filename_len)
    {
        // do nothing
    }

    HarnessLibSsh2 *harnessLibSsh2 = harnessLibSsh2ScriptRun(
        HRNLIBSSH2_SFTP_RENAME_EX,
        varLstAdd(
            varLstAdd(
                varLstAdd(
                    varLstNew(), varNewStrZ(source_filename)),
            varNewStrZ(dest_filename)),
        varNewInt64(flags)),
        (HarnessLibSsh2 *)sftp);

    return harnessLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_rmdir_ex
***********************************************************************************************************************************/
int libssh2_sftp_rmdir_ex(LIBSSH2_SFTP *sftp, const char *path, unsigned int path_len)
{
    // To avoid compiler complaining of unused param.
    // Not passing to harnessLibSsh2ScriptRun as parameter will vary depending on where tests are being run.
    if (path_len)
    {
        // do nothing
    }

    HarnessLibSsh2 *harnessLibSsh2 = harnessLibSsh2ScriptRun(
        HRNLIBSSH2_SFTP_RMDIR_EX,
        varLstAdd(
            varLstNew(), varNewStrZ(path)),
        (HarnessLibSsh2 *)sftp);

    return harnessLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_seek64
***********************************************************************************************************************************/
void libssh2_sftp_seek64(LIBSSH2_SFTP_HANDLE *handle, libssh2_uint64_t offset)
{
    HarnessLibSsh2 *harnessLibSsh2 = harnessLibSsh2ScriptRun(
        HRNLIBSSH2_SFTP_SEEK64,
        varLstAdd(
            varLstNew(), varNewUInt64(offset)),
        (HarnessLibSsh2 *)handle);

    // to avoid compiler complaining of unused harnessLibSsh2 var
    if (harnessLibSsh2->offset)
    {
        // do nothing
    }

    return;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_unlink_ex
***********************************************************************************************************************************/
int libssh2_sftp_unlink_ex(LIBSSH2_SFTP *sftp, const char *filename, unsigned int filename_len)
{
    // To avoid compiler complaining of unused param.
    // Not passing to harnessLibSsh2ScriptRun as parameter will vary depending on where tests are being run.
    if (filename_len)
    {
        // do nothing
    }

    HarnessLibSsh2 *harnessLibSsh2 = harnessLibSsh2ScriptRun(
        HRNLIBSSH2_SFTP_UNLINK_EX,
        varLstAdd(
            varLstNew(), varNewStrZ(filename)),
        (HarnessLibSsh2 *)sftp);

    return harnessLibSsh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_write
***********************************************************************************************************************************/
ssize_t libssh2_sftp_write(LIBSSH2_SFTP_HANDLE *handle, const char *buffer, size_t count)
{
    // We don't pass buffer to harnessLibSsh2ScriptRun. The first call for each invocation passes buffer with random data, which is
    // an issue for sftpTest.c.
    HarnessLibSsh2 *harnessLibSsh2 = harnessLibSsh2ScriptRun(
        HRNLIBSSH2_SFTP_WRITE,
        varLstAdd(
            varLstNew(), varNewUInt64(count)),
        (HarnessLibSsh2 *)handle);

    if (buffer)
    {
        // do nothing
    }

    // return number of bytes written
    return harnessLibSsh2->resultInt;
}

#endif // HAVE_LIBSSH2
