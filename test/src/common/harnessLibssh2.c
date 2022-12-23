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

#include "common/harnessLibssh2.h"
#include "common/harnessTest.h"

/***********************************************************************************************************************************
libssh2 shim error prefix
***********************************************************************************************************************************/
#define LIBSSH2_ERROR_PREFIX                                        "LIBSSH2 SHIM ERROR"

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
                        varLstAdd(
                            varLstNew(), varNewStr(NULL)),
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
                        varLstAdd(
                            varLstNew(), varNewStrZ(username)),
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
    return harnessLibssh2ScriptRun(HRNLIBSSH2_SFTP_SHUTDOWN, NULL, (HarnessLibssh2 *)sftp)->resultInt;
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
    return harnessLibssh2ScriptRun(HRNLIBSSH2_SESSION_FREE, NULL, (HarnessLibssh2 *)session)->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_stat_ex
***********************************************************************************************************************************/
int libssh2_sftp_stat_ex(
    LIBSSH2_SFTP *sftp, const char *path, unsigned int path_len, int stat_type, LIBSSH2_SFTP_ATTRIBUTES *attrs)
{
    // To avoid compiler complaining of unused param.
    // Not passing to harnessLibssh2ScriptRun as parameter will vary depending on where tests are being run.
    // Could we utilize test.c/build.c to calculate/define this and other length params?
    if (path_len)
    {
        // do nothing
    }

    HarnessLibssh2 *harnessLibssh2 = harnessLibssh2ScriptRun(
        HRNLIBSSH2_SFTP_STAT_EX,
        varLstAdd(
            varLstAdd(
                varLstNew(), varNewStrZ(path)),
        varNewInt(stat_type)),
        (HarnessLibssh2 *)sftp);

    if (attrs == NULL)
        THROW(AssertError, "attrs is NULL");

    attrs->flags = 0;
    attrs->flags |= harnessLibssh2->flags;

    attrs->permissions = 0;
    attrs->permissions |= harnessLibssh2->attrPerms;

    attrs->mtime = harnessLibssh2->mtime;
    attrs->uid = harnessLibssh2->uid;
    attrs->gid = harnessLibssh2->gid;
    attrs->filesize = harnessLibssh2->filesize;

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
    // Not passing to harnessLibssh2ScriptRun as parameter will vary depending on where tests are being run.
    if (path_len)
    {
        // do nothing
    }

    if (target_len)
    {
        // do nothing
    }

    HarnessLibssh2 *harnessLibssh2 = harnessLibssh2ScriptRun(
        HRNLIBSSH2_SFTP_SYMLINK_EX,
        varLstAdd(
            varLstAdd(
                varLstAdd(
                    varLstNew(), varNewStrZ(path)),
            varNewStrZ(target)),
        varNewInt(link_type)),
        (HarnessLibssh2 *)sftp);

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
            if (harnessLibssh2->symlinkExTarget != NULL)
            {
                if (strSize(harnessLibssh2->symlinkExTarget) < PATH_MAX)
                    strncpy(target, strZ(harnessLibssh2->symlinkExTarget), strSize(harnessLibssh2->symlinkExTarget));
                else
                    THROW_FMT(AssertError, "harnessLibssh2.c %s ERROR: symlinkExTarget too large for target buffer", __FUNCTION__);
            }

            rc = harnessLibssh2->resultInt != 0 ? harnessLibssh2->resultInt : (int)strSize(harnessLibssh2->symlinkExTarget);
            break;

        case LIBSSH2_SFTP_SYMLINK:
            if (harnessLibssh2->symlinkExTarget != NULL)
            {
                if (strSize(harnessLibssh2->symlinkExTarget) < PATH_MAX)
                    strncpy(target, strZ(harnessLibssh2->symlinkExTarget), strSize(harnessLibssh2->symlinkExTarget));
                else
                    THROW_FMT(AssertError, "harnessLibssh2.c %s ERROR: symlinkExTarget too large for target buffer", __FUNCTION__);
            }

            rc = harnessLibssh2->resultInt;
            break;

        default:
            THROW_FMT(AssertError, "harnessLibssh2.c %s ERROR: UNKNOWN link_type", __FUNCTION__);
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
    // Not passing to harnessLibssh2ScriptRun as parameter will vary depending on where tests are being run.
    if (filename_len)
    {
        // do nothing
    }

    HarnessLibssh2 *harnessLibssh2 = harnessLibssh2ScriptRun(
        HRNLIBSSH2_SFTP_OPEN_EX,
        varLstAdd(
            varLstAdd(
                varLstAdd(
                    varLstAdd(
                        varLstNew(), varNewStrZ(filename)),
                varNewUInt64(flags)),
            varNewInt64(mode)),
        varNewInt(open_type)),
        (HarnessLibssh2 *)sftp);

    return harnessLibssh2->resultNull ? NULL : (LIBSSH2_SFTP_HANDLE *)harnessLibssh2;
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

    HarnessLibssh2 *harnessLibssh2 = harnessLibssh2ScriptRun(
        HRNLIBSSH2_SFTP_READDIR_EX,
        varLstAdd(
            varLstAdd(
                varLstAdd(
                    varLstAdd(
                        varLstNew(), varNewStrZ(buffer)),
                varNewUInt64(buffer_maxlen)),
            varNewStrZ(longentry)),
        varNewUInt64(longentry_maxlen)),
        (HarnessLibssh2 *)handle);

    if (harnessLibssh2->fileName != NULL)
        strncpy(buffer, strZ(harnessLibssh2->fileName), buffer_maxlen);

    return harnessLibssh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_session_last_errno
***********************************************************************************************************************************/
int libssh2_session_last_errno(LIBSSH2_SESSION *session)
{
    return harnessLibssh2ScriptRun(
            HRNLIBSSH2_SESSION_LAST_ERRNO, NULL, (HarnessLibssh2 *)session)->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_fstat_ex
***********************************************************************************************************************************/
int libssh2_sftp_fstat_ex(LIBSSH2_SFTP_HANDLE *handle, LIBSSH2_SFTP_ATTRIBUTES *attrs, int setstat)
{
    if (attrs == NULL)
        THROW_FMT(AssertError, "%s attrs is NULL", __FUNCTION__);

    HarnessLibssh2 *harnessLibssh2 = harnessLibssh2ScriptRun(
        HRNLIBSSH2_SFTP_FSTAT_EX, varLstAdd(varLstNew(), varNewInt(setstat)), (HarnessLibssh2 *)handle);

    // populate attrs
    attrs->flags = 0;
    attrs->flags |= harnessLibssh2->flags;

    attrs->permissions = 0;
    attrs->permissions |= harnessLibssh2->attrPerms;

    attrs->mtime = harnessLibssh2->mtime;
    attrs->uid = harnessLibssh2->uid;
    attrs->gid = harnessLibssh2->gid;
    attrs->filesize = harnessLibssh2->filesize;

    return harnessLibssh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_fsync
***********************************************************************************************************************************/
int libssh2_sftp_fsync(LIBSSH2_SFTP_HANDLE *handle)
{
    return harnessLibssh2ScriptRun(HRNLIBSSH2_SFTP_FSYNC, NULL, (HarnessLibssh2 *)handle)->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_mkdir_ex
***********************************************************************************************************************************/
int libssh2_sftp_mkdir_ex(LIBSSH2_SFTP *sftp, const char *path, unsigned int path_len, long mode)
{
    // To avoid compiler complaining of unused param.
    // Not passing to harnessLibssh2ScriptRun as parameter will vary depending on where tests are being run.
    if (path_len)
    {
        // do nothing
    }

    HarnessLibssh2 *harnessLibssh2 = harnessLibssh2ScriptRun(
        HRNLIBSSH2_SFTP_MKDIR_EX,
        varLstAdd(
            varLstAdd(
                varLstNew(), varNewStrZ(path)),
        varNewInt64(mode)),
        (HarnessLibssh2 *)sftp);

    return harnessLibssh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_read
***********************************************************************************************************************************/
ssize_t libssh2_sftp_read(LIBSSH2_SFTP_HANDLE *handle, char *buffer, size_t buffer_maxlen)
{
    // We don't pass buffer to harnessLibssh2ScriptRun. The first call for each invocation passes buffer with random data, which is
    // an issue for sftpTest.c.
    HarnessLibssh2 *harnessLibssh2 = harnessLibssh2ScriptRun(
        HRNLIBSSH2_SFTP_READ,
        varLstAdd(
            varLstNew(), varNewUInt64(buffer_maxlen)),
        (HarnessLibssh2 *)handle);

    // copy read into buffer
    if (harnessLibssh2->readBuffer != NULL)
        strncpy(buffer, strZ(harnessLibssh2->readBuffer), strSize(harnessLibssh2->readBuffer));

    // number of bytes populated
    return harnessLibssh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_rename_ex
***********************************************************************************************************************************/
int libssh2_sftp_rename_ex(
    LIBSSH2_SFTP *sftp, const char *source_filename, unsigned int source_filename_len, const char *dest_filename,
    unsigned int dest_filename_len, long flags)
{
    // To avoid compiler complaining of unused param.
    // Not passing to harnessLibssh2ScriptRun as parameter will vary depending on where tests are being run.
    if (source_filename_len)
    {
        // do nothing
    }

    if (dest_filename_len)
    {
        // do nothing
    }

    HarnessLibssh2 *harnessLibssh2 = harnessLibssh2ScriptRun(
        HRNLIBSSH2_SFTP_RENAME_EX,
        varLstAdd(
            varLstAdd(
                varLstAdd(
                    varLstNew(), varNewStrZ(source_filename)),
            varNewStrZ(dest_filename)),
        varNewInt64(flags)),
        (HarnessLibssh2 *)sftp);

    return harnessLibssh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_rmdir_ex
***********************************************************************************************************************************/
int libssh2_sftp_rmdir_ex(LIBSSH2_SFTP *sftp, const char *path, unsigned int path_len)
{
    // To avoid compiler complaining of unused param.
    // Not passing to harnessLibssh2ScriptRun as parameter will vary depending on where tests are being run.
    if (path_len)
    {
        // do nothing
    }

    HarnessLibssh2 *harnessLibssh2 = harnessLibssh2ScriptRun(
        HRNLIBSSH2_SFTP_RMDIR_EX,
        varLstAdd(
            varLstNew(), varNewStrZ(path)),
        (HarnessLibssh2 *)sftp);

    return harnessLibssh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_seek64
***********************************************************************************************************************************/
void libssh2_sftp_seek64(LIBSSH2_SFTP_HANDLE *handle, libssh2_uint64_t offset)
{
    HarnessLibssh2 *harnessLibssh2 = harnessLibssh2ScriptRun(
        HRNLIBSSH2_SFTP_SEEK64,
        varLstAdd(
            varLstNew(), varNewUInt64(offset)),
        (HarnessLibssh2 *)handle);

    // to avoid compiler complaining of unused harnessLibssh2 var
    if (harnessLibssh2->offset)
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
    // Not passing to harnessLibssh2ScriptRun as parameter will vary depending on where tests are being run.
    if (filename_len)
    {
        // do nothing
    }

    HarnessLibssh2 *harnessLibssh2 = harnessLibssh2ScriptRun(
        HRNLIBSSH2_SFTP_UNLINK_EX,
        varLstAdd(
            varLstNew(), varNewStrZ(filename)),
        (HarnessLibssh2 *)sftp);

    return harnessLibssh2->resultInt;
}

/***********************************************************************************************************************************
Shim for libssh2_sftp_write
***********************************************************************************************************************************/
ssize_t libssh2_sftp_write(LIBSSH2_SFTP_HANDLE *handle, const char *buffer, size_t count)
{
    // We don't pass buffer to harnessLibssh2ScriptRun. The first call for each invocation passes buffer with random data, which is
    // an issue for sftpTest.c.
    HarnessLibssh2 *harnessLibssh2 = harnessLibssh2ScriptRun(
        HRNLIBSSH2_SFTP_WRITE,
        varLstAdd(
            varLstNew(), varNewUInt64(count)),
        (HarnessLibssh2 *)handle);

    if (buffer)
    {
        // do nothing
    }

    // return number of bytes written
    return harnessLibssh2->resultInt;
}

#endif // HAVE_LIBSSH2
