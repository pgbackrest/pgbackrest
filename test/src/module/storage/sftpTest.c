/***********************************************************************************************************************************
Test SFTP Storage
***********************************************************************************************************************************/
#include "common/io/io.h"
#include "common/io/session.h"
#include "common/io/socket/client.h"
#include "common/time.h"
#include "storage/cifs/storage.h"
#include "storage/read.h"
#include "storage/sftp/storage.h"
#include "storage/write.h"

#include "common/harnessConfig.h"
#include "common/harnessFd.h"
#include "common/harnessFork.h"
#include "common/harnessLibSsh2.h"
#include "common/harnessSocket.h"
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define HANDSHAKE_PARAM                                             "["STRINGIFY(HRN_SCK_FILE_DESCRIPTOR)"]"

#define SSH2_NO_BLOCK_READING_WRITING                               0
#define SSH2_BLOCK_READING                                          1
#define SSH2_BLOCK_WRITING                                          2
#define SSH2_BLOCK_READING_WRITING                                  3

/***********************************************************************************************************************************
Test function for path expression
***********************************************************************************************************************************/
#ifdef HAVE_LIBSSH2

static String *
storageTestPathExpression(const String *expression, const String *path)
{
    String *result = NULL;

    if (strcmp(strZ(expression), "<TEST>") == 0)
    {
        result = strCatZ(strNew(), "test");

        if (path != NULL)
            strCatFmt(result, "/%s", strZ(path));
    }
    else if (strcmp(strZ(expression), "<NULL>") != 0)
        THROW_FMT(AssertError, "invalid expression '%s'", strZ(expression));

    return result;
}

#endif // HAVE_LIBSSH2

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

#ifdef HAVE_LIBSSH2
    // Install shim to return defined fd
    hrnSckClientOpenShimInstall();
    // Install shim to manage true/false return for fdReady
    hrnFdReadyShimInstall();

    // Directory and file that cannot be accessed to test permissions errors
    const String *fileNoPerm = STRDEF(TEST_PATH "/noperm/noperm");
    String *pathNoPerm = strPath(fileNoPerm);

    // Write file for testing if storage is read-only
    const String *writeFile = STRDEF(TEST_PATH "/writefile");

    unsigned int repoIdx = 0;
#endif // HAVE_LIBSSH2

    // *****************************************************************************************************************************
    if (testBegin("storageNew()"))
    {
#ifdef HAVE_LIBSSH2
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("ssh2 init failure");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = -1},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(
                TEST_PATH_STR, STRDEF("localhost"), 22, TEST_USER_STR, 5, KEYPRIV, hashTypeSha1, .keyPub = KEYPUB,
                .hostKeyCheckType = SFTP_STRICT_HOSTKEY_CHECKING_STRICT),
            ServiceError, "unable to init libssh2");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("driver session failure");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]", .resultNull = true},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(
                TEST_PATH_STR, STRDEF("localhost"), 22, TEST_USER_STR, 5, KEYPRIV, hashTypeSha1, .keyPub = KEYPUB,
                .hostKeyCheckType = SFTP_STRICT_HOSTKEY_CHECKING_STRICT),
            ServiceError, "unable to init libssh2 session");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("handshake failure");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_WRITING},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_BAD_SOCKET},
            {.function = HRNLIBSSH2_SESSION_LAST_ERROR, .resultInt = LIBSSH2_ERROR_BAD_SOCKET, .errMsg = "Bad socket provided"},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(
                TEST_PATH_STR, STRDEF("localhost"), 22, TEST_USER_STR, 1000, KEYPRIV, hashTypeSha1, .keyPub = KEYPUB,
                .hostKeyCheckType = SFTP_STRICT_HOSTKEY_CHECKING_STRICT),
            ServiceError, "libssh2 handshake failed [-45]: Bad socket provided");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("handshake failure - timeout during libssh2 handshake");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(
                TEST_PATH_STR, STRDEF("localhost"), 22, TEST_USER_STR, 100, KEYPRIV, hashTypeSha1, .keyPub = KEYPUB,
                .hostKeyCheckType = SFTP_STRICT_HOSTKEY_CHECKING_STRICT),
            ServiceError, "timeout during libssh2 handshake [-37]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("hostkey hash fail");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_HOSTKEY_HASH, .param = "[2]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SOCKET_SEND},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(
                TEST_PATH_STR, STRDEF("localhost"), 22, TEST_USER_STR, 1000, KEYPRIV, hashTypeSha1, .keyPub = KEYPUB,
                .hostFingerprint = STRDEF("3132333435363738393039383736353433323130"),
                .hostKeyCheckType = SFTP_STRICT_HOSTKEY_CHECKING_FINGERPRINT),
            ServiceError, "libssh2 hostkey hash failed: libssh2 errno [-7]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid hostkey hash");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = 0},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = 0},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(
                TEST_PATH_STR, STRDEF("localhost"), 22, TEST_USER_STR, 20, KEYPRIV, cipherTypeAes256Cbc, .keyPub = KEYPUB,
                .write = true, .hostKeyCheckType = SFTP_STRICT_HOSTKEY_CHECKING_STRICT),
            ServiceError, "requested ssh2 hostkey hash type (aes-256-cbc) not available");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("public key from file auth failure leading - tilde key paths");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_HOSTKEY_HASH, .param = "[2]", .resultZ = "12345678909876543210"},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]", .resultInt = -16},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_OK},
            {.function = NULL}
        });

        // Load configuration
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, "~/.ssh/id_rsa");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, "~/.ssh/id_rsa.pub");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "fingerprint");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostFingerprint, "3132333435363738393039383736353433323130");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ERROR(
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            ServiceError,
            "public key authentication failed: libssh2 error [-16]\n"
            "HINT: libssh2 compiled against non-openssl libraries requires --repo-sftp-private-key-file and"
            " --repo-sftp-public-key-file to be provided\n"
            "HINT: libssh2 versions before 1.9.0 expect a PEM format keypair, try ssh-keygen -m PEM -t rsa -P \"\" to generate the"
            " keypair\n"
            "HINT: check authorization log on the SFTP server");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("fingerprint mismatch");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_HOSTKEY_HASH, .param = "[2]", .resultZ = "12345678909876543210"},
            {.function = NULL}
        });

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, "~/.ssh/id_rsa");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, "~/.ssh/id_rsa.pub");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "fingerprint");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostFingerprint, "9132333435363738393039383736353433323130");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ERROR(
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            ServiceError,
            "host [3132333435363738393039383736353433323130] and configured fingerprint (repo-sftp-host-fingerprint)"
            " [9132333435363738393039383736353433323130] do not match");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("known host init failure");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT, .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_ALLOC},
            {.function = HRNLIBSSH2_SESSION_LAST_ERROR, .resultInt = LIBSSH2_ERROR_ALLOC,
             .errMsg = "Unable to allocate memory for known-hosts collection"},
            {.function = NULL}
        });

        // Load configuration
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ERROR(
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            ServiceError,
            "failure during libssh2_knownhost_init: libssh2 errno [-6] Unable to allocate memory for known-hosts collection");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("libssh2_session_hostkey fail - return NULL - hostKeyCheckType = yes");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SOCKET_SEND},
            {.function = NULL}
        });

        // Load configuration
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ERROR(
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            ServiceError,
            "libssh2_session_hostkey failed to get hostkey: libssh2 error [-7]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("knownhost_checkp failure LIBSSH2_KNOWNHOST_CHECK_MISMATCH");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_MISMATCH},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            ServiceError,
            "known hosts failure: 'localhost' mismatch in known hosts files: LIBSSH2_KNOWNHOST_CHECK_MISMATCH [1]: check type "
            "[strict]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("knownhost_checkp failure LIBSSH2_KNOWNHOST_CHECK_FAILURE");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_FAILURE},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            ServiceError, "known hosts failure: 'localhost' LIBSSH2_KNOWNHOST_CHECK_FAILURE [3]: check type [strict]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("knownhost_checkp failure LIBSSH2_KNOWNHOST_CHECK_NOTFOUND");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_NOTFOUND},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            ServiceError,
            "known hosts failure: 'localhost' not found in known hosts files: LIBSSH2_KNOWNHOST_CHECK_NOTFOUND [2]: check type"
            " [strict]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read known_hosts file failure - empty files - log INFO on empty known_hosts files");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 0},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS2_FILE_CSTR "\",1]", .resultInt = 0},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" ETC_KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 0},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" ETC_KNOWNHOSTS2_FILE_CSTR "\",1]", .resultInt = 0},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_NOTFOUND},
            {.function = NULL}
        });

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ERROR(
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            ServiceError,
            "known hosts failure: 'localhost' not found in known hosts files: LIBSSH2_KNOWNHOST_CHECK_NOTFOUND [2]: check type"
            " [strict]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("knownhost_checkp unknown failure type");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = 5},
            {.function = NULL}
        });

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ERROR(
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            ServiceError, "known hosts failure: 'localhost' unknown failure [5]: check type [strict]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("public key from file auth failure, no public key");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_MATCH},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",null,\"" KEYPRIV_CSTR "\",null]", .resultInt = -16},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_OK},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            ServiceError,
            "public key authentication failed: libssh2 error [-16]\n"
            "HINT: libssh2 compiled against non-openssl libraries requires --repo-sftp-private-key-file and"
            " --repo-sftp-public-key-file to be provided\n"
            "HINT: libssh2 versions before 1.9.0 expect a PEM format keypair, try ssh-keygen -m PEM -t rsa -P \"\" to generate the"
            " keypair\n"
            "HINT: check authorization log on the SFTP server");
        TEST_RESULT_BOOL(
            unsetenv("PGBACKREST_REPO1_SFTP_PRIVATE_KEY_PASSPHRASE"), 0, "unset PGBACKREST_REPO1_SFTP_PRIVATE_KEY_PASSPHRASE");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("public key from file LIBSSH2_ERROR_EAGAIN, failure");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_MATCH},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            {.function = NULL}
        });

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ERROR(
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            ServiceError,
            "timeout during public key authentication");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init failure ssh2 error");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_MATCH},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_INIT, .resultNull = true},
            // storageSftpWaitFd returns false
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SOCKET_SEND},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SOCKET_SEND},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_ERROR_NONE},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            ServiceError,
            "unable to init libssh2_sftp session");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init fail && timeout");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_MATCH},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_INIT, .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            // fdReady shim returns false --> storageSftpWaitFd returns false
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            ServiceError,
            "timeout during init of libssh2_sftp session");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init fail no timeout");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_MATCH},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_INIT, .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            // fdReady shim returns true --> storageSftpWaitFd returns true
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING},
            {.function = HRNLIBSSH2_SFTP_INIT, .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SOCKET_SEND},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SOCKET_SEND},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_ERROR_NONE},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            ServiceError,
            "unable to init libssh2_sftp session");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init success");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        Storage *storageTest = NULL;

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            "new storage (defaults)");
        TEST_RESULT_BOOL(storageTest->write, false, "check write");

        // Free context, otherwise callbacks to storageSftpLibSsh2SessionFreeResource() accumulate
        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("knownhost_init WARN host key checking disabled, insecure connections, hostKeyCheckType = no");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_INIT},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "none");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        storageTest = NULL;

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            "new storage (defaults)");

        // Free context, otherwise callbacks to storageSftpLibSsh2SessionFreeResource() accumulate
        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("knownhost_init WARN init fail for user's known_hosts file for update, hostKeyCheckType = accept-new");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_NOTFOUND},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT, .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_ALLOC},
            {.function = HRNLIBSSH2_SESSION_LAST_ERROR, .resultInt = LIBSSH2_ERROR_ALLOC,
             .errMsg = "Unable to allocate memory for known-hosts collection"},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_INIT},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "accept-new");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        storageTest = NULL;

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            "new storage (defaults)");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: libssh2_knownhost_init failed for '/home/" TEST_USER "/.ssh/known_hosts' for update: libssh2 errno [-6] "
            "Unable to allocate memory for known-hosts collection");

        // Free context, otherwise callbacks to storageSftpLibSsh2SessionFreeResource() accumulate
        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("knownhost_init WARN missing user's known_host file for update - hostKeyCheckType = accept-new");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_NOTFOUND},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_FILE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_FILE},
            {.function = HRNLIBSSH2_SESSION_LAST_ERROR, .errMsg = "Failed to open file", .resultInt = LIBSSH2_ERROR_FILE},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_INIT},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "accept-new");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        storageTest = NULL;

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            "new storage (defaults)");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: libssh2 unable to read '/home/" TEST_USER "/.ssh/known_hosts' for update: libssh2 errno [-16] Failed "
            "to open file\n"
            "            HINT: does '/home/" TEST_USER "/.ssh/known_hosts' exist with proper permissions?");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("knownhost_init WARN unable to read user's known_host file for update - hostKeyCheckType = accept-new");

        // Create known_hosts file so it can be found by storageExistsP(), i.e. file exists but is unreadable
        HRN_SYSTEM_FMT("touch %s", strZ(strNewFmt("%s/.ssh/known_hosts", strZ(userHome()))));

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_NOTFOUND},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_FILE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_FILE},
            {.function = HRNLIBSSH2_SESSION_LAST_ERROR, .errMsg = "Failed to open file", .resultInt = LIBSSH2_ERROR_FILE},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_INIT},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = NULL;

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            "new storage (defaults)");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: libssh2 unable to read '/home/" TEST_USER "/.ssh/known_hosts' for update: libssh2 errno [-16] Failed "
            "to open file\n"
            "            HINT: does '/home/" TEST_USER "/.ssh/known_hosts' exist with proper permissions?");

        // Remove known_hosts file
        HRN_SYSTEM_FMT("rm %s", strZ(strNewFmt("%s/.ssh/known_hosts", strZ(userHome()))));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("knownhost_init WARN fail to load user's known_host file with error other than LIBSSH2_ERROR_FILE");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_NOTFOUND},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_KNOWN_HOSTS},
            {.function = HRNLIBSSH2_SESSION_LAST_ERROR, .errMsg = "Failed to parse known hosts file",
             .resultInt = LIBSSH2_ERROR_KNOWN_HOSTS},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_INIT},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = NULL;

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            "new storage (defaults)");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: libssh2 unable to read '/home/" TEST_USER "/.ssh/known_hosts' for update: libssh2 errno [-46] Failed to "
            "parse known hosts file\n"
            "            HINT: does '/home/" TEST_USER "/.ssh/known_hosts' exist with proper permissions?");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init success - hostKeyCheckType = accept-new - add host to user's known_hosts file RSA");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS2_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_FILE},
            {.function = HRNLIBSSH2_SESSION_LAST_ERROR, .errMsg = "Failed to open file", .resultInt = LIBSSH2_ERROR_FILE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" ETC_KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_FILE},
            {.function = HRNLIBSSH2_SESSION_LAST_ERROR, .errMsg = "Failed to open file", .resultInt = LIBSSH2_ERROR_FILE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" ETC_KNOWNHOSTS2_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_FILE},
            {.function = HRNLIBSSH2_SESSION_LAST_ERROR, .resultInt = LIBSSH2_ERROR_FILE},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_NOTFOUND},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_ADDC,
             .param = "[\"localhost\",null,\"12345678901234567890\",20,\"Generated from pgBackRest\",25,589825]"},
            {.function = HRNLIBSSH2_KNOWNHOST_WRITEFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_INIT},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "accept-new");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        storageTest = NULL;

        harnessLogLevelSet(logLevelDetail);
        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            "new storage (defaults)");
        TEST_RESULT_LOG(
            "P00 DETAIL: libssh2 '/home/" TEST_USER "/.ssh/known_hosts' file is empty\n"
            "P00 DETAIL: libssh2 read '/home/" TEST_USER "/.ssh/known_hosts2' failed: libssh2 errno [-16] Failed to open file\n"
            "P00 DETAIL: libssh2 read '/etc/ssh/ssh_known_hosts' failed: libssh2 errno [-16] Failed to open file\n"
            "P00 DETAIL: libssh2 read '/etc/ssh/ssh_known_hosts2' failed: libssh2 errno [-16] libssh2 no session error message "
            "provided [-16]\n"
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: pgBackRest added new host 'localhost' to '/home/" TEST_USER "/.ssh/known_hosts'");

        harnessLogLevelReset();

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init success - hostKeyCheckType = accept-new - add host to user's known_hosts file DSS");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS2_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" ETC_KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" ETC_KNOWNHOSTS2_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_DSS, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_NOTFOUND},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_ADDC,
             .param = "[\"localhost\",null,\"12345678901234567890\",20,\"Generated from pgBackRest\",25,851969]"},
            {.function = HRNLIBSSH2_KNOWNHOST_WRITEFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_INIT},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = NULL;

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            "new storage (defaults)");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: pgBackRest added new host 'localhost' to '/home/" TEST_USER "/.ssh/known_hosts'");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

#ifdef LIBSSH2_HOSTKEY_TYPE_ECDSA_256
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init success - hostKeyCheckType = accept-new - add host to user's known_hosts ECDSA_256");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS2_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" ETC_KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" ETC_KNOWNHOSTS2_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_ECDSA_256, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_NOTFOUND},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_ADDC,
             .param = "[\"localhost\",null,\"12345678901234567890\",20,\"Generated from pgBackRest\",25,1114113]"},
            {.function = HRNLIBSSH2_KNOWNHOST_WRITEFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_INIT},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = NULL;

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            "new storage (defaults)");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: pgBackRest added new host 'localhost' to '/home/" TEST_USER "/.ssh/known_hosts'");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
#endif

#ifdef LIBSSH2_HOSTKEY_TYPE_ECDSA_384
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init success - hostKeyCheckType = accept-new - add host to user's known_hosts ECDSA_384");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS2_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" ETC_KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" ETC_KNOWNHOSTS2_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_ECDSA_384, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_NOTFOUND},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_ADDC,
             .param = "[\"localhost\",null,\"12345678901234567890\",20,\"Generated from pgBackRest\",25,1376257]"},
            {.function = HRNLIBSSH2_KNOWNHOST_WRITEFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_INIT},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = NULL;

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            "new storage (defaults)");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: pgBackRest added new host 'localhost' to '/home/" TEST_USER "/.ssh/known_hosts'");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
#endif

#ifdef LIBSSH2_HOSTKEY_TYPE_ECDSA_521
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init success - hostKeyCheckType = accept-new - add host to user's known_hosts ECDSA_521");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS2_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" ETC_KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" ETC_KNOWNHOSTS2_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_ECDSA_521, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_NOTFOUND},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_ADDC,
             .param = "[\"localhost\",null,\"12345678901234567890\",20,\"Generated from pgBackRest\",25,1638401]"},
            {.function = HRNLIBSSH2_KNOWNHOST_WRITEFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_INIT},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = NULL;

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            "new storage (defaults)");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: pgBackRest added new host 'localhost' to '/home/" TEST_USER "/.ssh/known_hosts'");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
#endif

#ifdef LIBSSH2_HOSTKEY_TYPE_ED25519
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init success - hostKeyCheckType = accept-new - add host to user's known_hosts ED25519");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS2_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" ETC_KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" ETC_KNOWNHOSTS2_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_ED25519, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_NOTFOUND},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_ADDC,
             .param = "[\"localhost\",null,\"12345678901234567890\",20,\"Generated from pgBackRest\",25,1900545]"},
            {.function = HRNLIBSSH2_KNOWNHOST_WRITEFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_INIT},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = NULL;

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            "new storage (defaults)");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: pgBackRest added new host 'localhost' to '/home/" TEST_USER "/.ssh/known_hosts'");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
#endif

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init WARN - hostKeyCheckType = accept-new - fail to write user's known_hosts file");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS2_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" ETC_KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" ETC_KNOWNHOSTS2_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_NOTFOUND},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_ADDC,
             .param = "[\"localhost\",null,\"12345678901234567890\",20,\"Generated from pgBackRest\",25,589825]"},
            {.function = HRNLIBSSH2_KNOWNHOST_WRITEFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_FILE},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_INIT},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = NULL;

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            "new storage (defaults)");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: pgBackRest unable to write '/home/" TEST_USER "/.ssh/known_hosts' for update");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init WARN - hostKeyCheckType = accept-new, add to user's known_hosts - unknown key type");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS2_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" ETC_KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" ETC_KNOWNHOSTS2_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = 9, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_NOTFOUND},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_INIT},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = NULL;

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            "new storage (defaults)");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: unsupported key type [9], unable to update knownhosts for 'localhost'");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init WARN - hostKeyCheckType = accept-new - add host to user's known_hosts failure");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS2_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" ETC_KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" ETC_KNOWNHOSTS2_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_NOTFOUND},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_ADDC, .resultInt = LIBSSH2_ERROR_KNOWN_HOSTS,
             .param = "[\"localhost\",null,\"12345678901234567890\",20,\"Generated from pgBackRest\",25,589825]"},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_INIT},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = NULL;

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            "new storage (defaults)");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: pgBackRest failed to add 'localhost' to known_hosts internal list");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("knownhost_checkp failure LIBSSH2_KNOWNHOST_CHECK_FAILURE hostKeyCheckType = accept-new");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_FAILURE},
            {.function = NULL},
        });

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "accept-new");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ERROR(
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            ServiceError, "known hosts failure: 'localhost': LIBSSH2_KNOWNHOST_CHECK_FAILURE [3]: check type [accept-new]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("knownhost_checkp failure LIBSSH2_KNOWNHOST_CHECK_MISMATCH hostKeyCheckType = accept-new");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_MISMATCH},
            {.function = NULL},
        });

        TEST_ERROR(
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            ServiceError,
            "known hosts failure: 'localhost': mismatch in known hosts files: LIBSSH2_KNOWNHOST_CHECK_MISMATCH [1]: check type"
            " [accept-new]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init success LIBSSH2_KNOWNHOST_CHECK_MISMATCH hostKeyCheckType = accept-new");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_MATCH},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",null,\"" KEYPRIV_CSTR "\",null]", .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_INIT},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            "new storage (defaults)");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init WARN - hostKeyCheckType = accept-new - add host to user's known_hosts");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS2_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" ETC_KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" ETC_KNOWNHOSTS2_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_NOTFOUND},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_ADDC,
             .param = "[\"localhost\",null,\"12345678901234567890\",20,\"Generated from pgBackRest\",25,589825]"},
            {.function = HRNLIBSSH2_KNOWNHOST_WRITEFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_INIT},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "accept-new");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        storageTest = NULL;

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx),
                .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            "new storage (defaults)");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: pgBackRest added new host 'localhost' to '/home/" TEST_USER "/.ssh/known_hosts'");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session failure - libssh2_session_hostkey fail - hostKeyCheckType = accept-new");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SOCKET_SEND},
            {.function = NULL}
        });

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "accept-new");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ERROR(
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            ServiceError,
            "libssh2_session_hostkey failed to get hostkey: libssh2 error [-7]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init success timeout");

        // Shim sets FD for tests.
        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_MATCH},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_INIT, .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "accept-new");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            "new storage (defaults)");
        TEST_RESULT_BOOL(storageTest->write, false, "check write");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create new storage with defaults && a single known_hosts file with leading tilde");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"/home/" TEST_USER "/.ssh/pgbackrest_known_hosts\",1]",
             .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_MATCH},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_INIT, .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, "/tmp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, "~/.ssh/pgbackrest_known_hosts");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "strict");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            "new storage (defaults)");
        TEST_RESULT_STR_Z(storageTest->path, "/tmp", "check path");
        TEST_RESULT_INT(storageTest->modeFile, 0640, "check file mode");
        TEST_RESULT_INT(storageTest->modePath, 0750, "check path mode");
        TEST_RESULT_BOOL(storageTest->write, false, "check write");
        TEST_RESULT_STR_Z(
            strLstGet(strLstNewVarLst(cfgOptionLst(cfgOptRepoSftpKnownHost)), 0), "~/.ssh/pgbackrest_known_hosts",
            "check known hosts path");
        TEST_RESULT_BOOL(storageTest->pathExpressionFunction == NULL, true, "check expression function is not set");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create new storage - override defaults");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, "/path/to");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, "~/.ssh/known_hosts");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "accept-new");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = 0600,
                .modePath = 0700, .write = true, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            "new storage (non-default)");
        TEST_RESULT_STR_Z(storageTest->path, "/path/to", "check path");
        TEST_RESULT_INT(storageTest->modeFile, 0600, "check file mode");
        TEST_RESULT_INT(storageTest->modePath, 0700, "check path mode");
        TEST_RESULT_BOOL(storageTest->write, true, "check write");
        TEST_RESULT_BOOL(storageTest->pathExpressionFunction == NULL, true, "check expression function is empty");

        TEST_RESULT_PTR(storageInterface(storageTest).info, storageTest->pub.interface.info, "check interface");
        TEST_RESULT_PTR(storageDriver(storageTest), storageTest->pub.driver, "check driver");
        TEST_RESULT_UINT(storageType(storageTest), storageTest->pub.type, "check type");
        TEST_RESULT_BOOL(storageFeature(storageTest, storageFeaturePath), true, "check path feature");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("public key from file auth failure, key passphrase");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_MATCH},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",\"keyPassphrase\"]",
             .resultInt = -16},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_OK},
            {.function = NULL}
        });

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgEnvKeyRawZ(cfgOptRepoSftpPrivateKeyPassphrase, 1, "keyPassphrase");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ERROR(
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            ServiceError,
            "public key authentication failed: libssh2 error [-16]\n"
            "HINT: libssh2 compiled against non-openssl libraries requires --repo-sftp-private-key-file and"
            " --repo-sftp-public-key-file to be provided\n"
            "HINT: libssh2 versions before 1.9.0 expect a PEM format keypair, try ssh-keygen -m PEM -t rsa -P \"\" to generate the"
            " keypair\n"
            "HINT: check authorization log on the SFTP server");

        TEST_RESULT_BOOL(
            unsetenv("PGBACKREST_REPO1_SFTP_PRIVATE_KEY_PASSPHRASE"), 0, "unset PGBACKREST_REPO1_SFTP_PRIVATE_KEY_PASSPHRASE");
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("storageExists() and storagePathExists()"))
    {
#ifdef HAVE_LIBSSH2
        // Mimic creation of /noperm/noperm for permission denied
        // drwx------ 2 root root 3 Dec  5 15:30 noperm
        // noperm/:
        // total 1
        // -rw------- 1 root root 0 Dec  5 15:30 noperm
        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // File missing
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/missing\",0]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/missing\",0]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE, .sleep = 100},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/missing\",0]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/missing\",0]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/missing\",0]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            // Path missing
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "\",0]",
             .attrPerms = LIBSSH2_SFTP_S_IFDIR, .resultInt = LIBSSH2_ERROR_NONE},
            // Permission denied
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/noperm/noperm\",0]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/noperm/noperm\",0]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            // File and path
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/exists\",0]",
             .attrPerms = LIBSSH2_SFTP_S_IFREG, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pathExists\",0]",
             .attrPerms = LIBSSH2_SFTP_S_IFDIR, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/exists\",0]",
             .attrPerms = LIBSSH2_SFTP_S_IFREG, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pathExists\",0]",
             .attrPerms = LIBSSH2_SFTP_S_IFDIR, .resultInt = LIBSSH2_ERROR_NONE},
            // File exists after wait
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/exists\",0]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/exists\",0]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/exists\",0]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/exists\",0]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/exists\",0]",
             .attrPerms = LIBSSH2_SFTP_S_IFREG, .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "strict");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        Storage *storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file");

        TEST_RESULT_BOOL(storageExistsP(storageTest, STRDEF("missing")), false, "file does not exist");
        TEST_RESULT_BOOL(storageExistsP(storageTest, STRDEF("missing"), .timeout = 100), false, "file does not exist");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path");

        TEST_RESULT_BOOL(storagePathExistsP(storageTest, STRDEF("missing")), false, "path does not exist");
        TEST_RESULT_BOOL(storagePathExistsP(storageTest, NULL), true, "test path exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("permission denied");

        TEST_ERROR_FMT(
            storageExistsP(storageTest, fileNoPerm),
            FileOpenError,
            STORAGE_ERROR_INFO ": libssh2 error [-31]: sftp error [3] permission denied", strZ(fileNoPerm));
        TEST_ERROR_FMT(
            storagePathExistsP(storageTest, fileNoPerm),
            FileOpenError,
            STORAGE_ERROR_INFO ": libssh2 error [-31]: sftp error [3] permission denied", strZ(fileNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file and path");

        const String *fileExists = STRDEF(TEST_PATH "/exists");
        const String *pathExists = STRDEF(TEST_PATH "/pathExists");

        TEST_RESULT_BOOL(storageExistsP(storageTest, fileExists), true, "file exists");
        TEST_RESULT_BOOL(storageExistsP(storageTest, pathExists), false, "not a file");
        TEST_RESULT_BOOL(storagePathExistsP(storageTest, fileExists), false, "not a path");
        TEST_RESULT_BOOL(storagePathExistsP(storageTest, pathExists), true, "path exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file after wait");

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                sleepMSec(250);

                // Mimic HRN_SYSTEM_FMT("touch %s", strZ(fileExists));
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                TEST_RESULT_BOOL(storageExistsP(storageTest, fileExists, .timeout = 1000), true, "file exists after wait");
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("storageInfo()"))
    {
#ifdef HAVE_LIBSSH2
        // File open error via permission denied sftp error
        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/noperm/noperm\",1]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Create storage object for testing
        Storage *storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ERROR_FMT(
            storageInfoP(storageTest, fileNoPerm),
            FileOpenError,
            STORAGE_ERROR_INFO ": libssh2 error [-31]: sftp error [3] permission denied", strZ(fileNoPerm));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // File open error via ssh error - covers storageSftpLibSsh2FxNoSuchFile where rc != LIBSSH2_ERROR_SFTP_PROTOCOL
        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/noperm/noperm\",1]", .resultInt = LIBSSH2_ERROR_INVAL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Create storage object for testing
        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ERROR_FMT(
            storageInfoP(storageTest, fileNoPerm), FileOpenError, STORAGE_ERROR_INFO ": libssh2 error [-34]", strZ(fileNoPerm));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info for / exists");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/\",1]", .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, strZ(FSLASH_STR));
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "strict");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        // Create storage object for testing
        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_RESULT_BOOL(storageInfoP(storageTest, NULL).exists, true, "info for /");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info for / does not exist with no path feature");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        Storage *storageRootNoPath = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        storageRootNoPath->pub.interface.feature ^= 1 << storageFeaturePath;

        TEST_RESULT_BOOL(storageInfoP(storageRootNoPath, NULL, .ignoreMissing = true).exists, false, "no info for /");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageRootNoPath)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("directory does not exist");

        const String *fileName = STRDEF(TEST_PATH "/fileinfo");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // File does not exist
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/fileinfo\",1]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/fileinfo\",1]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            // Info out of base path - first test libssh2_sftp_stat_ex throws error before reaching libssh2 code second test
            // libssh2_sftp_stat_stat_ex reaches libssh2 code
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/etc\",1]", .resultInt = LIBSSH2_ERROR_NONE},
            // Info - path
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // Info - path exists only
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // Info basic - path
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "\",1]",
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // Info - file
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/fileinfo\",1]",
             .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                      LIBSSH2_SFTP_ATTR_SIZE,
             .mtime = 1555155555, .uid = 99999, .gid = 99999, .filesize = 8},
            // Info - link
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/testlink\",1]",
             .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG | LIBSSH2_SFTP_S_IRWXO,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"" TEST_PATH "/testlink\",\"\",1]",
             .resultInt = LIBSSH2_ERROR_NONE, .symlinkExTarget = STRDEF("/tmp")},
            // Info - link .followLink = true
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/testlink\",0]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/testlink\",0]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG | LIBSSH2_SFTP_S_IRWXO,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .uid = 0, .gid = 0},
            // Info - link .followLink = true, timeout EAGAIN in followLink
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/testlink\",0]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            // Info - link .followLink = false, timeout EAGAIN
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/testlink\",1]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            // Info - link .followLink = false, libssh2_sftp_symlink_ex link destination size timeout
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/testlink\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG | LIBSSH2_SFTP_S_IRWXO,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"" TEST_PATH "/testlink\",\"\",1]",
             .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            // Info - link .followLink = false, libssh2_sftp_symlink_ex link destination size fail
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/testlink\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG | LIBSSH2_SFTP_S_IRWXO,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"" TEST_PATH "/testlink\",\"\",1]",
             .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"" TEST_PATH "/testlink\",\"\",1]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_LINK_LOOP},
            // Info - pipe
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/testpipe\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                          LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IWOTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "strict");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        // Create storage object for testing
        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ERROR_FMT(storageInfoP(storageTest, fileName), FileOpenError, STORAGE_ERROR_INFO_MISSING, strZ(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file does not exist");

        StorageInfo info = {0};
        TEST_ASSIGN(info, storageInfoP(storageTest, fileName, .ignoreMissing = true), "get file info (missing)");
        TEST_RESULT_BOOL(info.exists, false, "check not exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info outside of base path");

        TEST_ERROR(
            storageInfoP(storageTest, STRDEF("/etc"), .ignoreMissing = true), AssertError,
            "absolute path '/etc' is not in base path '" TEST_PATH "'");
        TEST_RESULT_BOOL(
            storageInfoP(storageTest, STRDEF("/etc"), .ignoreMissing = true, .noPathEnforce = true).exists, true,
            "path not enforced");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info - path");

        HRN_STORAGE_TIME(storageTest, TEST_PATH, 1555160000);

        TEST_ASSIGN(info, storageInfoP(storageTest, TEST_PATH_STR), "get path info");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypePath, "check type");
        TEST_RESULT_UINT(info.size, 0, "check size");
        TEST_RESULT_INT(info.mode, 0770, "check mode");
        TEST_RESULT_INT(info.timeModified, 1555160000, "check mod time");
        TEST_RESULT_STR(info.linkDestination, NULL, "no link destination");
        TEST_RESULT_UINT(info.userId, TEST_USER_ID, "check user id");
        TEST_RESULT_STR(info.user, NULL, "check user");
        TEST_RESULT_UINT(info.groupId, TEST_GROUP_ID, "check group id");
        TEST_RESULT_STR(info.group, NULL, "check group");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info - path existence only");

        TEST_ASSIGN(info, storageInfoP(storageTest, TEST_PATH_STR, .level = storageInfoLevelExists), "path exists");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_UINT(info.size, 0, "check exists");
        TEST_RESULT_INT(info.timeModified, 0, "check time");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info basic - path");

        storageTest->pub.interface.feature ^= 1 << storageFeatureInfoDetail;

        TEST_ASSIGN(info, storageInfoP(storageTest, TEST_PATH_STR), "get path info");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypePath, "check type");
        TEST_RESULT_UINT(info.size, 0, "check size");
        TEST_RESULT_INT(info.mode, 0, "check mode");
        TEST_RESULT_INT(info.timeModified, 1555160000, "check mod time");
        TEST_RESULT_STR(info.linkDestination, NULL, "no link destination");
        TEST_RESULT_UINT(info.userId, 0, "check user id");
        TEST_RESULT_STR(info.user, NULL, "check user");
        TEST_RESULT_UINT(info.groupId, 0, "check group id");
        TEST_RESULT_STR(info.group, NULL, "check group");

        storageTest->pub.interface.feature ^= 1 << storageFeatureInfoDetail;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info - file");

        // Mimic file chown'd 99999:99999, timestamp 1555155555, containing "TESTFILE"
        TEST_ASSIGN(info, storageInfoP(storageTest, fileName), "get file info");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypeFile, "check type");
        TEST_RESULT_UINT(info.size, 8, "check size");
        TEST_RESULT_INT(info.mode, 0640, "check mode");
        TEST_RESULT_INT(info.timeModified, 1555155555, "check mod time");
        TEST_RESULT_STR(info.linkDestination, NULL, "no link destination");
        TEST_RESULT_STR(info.user, NULL, "check user");
        TEST_RESULT_STR(info.group, NULL, "check group");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info - link");

        const String *linkName = STRDEF(TEST_PATH "/testlink");

        // Mimic link testlink to /tmp
        TEST_ASSIGN(info, storageInfoP(storageTest, linkName), "get link info");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypeLink, "check type");
        TEST_RESULT_UINT(info.size, 0, "check size");
        TEST_RESULT_INT(info.mode, 0777, "check mode");
        TEST_RESULT_STR_Z(info.linkDestination, "/tmp", "check link destination");
        TEST_RESULT_STR(info.user, NULL, "check user");
        TEST_RESULT_STR(info.group, NULL, "check group");

        TEST_ASSIGN(info, storageInfoP(storageTest, linkName, .followLink = true), "get info from path pointed to by link");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypePath, "check type");
        TEST_RESULT_UINT(info.size, 0, "check size");
        TEST_RESULT_INT(info.mode, 0777, "check mode");
        TEST_RESULT_STR(info.linkDestination, NULL, "check link destination");
        TEST_RESULT_STR_Z(info.user, NULL, "check user");
        TEST_RESULT_STR_Z(info.group, NULL, "check group");

        // Exercise paths/branches
        // libssh2_sftp_stat_ex timeout EAGAIN followLink true
        TEST_ERROR_FMT(
            storageInfoP(storageTest, linkName, .followLink = true), FileOpenError, "timeout opening '" TEST_PATH "/testlink'");

        // libssh2_sftp_stat_ex timeout EAGAIN followLink false
        TEST_ERROR_FMT(
            storageInfoP(storageTest, linkName, .followLink = false), FileOpenError, "timeout opening '" TEST_PATH "/testlink'");

        // libssh2_sftp_symlink_ex link destination timeout EAGAIN followLink false
        TEST_ERROR_FMT(
            storageInfoP(storageTest, linkName, .followLink = false), FileReadError,
            "timeout getting destination for link '" TEST_PATH "/testlink'");

        // libssh2_sftp_symlink_ex fail link destination followLink false
        TEST_ERROR_FMT(
            storageInfoP(storageTest, linkName, .followLink = false), FileReadError,
            "unable to get destination for link '" TEST_PATH "/testlink': libssh2 error [-31]: sftp error [21] link loop");

        // --------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info - pipe");

        const String *pipeName = STRDEF(TEST_PATH "/testpipe");

        // Mimic pipe "/testpipe" with mode 0666
        TEST_ASSIGN(info, storageInfoP(storageTest, pipeName), "get info from pipe (special file)");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypeSpecial, "check type");
        TEST_RESULT_UINT(info.size, 0, "check size");
        TEST_RESULT_INT(info.mode, 0666, "check mode");
        TEST_RESULT_STR(info.linkDestination, NULL, "check link destination");
        TEST_RESULT_STR(info.user, NULL, "check user");
        TEST_RESULT_STR(info.group, NULL, "check group");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNewItrP()"))
    {
#ifdef HAVE_LIBSSH2
        // Mimic creation of /noperm/noperm
        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // Path missing errorOnMissing true
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/BOGUS\",0,0,1]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/BOGUS\",0,0,1]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            // Ignore missing dir
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/BOGUS\",0,0,1]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            // Path no perm
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/noperm\",0,0,1]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            // Path no perm ignore missing
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/noperm\",0,0,1]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            // Helper function storageSftpListEntry()
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"pg/missing\",1]", .resultNull = true},
            // Path with only dot
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg\",0]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IWOTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/pg\",0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("."),
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IWOTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // Path with file, link, pipe, dot dotdot
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg\",0]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IWOTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/pg\",0,0,1]"},
            // readdir returns .
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("."),
             .resultInt = 3, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // readdir returns ..
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\".\",4095,null,0]", .fileName = STRDEF(".."),
             .resultInt = 8, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // readdir returns .include
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"..\",4095,null,0]", .fileName = STRDEF(".include"),
             .resultInt = 8, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = 77777, .gid = 77777},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/.include\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP |
                          LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = 77777, .gid = 77777},
            // readdir returns file
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\".include\",4095,null,0]", .fileName = STRDEF("file"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/file\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                          LIBSSH2_SFTP_S_IWGRP,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                      LIBSSH2_SFTP_ATTR_SIZE,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID, .filesize = 8},
            // readdir returns link
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"file\",4095,null,0]", .fileName = STRDEF("link"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/link\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID, .symlinkExTarget = STRDEF("../file")},
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"" TEST_PATH "/pg/link\",\"\",1]",
             .resultInt = LIBSSH2_ERROR_NONE, .symlinkExTarget = STRDEF("../file")},
            // readdir returns pipe
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"link\",4095,null,0]", .fileName = STRDEF("pipe"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/pipe\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"pipe\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/pg/.include\",0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP |
                          LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = 77777, .gid = 77777},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IWOTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/link\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID, .symlinkExTarget = STRDEF("../file")},
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"" TEST_PATH "/pg/link\",\"\",1]",
             .resultInt = LIBSSH2_ERROR_NONE, .symlinkExTarget = STRDEF("../file")},
            // Helper function - storageSftpListEntry() info doesn't exist
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"pg/missing\",1]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"pg/missing\",1]", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "strict");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        // Create storage object for testing
        Storage *storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path missing");

        TEST_ERROR_FMT(
            storageNewItrP(storageTest, STRDEF(BOGUS_STR), .errorOnMissing = true), PathMissingError,
            STORAGE_ERROR_LIST_INFO_MISSING, TEST_PATH "/BOGUS");

        TEST_RESULT_PTR(storageNewItrP(storageTest, STRDEF(BOGUS_STR), .nullOnMissing = true), NULL, "ignore missing dir");

        TEST_ERROR_FMT(
            storageNewItrP(storageTest, pathNoPerm, .errorOnMissing = true), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": libssh2 error [-31]: sftp error [3] permission denied", strZ(pathNoPerm));

        // Should still error even when ignore missing
        TEST_ERROR_FMT(
            storageNewItrP(storageTest, pathNoPerm), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": libssh2 error [-31]: sftp error [3] permission denied", strZ(pathNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("helper function - storageSftpListEntry()");

        TEST_RESULT_VOID(
            storageSftpListEntry(
                (StorageSftp *)storageDriver(storageTest), storageLstNew(storageInfoLevelBasic), STRDEF("pg"), "missing",
                storageInfoLevelBasic),
            "missing path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path with only dot");

        // Mimic creation of TEST_PATH "/pg" mode 0766

        // NOTE: if operating against an actual sftp server, a neutral umask is required to get the proper permissions.
        // Without the neutral umask, permissions were 0764.
        TEST_STORAGE_LIST(
            storageTest, "pg",
            "./ {u=" TEST_USER_ID_Z ", g=" TEST_GROUP_ID_Z ", m=0766}\n",
            .level = storageInfoLevelDetail, .includeDot = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path with file, link, pipe");

        // Mimic creation of TEST_PATH "/pg/.include" mode 0755 chown'd 77777:77777
        // Mimic creation of TEST_PATH "pg/file" mode 0660 timemodified 1656433838 containing "TESTDATA"
        // Mimic creation of TEST_PATH "/pg/link" linked to "../file"
        // Mimic creation of TEST_PATH "/pg/pipe" mode 777
        TEST_STORAGE_LIST(
            storageTest, "pg",
            "./ {u=" TEST_USER_ID_Z ", g=" TEST_GROUP_ID_Z ", m=0766}\n"
            ".include/ {u=77777, g=77777, m=0755}\n"
            "file {s=8, t=1656433838, u=" TEST_USER_ID_Z ", g=" TEST_GROUP_ID_Z ", m=0660}\n"
            "link> {d=../file, u=" TEST_USER_ID_Z ", g=" TEST_GROUP_ID_Z "}\n"
            "pipe*\n",
            .level = storageInfoLevelDetail, .includeDot = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("helper function - storageSftpListEntry() info doesn't exist");

        TEST_RESULT_VOID(
            storageSftpListEntry(
                (StorageSftp *)storageDriver(storageTest), storageLstNew(storageInfoLevelBasic), STRDEF("pg"), "missing",
                storageInfoLevelBasic),
            "missing path");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageItrMore() twice in a row");

        StorageIterator *storageItr = NULL;

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/pg\",0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("."),
             .resultInt = 3, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\".\",4095,null,0]", .fileName = STRDEF(".include"),
             .resultInt = 8, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = 77777, .gid = 77777},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/.include\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP |
                          LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = 77777, .gid = 77777},
            // readdir returns file
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\".include\",4095,null,0]", .fileName = STRDEF("file"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/file\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                          LIBSSH2_SFTP_S_IWGRP,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                      LIBSSH2_SFTP_ATTR_SIZE,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID, .filesize = 8},
            // readdir returns link
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"file\",4095,null,0]", .fileName = STRDEF("link"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/link\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID, .symlinkExTarget = STRDEF("../file")},
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"" TEST_PATH "/pg/link\",\"\",1]",
             .resultInt = LIBSSH2_ERROR_NONE, .symlinkExTarget = STRDEF("../file")},
            // readdir returns pipe
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"link\",4095,null,0]", .fileName = STRDEF("pipe"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/pipe\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"pipe\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Create storage object for testing
        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)));

        TEST_ASSIGN(storageItr, storageNewItrP(storageTest, STRDEF("pg")), "new iterator");
        TEST_RESULT_BOOL(storageItrMore(storageItr), true, "check more");
        TEST_RESULT_BOOL(storageItrMore(storageItr), true, "check more again");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path - recurse desc");

        // Mimic creation of "pg/path" mode 0700
        // Mimic creation of "pg/path/file" mode 0600 timemodified 1656434296 containing "TESTDATA"
        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg\",0]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/pg\",0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("."),
             .resultInt = 3, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\".\",4095,null,0]", .fileName = STRDEF("path"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = 77777, .gid = 77777},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/path\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP |
                          LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = 77777, .gid = 77777},
            // readdir returns file
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"path\",4095,null,0]", .fileName = STRDEF("file"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/file\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                          LIBSSH2_SFTP_S_IWGRP,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                      LIBSSH2_SFTP_ATTR_SIZE,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID, .filesize = 8},
            // readdir returns link
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"file\",4095,null,0]", .fileName = STRDEF("link"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/link\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID, .symlinkExTarget = STRDEF("../file")},
            // readdir returns pipe
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"link\",4095,null,0]", .fileName = STRDEF("pipe"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/pipe\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"pipe\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            // Recurse readdir path
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/pg/path\",0,0,1]"},
            // readdir returns file
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("file"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/path/file\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                          LIBSSH2_SFTP_S_IWGRP,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                      LIBSSH2_SFTP_ATTR_SIZE,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID, .filesize = 8},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"file\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/link\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID, .symlinkExTarget = STRDEF("../file")},
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"" TEST_PATH "/pg/link\",\"\",1]",
             .resultInt = LIBSSH2_ERROR_NONE, .symlinkExTarget = STRDEF("../file")},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Create storage object for testing
        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)));

        TEST_STORAGE_LIST(
            storageTest, "pg",
            "pipe*\n"
            "path/file {s=8, t=1656434296}\n"
            "path/\n"
            "link> {d=../file}\n"
            "file {s=8, t=1656433838}\n"
            "./\n",
            .level = storageInfoLevelBasic, .includeDot = true, .sortOrder = sortOrderDesc);

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path - recurse asc");

        // Create a path with a subpath that will always be last to make sure lists are not freed too early in the iterator
        // Mimic creation of "pg/zzz/yyy" mode 0700
        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg\",0]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/pg\",0,0,1]"},
            // readdir returns dot
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("."),
             .resultInt = 3, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // readdir returns path
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\".\",4095,null,0]", .fileName = STRDEF("path"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = 77777, .gid = 77777},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/path\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP |
                          LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = 77777, .gid = 77777},
            // readdir returns file
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"path\",4095,null,0]", .fileName = STRDEF("file"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/file\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                          LIBSSH2_SFTP_S_IWGRP,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                      LIBSSH2_SFTP_ATTR_SIZE,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID, .filesize = 8},
            // readdir returns link
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"file\",4095,null,0]", .fileName = STRDEF("link"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/link\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID, .symlinkExTarget = STRDEF("../file")},
            // readdir returns pipe
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"link\",4095,null,0]", .fileName = STRDEF("pipe"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/pipe\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // readdir returns zzz
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"pipe\",4095,null,0]", .fileName = STRDEF("zzz"),
             .resultInt = 3, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/zzz\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // readdir return empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"zzz\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            // Recurse readdir path
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/pg/path\",0,0,1]"},
            // readdir returns file
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("file"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/path/file\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                          LIBSSH2_SFTP_S_IWGRP,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                      LIBSSH2_SFTP_ATTR_SIZE,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID, .filesize = 8},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"file\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            // Recurse zzz
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/pg/zzz\",0,0,1]"},
            // readdir returns yyy
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("yyy"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/zzz/yyy\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"yyy\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            // Recurse yyy
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/pg/zzz/yyy\",0,0,1]"},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/link\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID, .symlinkExTarget = STRDEF("../file")},
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"" TEST_PATH "/pg/link\",\"\",1]",
             .resultInt = LIBSSH2_ERROR_NONE, .symlinkExTarget = STRDEF("../file")},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Create storage object for testing
        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)));

        TEST_STORAGE_LIST(
            storageTest, "pg",
            "./\n"
            "file {s=8, t=1656433838}\n"
            "link> {d=../file}\n"
            "path/\n"
            "path/file {s=8, t=1656434296}\n"
            "pipe*\n"
            "zzz/\n"
            "zzz/yyy/\n",
            .level = storageInfoLevelBasic, .includeDot = true);

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path basic info - recurse");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg\",0]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/pg\",0,0,1]"},
            // readdir returns dot
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("."),
             .resultInt = 3, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // readdir returns path
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\".\",4095,null,0]", .fileName = STRDEF("path"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = 77777, .gid = 77777},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/path\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP |
                          LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = 77777, .gid = 77777},
            // readdir returns file
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"path\",4095,null,0]", .fileName = STRDEF("file"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/file\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                          LIBSSH2_SFTP_S_IWGRP,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                      LIBSSH2_SFTP_ATTR_SIZE,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID, .filesize = 8},
            // readdir returns link
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"file\",4095,null,0]", .fileName = STRDEF("link"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/link\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID, .symlinkExTarget = STRDEF("../file")},
            // readdir returns pipe
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"link\",4095,null,0]", .fileName = STRDEF("pipe"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/pipe\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // readdir returns zzz
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"pipe\",4095,null,0]", .fileName = STRDEF("zzz"),
             .resultInt = 3, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/zzz\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // readdir return empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"zzz\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            // Recurse zzz
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/pg/zzz\",0,0,1]"},
            // readdir returns yyy
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("yyy"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/zzz/yyy\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"yyy\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            // Recurse yyy
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/pg/zzz/yyy\",0,0,1]"},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            // Recurse path
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/pg/path\",0,0,1]"},
            // readdir returns file
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("file"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/path/file\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                          LIBSSH2_SFTP_S_IWGRP,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                      LIBSSH2_SFTP_ATTR_SIZE,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID, .filesize = 8},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"file\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/link\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID, .symlinkExTarget = STRDEF("../file")},
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"" TEST_PATH "/pg/link\",\"\",1]",
             .resultInt = LIBSSH2_ERROR_NONE, .symlinkExTarget = STRDEF("../file")},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Create storage object for testing
        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)));

        storageTest->pub.interface.feature ^= 1 << storageFeatureInfoDetail;

        TEST_STORAGE_LIST(
            storageTest, "pg",
            "zzz/yyy/\n"
            "zzz/\n"
            "pipe*\n"
            "path/file {s=8, t=1656434296}\n"
            "path/\n"
            "link> {d=../file}\n"
            "file {s=8, t=1656433838}\n"
            "./\n",
            .levelForce = true, .includeDot = true, .sortOrder = sortOrderDesc);

        storageTest->pub.interface.feature ^= 1 << storageFeatureInfoDetail;

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("empty path - filter");

        // Mimic "pg/empty" mode 0700
        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg\",0]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/pg\",0,0,1]"},
            // readdir returns dot
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("."),
             .resultInt = 3, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\".\",4095,null,0]", .fileName = STRDEF("empty"),
             .resultInt = 5, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = 77777, .gid = 77777},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/empty\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP |
                          LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = 77777, .gid = 77777},
            // readdir returns path
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"empty\",4095,null,0]", .fileName = STRDEF("path"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = 77777, .gid = 77777},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/path\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP |
                          LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = 77777, .gid = 77777},
            // readdir returns file
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"path\",4095,null,0]", .fileName = STRDEF("file"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/file\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                          LIBSSH2_SFTP_S_IWGRP,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                      LIBSSH2_SFTP_ATTR_SIZE,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID, .filesize = 8},
            // readdir returns link
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"file\",4095,null,0]", .fileName = STRDEF("link"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/link\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID, .symlinkExTarget = STRDEF("../file")},
            // readdir returns pipe
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"link\",4095,null,0]", .fileName = STRDEF("pipe"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/pipe\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // readdir returns zzz
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"pipe\",4095,null,0]", .fileName = STRDEF("zzz"),
             .resultInt = 3, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/zzz\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // readdir return empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"zzz\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            // Recurse empty
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/pg/empty\",0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            // Recurse path
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/pg/path\",0,0,1]"},
            // readdir returns file
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("file"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/path/file\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                          LIBSSH2_SFTP_S_IWGRP,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                      LIBSSH2_SFTP_ATTR_SIZE,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID, .filesize = 8},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"file\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            // Recurse zzz
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/pg/zzz\",0,0,1]"},
            // readdir returns yyy
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("yyy"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/zzz/yyy\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"yyy\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            // Recurse yyy
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/pg/zzz/yyy\",0,0,1]"},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Create storage object for testing
        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)));

        TEST_STORAGE_LIST(
            storageTest, "pg",
            "empty/\n",
            .level = storageInfoLevelType, .expression = "^empty");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("filter in subpath during recursion");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg\",0]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/pg\",0,0,1]"},
            // readdir returns dot
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("."),
             .resultInt = 3, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // readdir returns path
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\".\",4095,null,0]", .fileName = STRDEF("path"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = 77777, .gid = 77777},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/path\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP |
                          LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = 77777, .gid = 77777},
            // readdir returns file
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"path\",4095,null,0]", .fileName = STRDEF("file"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/file\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                          LIBSSH2_SFTP_S_IWGRP,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                      LIBSSH2_SFTP_ATTR_SIZE,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID, .filesize = 8},
            // readdir returns link
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"file\",4095,null,0]", .fileName = STRDEF("link"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/link\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID, .symlinkExTarget = STRDEF("../file")},
            // readdir returns pipe
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"link\",4095,null,0]", .fileName = STRDEF("pipe"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/pipe\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // readdir returns zzz
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"pipe\",4095,null,0]", .fileName = STRDEF("zzz"),
             .resultInt = 3, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/zzz\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // readdir return empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"zzz\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            // Recurse path
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/pg/path\",0,0,1]"},
            // readdir returns file
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("file"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/path/file\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                          LIBSSH2_SFTP_S_IWGRP,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                      LIBSSH2_SFTP_ATTR_SIZE,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID, .filesize = 8},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"file\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            // Recurse zzz
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/pg/zzz\",0,0,1]"},
            // readdir returns yyy
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("yyy"),
             .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/pg/zzz/yyy\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IWGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"yyy\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656433838, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            // Recurse yyy
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/pg/zzz/yyy\",0,0,1]"},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Create storage object for testing
        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)));

        TEST_STORAGE_LIST(
            storageTest, "pg",
            "path/file {s=8, t=1656434296}\n",
            .level = storageInfoLevelBasic, .expression = "\\/file$");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("storageList()"))
    {
#ifdef HAVE_LIBSSH2
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path missing");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/BOGUS\",0,0,1]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/BOGUS\",0,0,1]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            // Empty list
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/BOGUS\",0,0,1]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            // Timeout on sftp_open_ex
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/BOGUS\",0,0,1]", .resultNull = true,
             .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/BOGUS\",0,0,1]", .resultNull = true,
             .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            // Error on missing, regardless of errorOnMissing setting
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/noperm\",0,0,1]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            // Ignore missing
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/noperm\",0,0,1]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Create storage object for testing
        Storage *storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)));

        TEST_ERROR_FMT(
            storageListP(storageTest, STRDEF(BOGUS_STR), .errorOnMissing = true), PathMissingError, STORAGE_ERROR_LIST_INFO_MISSING,
            TEST_PATH "/BOGUS");

        TEST_RESULT_PTR(storageListP(storageTest, STRDEF(BOGUS_STR), .nullOnMissing = true), NULL, "null for missing dir");
        TEST_RESULT_UINT(strLstSize(storageListP(storageTest, STRDEF(BOGUS_STR))), 0, "empty list for missing dir");

        // Timeout on sftp_open_ex
        TEST_ERROR_FMT(
            storageListP(storageTest, STRDEF(BOGUS_STR), .errorOnMissing = true), FileReadError,
            "timeout opening directory '" TEST_PATH "/BOGUS'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on missing, regardless of errorOnMissing setting");

        TEST_ERROR_FMT(
            storageListP(storageTest, pathNoPerm, .errorOnMissing = true), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": libssh2 error [-31]: sftp error [3] permission denied", strZ(pathNoPerm));

        // Should still error even when ignore missing
        TEST_ERROR_FMT(
            storageListP(storageTest, pathNoPerm), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": libssh2 error [-31]: sftp error [3] permission denied", strZ(pathNoPerm));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("list - path with files");

        ioBufferSizeSet(65536);

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // storagePutP(storageNewWriteP())
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/.aaa.txt.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[14]", .resultInt = 14},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX, .resultInt = LIBSSH2_ERROR_EAGAIN,
             .param = "[\"" TEST_PATH "/.aaa.txt.pgbackrest.tmp\",\"" TEST_PATH "/.aaa.txt\",7]"},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL,
             .param = "[\"" TEST_PATH "/.aaa.txt.pgbackrest.tmp\",\"" TEST_PATH "/.aaa.txt\",7]"},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"" TEST_PATH "/.aaa.txt\"]", .resultUInt = LIBSSH2_FX_OK},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX, .resultInt = LIBSSH2_ERROR_NONE,
             .param = "[\"" TEST_PATH "/.aaa.txt.pgbackrest.tmp\",\"" TEST_PATH "/.aaa.txt\",7]"},
            // strListSort(storageListP())
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "\",0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF(".aaa.txt"),
             .resultInt = 8, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\".aaa.txt\",4095,null,0]", .fileName = STRDEF("noperm"),
             .resultInt = 6, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"noperm\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            // bbb.txt
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/bbb.txt.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[7]", .resultInt = 7},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
             .param = "[\"" TEST_PATH "/bbb.txt.pgbackrest.tmp\",\"" TEST_PATH "/bbb.txt\",7]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "\",0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("bbb.txt"),
             .resultInt = 7, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"bbb.txt\",4095,null,0]", .fileName = STRDEF("noperm"),
             .resultInt = 6, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"noperm\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            // Path with only dot, readdir timeout EAGAIN
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "\",0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .resultInt = LIBSSH2_ERROR_EAGAIN, .param = "[\"\",4095,null,0]"},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .resultInt = LIBSSH2_ERROR_EAGAIN, .param = "[\"\",4095,null,0]"},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            // Fail to close path after listing timeout EAGAIN
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "\",0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("."),
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            // Fail to close path after listing
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "\",0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("."),
             .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1555160000, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_SOCKET_RECV},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Create storage object for testing
        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, STRDEF(".aaa.txt")), BUFSTRDEF("aaaaaaaaaaaaaa")), "write aaa.text");
        TEST_RESULT_STRLST_Z(strLstSort(storageListP(storageTest, NULL), sortOrderAsc), ".aaa.txt\n" "noperm\n", "dir list");

        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, STRDEF("bbb.txt")), BUFSTRDEF("bbb.txt")), "write bbb.text");
        TEST_RESULT_STRLST_Z(storageListP(storageTest, NULL, .expression = STRDEF("^bbb")), "bbb.txt\n", "dir list");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpList() readdir EAGAIN timeout");

        TEST_RESULT_VOID(storageListP(storageTest, NULL, .errorOnMissing = true), "storageSftpList readdir EAGAIN timeout");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpList() fail to close path after listing EAGAIN timeout");

        TEST_ERROR_FMT(
            storageListP(storageTest, NULL, .errorOnMissing = true), PathCloseError,
            "timeout closing path '" TEST_PATH "' after listing");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpList() fail to close path after listing");

        TEST_ERROR_FMT(
            storageListP(storageTest, NULL, .errorOnMissing = true), PathCloseError,
            "unable to close path '" TEST_PATH "' after listing: libssh2 error [-43]");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePath()"))
    {
#ifdef HAVE_LIBSSH2
        Storage *storageTest = NULL;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path - root path");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, strZ(FSLASH_STR));
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "strict");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx))),
            "new storage /");
        TEST_RESULT_STR_Z(storagePathP(storageTest, NULL), "/", "root dir");
        TEST_RESULT_STR_Z(storagePathP(storageTest, STRDEF("/")), "/", "same as root dir");
        TEST_RESULT_STR_Z(storagePathP(storageTest, STRDEF("subdir")), "/subdir", "simple subdir");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path - expressions");

        TEST_ERROR(
            storagePathP(storageTest, STRDEF("<TEST>")), AssertError, "expression '<TEST>' not valid without callback function");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, "/path/to");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "strict");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)),
                .pathExpressionFunction = storageTestPathExpression),
            "new storage /path/to");
        TEST_RESULT_STR_Z(storagePathP(storageTest, NULL), "/path/to", "root dir");
        TEST_RESULT_STR_Z(storagePathP(storageTest, STRDEF("/path/to")), "/path/to", "absolute root dir");
        TEST_RESULT_STR_Z(storagePathP(storageTest, STRDEF("is/a/subdir")), "/path/to/is/a/subdir", "subdir");

        TEST_ERROR(
            storagePathP(storageTest, STRDEF("/bogus")), AssertError, "absolute path '/bogus' is not in base path '/path/to'");
        TEST_ERROR(
            storagePathP(storageTest, STRDEF("/path/toot")), AssertError,
            "absolute path '/path/toot' is not in base path '/path/to'");

        // Path enforcement disabled for a single call
        TEST_RESULT_STR_Z(storagePathP(storageTest, STRDEF("/bogus"), .noEnforce = true), "/bogus", "path enforce disabled");

        TEST_ERROR(storagePathP(storageTest, STRDEF("<TEST")), AssertError, "end > not found in path expression '<TEST'");
        TEST_ERROR(
            storagePathP(storageTest, STRDEF("<TEST>" BOGUS_STR)), AssertError,
            "'/' should separate expression and path '<TEST>BOGUS'");

        TEST_RESULT_STR_Z(storagePathP(storageTest, STRDEF("<TEST>")), "/path/to/test", "expression");
        TEST_ERROR(strZ(storagePathP(storageTest, STRDEF("<TEST>/"))), AssertError, "path '<TEST>/' should not end in '/'");

        TEST_RESULT_STR_Z(
            storagePathP(storageTest, STRDEF("<TEST>/something")), "/path/to/test/something", "expression with path");

        TEST_ERROR(storagePathP(storageTest, STRDEF("<NULL>")), AssertError, "evaluated path '<NULL>' cannot be null");

        TEST_ERROR(storagePathP(storageTest, STRDEF("<WHATEVS>")), AssertError, "invalid expression '<WHATEVS>'");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathCreate()"))
    {
#ifdef HAVE_LIBSSH2
        Storage *storageTest = NULL;

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // Create /sub1
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub1\",488]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub1\",488]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/sub1\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                      LIBSSH2_SFTP_ATTR_SIZE,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // create /sub1 again fails mkdir_ex ssh error
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub1\",488]",
             .resultInt = LIBSSH2_ERROR_SOCKET_SEND},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_ERROR_NONE},

            // create /sub1 again no error on file already exists, file already exists
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub1\",488]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/sub1\",0]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/sub1\",0]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                      LIBSSH2_SFTP_ATTR_SIZE,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // create /sub1 again fails stat_ex, file doesn't already exist
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub1\",488]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/sub1\",0]", .resultInt = LIBSSH2_ERROR_SOCKET_SEND},
            // create /sub1 again fail timeout EAGAIN on stat_ex
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub1\",488]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/sub1\",0]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            // create /sub1 again fail with .errorOnExists, file already exists
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub1\",488]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/sub1\",0]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                      LIBSSH2_SFTP_ATTR_SIZE,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_ERROR_NONE},
            // sub2 custom mode
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub2\",511]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/sub2\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG | LIBSSH2_SFTP_S_IRWXO,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                      LIBSSH2_SFTP_ATTR_SIZE,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // sub3/sub4 .noParentCreate fail
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub3/sub4\",488]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            // sub3/sub4 success
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub3/sub4\",488]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub3\",488]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub3/sub4\",488]",
             .resultInt = LIBSSH2_ERROR_NONE},
            // subfail EAGAIN timeout
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/subfail\",488]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/subfail\",488]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true),
            "new storage /");
        TEST_RESULT_VOID(storagePathCreateP(storageTest, STRDEF("sub1")), "create sub1");
        TEST_RESULT_INT(storageInfoP(storageTest, STRDEF("sub1")).mode, 0750, "check sub1 dir mode");
        TEST_ERROR(
            storagePathCreateP(storageTest, STRDEF("sub1")), PathCreateError,
            "ssh2 error [-7] unable to create path '" TEST_PATH "/sub1': libssh2 error [-7]");
        TEST_RESULT_VOID(storagePathCreateP(storageTest, STRDEF("sub1")), "create sub1 again no .errorOnExists fail EAGAIN");
        TEST_RESULT_VOID(
            storagePathCreateP(storageTest, STRDEF("sub1")), "create sub1 again no .errorOnExists fail other than EAGAIN");
        TEST_ERROR(storagePathCreateP(storageTest, STRDEF("sub1")), PathCreateError, "timeout stat'ing path '" TEST_PATH "/sub1'");
        TEST_ERROR(
            storagePathCreateP(storageTest, STRDEF("sub1"), .errorOnExists = true), PathCreateError,
            "sftp error unable to create path '" TEST_PATH "/sub1': path already exists");

        // NOTE: if operating against an actual sftp server, a neutral umask is required to get the proper permissions.
        // Without the neutral umask, permissions were 0775.
        TEST_RESULT_VOID(storagePathCreateP(storageTest, STRDEF("sub2"), .mode = 0777), "create sub2 with custom mode");
        TEST_RESULT_INT(storageInfoP(storageTest, STRDEF("sub2")).mode, 0777, "check sub2 dir mode");

        TEST_ERROR(
            storagePathCreateP(storageTest, STRDEF("sub3/sub4"), .noParentCreate = true), PathCreateError,
            "sftp error unable to create path '" TEST_PATH "/sub3/sub4': libssh2 error [-31]: sftp error [2] no such file");
        TEST_RESULT_VOID(storagePathCreateP(storageTest, STRDEF("sub3/sub4")), "create sub3/sub4");

        // LIBSSH2_ERROR_EAGAIN timeout fail
        TEST_ERROR(
            storagePathCreateP(storageTest, STRDEF("subfail")), PathCreateError,
            "timeout creating path '" TEST_PATH "/subfail'");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path create, timeout success on stat");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // Timeout success
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/subfail\",488]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/subfail\",0]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/subfail\",0]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/subfail\",0]", .resultInt = LIBSSH2_ERROR_NONE},
            // Error other than no such file && no parent create LIBSSH2_FX_PERMISSION_DENIED}
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/subfail\",488]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            // No error on already exists
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/subfail\",488]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FILE_ALREADY_EXISTS},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/subfail\",0]", .resultInt = LIBSSH2_ERROR_NONE},
            // Error on already exists
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/subfail\",488]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FILE_ALREADY_EXISTS},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/subfail\",0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FILE_ALREADY_EXISTS},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true),
            "new storage /");
        TEST_RESULT_VOID(storagePathCreateP(storageTest, STRDEF("subfail")), "timeout success");
        TEST_ERROR(
            storagePathCreateP(storageTest, STRDEF("subfail"), .noParentCreate = true), PathCreateError,
            "sftp error unable to create path '" TEST_PATH "/subfail': libssh2 error [-31]: sftp error [3] permission denied");
        TEST_RESULT_VOID(storagePathCreateP(storageTest, STRDEF("subfail")), "do not throw error on already exists");
        TEST_ERROR(
            storagePathCreateP(storageTest, STRDEF("subfail"), .errorOnExists = true), PathCreateError,
            "sftp error unable to create path '" TEST_PATH "/subfail': path already exists");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathRemove()"))
    {
#ifdef HAVE_LIBSSH2
        Storage *storageTest = NULL;

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // Path remove missing errorOnMissing
            {.function = HRNLIBSSH2_SFTP_RMDIR_EX, .param = "[\"" TEST_PATH "/remove1\"]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_RMDIR_EX, .param = "[\"" TEST_PATH "/remove1\"]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            // Recurse - ignore missing path
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/remove1\",0,0,1]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_RMDIR_EX, .param = "[\"" TEST_PATH "/remove1\"]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            // Recurse parent/subpath permission denied
            {.function = HRNLIBSSH2_SFTP_RMDIR_EX, .param = "[\"" TEST_PATH "/remove1/remove2\"]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            // Recurse subpath permission denied
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/remove1/remove2\",0,0,1]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/remove1/remove2\",0,0,1]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            // Path remove - file in subpath, permission denied
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/remove1\",0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("remove2"),
             .resultInt = 7, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"remove2\",4095,null,0]", .fileName = STRDEF("remove.txt"),
             .resultInt = 10,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"remove.txt\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"" TEST_PATH "/remove1/remove2\"]",
             .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"" TEST_PATH "/remove1/remove2\"]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/remove1/remove2\",0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("remove.txt"),
             .resultInt = 10,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG | LIBSSH2_SFTP_S_IRWXO,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"remove.txt\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"" TEST_PATH "/remove1/remove2/remove.txt\"]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/remove1/remove2/remove.txt\",0,0,1]",
             .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_RMDIR_EX, .param = "[\"" TEST_PATH "/remove1/remove2/remove.txt\"]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_RMDIR_EX, .param = "[\"" TEST_PATH "/remove1/remove2\"]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            // Path remove - path with subpath and file removed
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/remove1\",0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("remove2"), .resultInt = 7,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG| LIBSSH2_SFTP_S_IRWXO,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"remove2\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"" TEST_PATH "/remove1/remove2\"]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/remove1/remove2\",0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("remove.txt"),
             .resultInt = 10,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG | LIBSSH2_SFTP_S_IRWXO,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"remove.txt\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"" TEST_PATH "/remove1/remove2/remove.txt\"]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RMDIR_EX, .param = "[\"" TEST_PATH "/remove1/remove2\"]",
             .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_RMDIR_EX, .param = "[\"" TEST_PATH "/remove1/remove2\"]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RMDIR_EX, .param = "[\"" TEST_PATH "/remove1\"]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_RMDIR_EX, .param = "[\"" TEST_PATH "/remove1\"]", .resultInt = LIBSSH2_ERROR_NONE},
            // Path remove - path with subpath ssh fail on libssh2_sftp_unlink_ex
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/remove1\",0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("remove2"), .resultInt = 7,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG| LIBSSH2_SFTP_S_IRWXO,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"remove2\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"" TEST_PATH "/remove1/remove2\"]",
             .resultInt = LIBSSH2_ERROR_SOCKET_SEND},
            // unlink SFTP failure other than LIBSSH2_FX_FAILURE / LIBSSH2_FX_PERMISSION_DENIED
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/remove1\",0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("remove2"), .resultInt = 7,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG| LIBSSH2_SFTP_S_IRWXO,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"remove2\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"" TEST_PATH "/remove1/remove2\"]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_CONNECTION_LOST},

            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true),
            "new storage /");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - missing");

        const String *pathRemove1 = STRDEF(TEST_PATH "/remove1");

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .errorOnMissing = true), PathRemoveError,
            STORAGE_ERROR_PATH_REMOVE_MISSING, strZ(pathRemove1));
        TEST_RESULT_VOID(storagePathRemoveP(storageTest, pathRemove1, .recurse = true), "ignore missing path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - parent/subpath permission denied");

        String *pathRemove2 = strNewFmt("%s/remove2", strZ(pathRemove1));

        // Mimic creation of pathRemove2 mode 700
        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove2), PathRemoveError,
            STORAGE_ERROR_PATH_REMOVE " sftp error [3] permission denied", strZ(pathRemove2));
        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove2, .recurse = true), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": libssh2 error [-31]: sftp error [3] permission denied", strZ(pathRemove2));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - subpath permission denied");

        // Mimic chmod 777 pathRemove1
        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove2, .recurse = true), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": libssh2 error [-31]: sftp error [3] permission denied", strZ(pathRemove2));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - file in subpath, permission denied");

        String *fileRemove = strNewFmt("%s/remove.txt", strZ(pathRemove2));

        // Mimic "sudo chmod 755 %s && sudo touch %s && sudo chmod 777 %s", strZ(pathRemove2), strZ(fileRemove), strZ(fileRemove));
        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), PathRemoveError,
            STORAGE_ERROR_PATH_REMOVE " sftp error [3] permission denied", strZ(pathRemove2));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - path with subpath and file removed");

        // Mimic chmod 777 pathRemove2
        TEST_RESULT_VOID(storagePathRemoveP(storageTest, pathRemove1, .recurse = true), "remove path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - path with subpath ssh fail on unlink");

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), PathRemoveError,
            STORAGE_ERROR_PATH_REMOVE_FILE " libssh ssh [-7]", strZ(pathRemove2));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - other than LIBSSH2_FX_FAILURE/LIBSSH2_FX_PERMISSION_DENIED");

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), PathRemoveError,
            STORAGE_ERROR_PATH_REMOVE_FILE " libssh sftp [7] connection lost", strZ(pathRemove2));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - unlink LIBSSH2_ERROR_EAGAIN timeout");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/remove1\",0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("remove2"), .resultInt = 7,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG| LIBSSH2_SFTP_S_IRWXO,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"remove2\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"" TEST_PATH "/remove1/remove2\"]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/remove1/remove2\",0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF("remove.txt"),
             .resultInt = 10,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG | LIBSSH2_SFTP_S_IRWXO,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"remove.txt\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                          LIBSSH2_SFTP_S_IROTH,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"" TEST_PATH "/remove1/remove2/remove.txt\"]",
             .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"" TEST_PATH "/remove1/remove2/remove.txt\"]",
             .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true),
            "new storage /");
        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), PathRemoveError, "timeout removing file '%s'",
            strZ(fileRemove));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - rmdir LIBSSH2_ERROR_EAGAIN timeout");
        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/remove1\",0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RMDIR_EX, .param = "[\"" TEST_PATH "/remove1\"]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true),
            "new storage /");
        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), PathRemoveError, "timeout removing path '%s'",
            strZ(pathRemove1));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - rmdir ssh error");
        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/remove1\",0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4095,null,0]", .fileName = STRDEF(""),
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RMDIR_EX, .param = "[\"" TEST_PATH "/remove1\"]", .resultInt = LIBSSH2_ERROR_SOCKET_SEND},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true),
            "new storage /");
        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), PathRemoveError,
            "unable to remove path '%s': libssh2 error [-7]", strZ(pathRemove1));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNewRead()"))
    {
#ifdef HAVE_LIBSSH2
        Storage *storageTest = NULL;
        StorageRead *file = NULL;
        const String *fileName = STRDEF(TEST_PATH "/readtest.txt");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // Missing sftp error no such file
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/readtest.txt\",1,0,0]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            // Timeout EAGAIN
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/readtest.txt\",1,0,0]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_WRITING},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/readtest.txt\",1,0,0]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_OK},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_ERROR_NONE},
            // Error not sftp, not EAGAIN
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/readtest.txt\",1,0,0]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_METHOD_NOT_SUPPORTED},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_METHOD_NOT_SUPPORTED},
            // Read success
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/readtest.txt\",1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true),
            "new storage /");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read missing");

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");

        // Missing no such file
        TEST_ERROR_FMT(ioReadOpen(storageReadIo(file)), FileMissingError, STORAGE_ERROR_READ_MISSING, strZ(fileName));

        // Missing EAGAIN timeout
        TEST_ERROR_FMT(ioReadOpen(storageReadIo(file)), FileOpenError, STORAGE_ERROR_READ_OPEN ": libssh2 error [-37]", strZ(fileName));

        // Missing not sftp, not EAGAIN
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), false, "not sftp, not EAGAIN");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read success");

        // Mimic creation of TEST_PATH "/readtest.txt"
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_RESULT_INT(ioReadFd(storageReadIo(file)), -1, "check read fd");
        TEST_RESULT_VOID(ioReadClose(storageReadIo(file)), "close file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("storageReadClose()"))
    {
#ifdef HAVE_LIBSSH2
        // ------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("close success via storageReadSftpClose()");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // storageReadSftpClose
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/readtest.txt\",1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            // close(ioSessionFd()...)
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        Storage *storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        StorageRead *file = NULL;
        const String *fileName = STRDEF(TEST_PATH "/readtest.txt");

        // Mimic creation of fileName
        Buffer *outBuffer = bufNew(2);

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_RESULT_VOID(storageReadSftpClose((StorageReadSftp *)ioReadDriver(storageReadIo(file))), "close file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // ------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("close success via close()");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // close(ioSessionFd()...)
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/readtest.txt\",1,0,0]"},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        close(ioSessionFd(((StorageReadSftp *)ioReadDriver(storageReadIo(file)))->storage->ioSession));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // ------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("close, null sftpHandle");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // storageReadSftpClose null sftpHandle
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/readtest.txt\",1,0,0]"},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        ((StorageReadSftp *)ioReadDriver(storageReadIo(file)))->sftpHandle = NULL;
        TEST_RESULT_VOID(storageReadSftpClose(ioReadDriver(storageReadIo(file))), "close file null sftpHandle");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // ------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("close fail via storageReadSftpClose() EAGAIN");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // storageReadSftpClose
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/readtest.txt\",1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_ERROR_NONE},
            // close(ioSessionFd()...)
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_ERROR(
            storageReadSftpClose(ioReadDriver(storageReadIo(file))), FileCloseError,
            "timeout closing file '" TEST_PATH "/readtest.txt': libssh2 error [-37]");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // ------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("close fail via storageReadSftpClose() sftp error");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // storageReadSftpClose
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/readtest.txt\",1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_ERROR(
            storageReadSftpClose(ioReadDriver(storageReadIo(file))), FileCloseError,
            "unable to close file '" TEST_PATH "/readtest.txt' after read: libssh2 errno [-31]: sftp errno [4]");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // ------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("close fail via storageReadSftpClose() ssh error");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // storageReadSftpClose
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/readtest.txt\",1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_ZLIB},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_ERROR(
            storageReadSftpClose(ioReadDriver(storageReadIo(file))), FileCloseError,
            "unable to close file '" TEST_PATH "/readtest.txt' after read: libssh2 errno [-29]");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // ------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageReadSftp() sftp error");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // storageReadSftpClose
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/readtest.txt\",1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_ERROR(
            storageReadSftp(ioReadDriver(storageReadIo(file)), outBuffer, false), FileReadError,
            "unable to read '" TEST_PATH "/readtest.txt': sftp errno [4]");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // ------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageReadSftp() ssh error");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // storageReadSftpClose
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/readtest.txt\",1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = LIBSSH2_ERROR_ZLIB},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_ERROR(
            storageReadSftp(ioReadDriver(storageReadIo(file)), outBuffer, false), FileReadError,
            "unable to read '" TEST_PATH "/readtest.txt': libssh2 error [-29]");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNewWrite()"))
    {
#ifdef HAVE_LIBSSH2
        const String *fileName = STRDEF(TEST_PATH "/sub1/testfile");
        StorageWrite *file = NULL;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("permission denied");

        // Mimic creation of /noperm/noperm
        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // Permission denied
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/noperm/noperm\",26,416,0]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/noperm/noperm\",26,416,0]", .resultNull = true,
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        Storage *storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileNoPerm, .noAtomic = true, .noCreatePath = false), "new write file");
        TEST_ERROR_FMT(
            ioWriteOpen(storageWriteIo(file)), FileOpenError,
            STORAGE_ERROR_WRITE_OPEN ": libssh2 error [-31]: sftp error [3] permission denied", strZ(fileNoPerm));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("timeout");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // Timeout
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/noperm/noperm\",26,416,0]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/noperm/noperm\",26,416,0]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileNoPerm, .noAtomic = true, .noCreatePath = false), "new write file");
        TEST_ERROR_FMT(ioWriteOpen(storageWriteIo(file)), FileOpenError, "timeout while opening file '%s'", strZ(fileNoPerm));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("missing");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // Missing
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/missing\",26,416,0]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "\",488]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/missing\",26,416,0]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(
            file, storageNewWriteP(storageTest, STRDEF("missing"), .noAtomic = true, .noCreatePath = false), "new write file");
        TEST_ERROR_FMT(ioWriteOpen(storageWriteIo(file)), FileMissingError, STORAGE_ERROR_WRITE_MISSING, TEST_PATH "/missing");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write file - defaults");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true,
             .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub1\",488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
             .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",\"" TEST_PATH "/sub1/testfile\",7]",
             .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageWriteSftpClose timeout on fsync");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true,
             .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub1\",488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        const String *fileNameTmp = STRDEF(TEST_PATH "/sub1/testfile.pgbackrest.tmp");

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_ERROR_FMT(ioWriteClose(storageWriteIo(file)), FileSyncError, "timeout syncing file '%s'", strZ(fileNameTmp));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageWriteSftpClose error on fsync");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true,
             .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub1\",488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_SOCKET_SEND},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        fileNameTmp = STRDEF(TEST_PATH "/sub1/testfile.pgbackrest.tmp");

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_ERROR_FMT(
            ioWriteClose(storageWriteIo(file)),
            FileSyncError, "unable to sync file '%s' after write: libssh2 error [-7]", strZ(fileNameTmp));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageWriteSftpClose error on close");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true,
             .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub1\",488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_SOCKET_SEND},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_OK},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_ERROR_FMT(
            ioWriteClose(storageWriteIo(file)), FileCloseError,
            "unable to close file '%s' after write: libssh2 error [-7]", strZ(fileNameTmp));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("timeout on rename");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true,
             .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub1\",488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
             .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",\"" TEST_PATH "/sub1/testfile\",7]",
             .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_ERROR_FMT(ioWriteClose(storageWriteIo(file)), FileCloseError, "timeout renaming file '%s'", strZ(fileNameTmp));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp error on rename, other than LIBSSH2_FX_FAILURE");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true,
             .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub1\",488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
             .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",\"" TEST_PATH "/sub1/testfile\",7]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_CONNECTION_LOST},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_ERROR_FMT(
            ioWriteClose(storageWriteIo(file)), FileCloseError,
            "unable to move '%s' to '%s': libssh2 error [-31]: sftp error [7] connection lost", strZ(fileNameTmp),
            strZ(fileName));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp remove existing file on rename - LIBSSH2_FX_FILE_ALREADY_EXISTS");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true,
             .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub1\",488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
             .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",\"" TEST_PATH "/sub1/testfile\",7]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FILE_ALREADY_EXISTS},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"" TEST_PATH "/sub1/testfile\"]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
             .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",\"" TEST_PATH "/sub1/testfile\",7]",
             .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("ssh error on rename");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true,
             .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub1\",488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
             .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",\"" TEST_PATH "/sub1/testfile\",7]",
             .resultInt = LIBSSH2_ERROR_SOCKET_SEND},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_OK},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_ERROR_FMT(
            ioWriteClose(storageWriteIo(file)), FileCloseError,
            "unable to move '%s' to '%s': libssh2 error [-7]", strZ(fileNameTmp), strZ(fileName));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageWriteSftpClose() timeout EAGAIN on libssh2_sftp_close");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true,
             .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub1\",488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_ERROR_FMT(ioWriteClose(storageWriteIo(file)), FileCloseError, "timeout closing file '%s'", strZ(fileNameTmp));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write file - noAtomic = true");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true,
             .param = "[\"" TEST_PATH "/sub1/testfile\",26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub1\",488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/sub1/testfile\",26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName, .noAtomic = true), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write file - syncPath not NULL, .noSyncPath = true");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true, .param = "[\"" TEST_PATH "/sub1/testfile\",26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub1\",488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/sub1/testfile\",26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        // Make interface.pathSync != NULL
        ((StorageSftp *)storageDriver(storageTest))->interface.pathSync = malloc(1);

        TEST_ASSIGN(
            file, storageNewWriteP(storageTest, fileName, .noAtomic = true, .noSyncPath = true), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");

        free(((StorageSftp *)storageDriver(storageTest))->interface.pathSync);

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write file - syncPath not NULL, .noSyncPath = false");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true, .param = "[\"" TEST_PATH "/sub1/testfile\",26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub1\",488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/sub1/testfile\",26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        // Make interface.pathSync != NULL
        ((StorageSftp *)storageDriver(storageTest))->interface.pathSync = malloc(1);

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName, .noAtomic = true, .noSyncPath = false), "new write file");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");

        free(((StorageSftp *)storageDriver(storageTest))->interface.pathSync);

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write file - syncFile false");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true, .param = "[\"" TEST_PATH "/sub1/testfile\",26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub1\",488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/sub1/testfile\",26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        ((StorageSftp *)storageDriver(storageTest))->interface.pathSync = malloc(1);

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName, .noAtomic = true, .noSyncPath = false), "new write file");
        ((StorageWriteSftp *)ioWriteDriver(storageWriteIo(file)))->interface.syncFile = false;
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");

        free(((StorageSftp *)storageDriver(storageTest))->interface.pathSync);

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageWriteSftpClose() - null sftpHandle");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true, .param = "[\"" TEST_PATH "/sub1/testfile\",26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub1\",488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/sub1/testfile\",26,416,0]"},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName, .noAtomic = true, .noSyncPath = false), "new write file");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");

        // Make sftpHandle NULL
        ((StorageWriteSftp *)ioWriteDriver(storageWriteIo(file)))->sftpHandle = NULL;

        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePut() and storageGet()"))
    {
#ifdef HAVE_LIBSSH2
        ioBufferSizeSet(65536);

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "\",1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[65536]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[65536]", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        Storage *storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get error - attempt to get directory");

        TEST_ERROR(
            storageGetP(storageNewReadP(storageTest, TEST_PATH_STR, .ignoreMissing = false)), FileReadError,
            "unable to read '" TEST_PATH "': sftp errno [4]");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - empty file");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/test.empty.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
             .param = "[\"" TEST_PATH "/test.empty.pgbackrest.tmp\",\"" TEST_PATH "/test.empty\",7]",
             .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        const String *emptyFile = STRDEF(TEST_PATH "/test.empty");
        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, emptyFile), NULL), "put empty file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - empty file, already exists");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/test.empty.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
             .param = "[\"" TEST_PATH "/test.empty.pgbackrest.tmp\",\"" TEST_PATH "/test.empty\",7]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"" TEST_PATH "/test.empty\"]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"" TEST_PATH "/test.empty\"]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = 99},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ERROR(
            storagePutP(storageNewWriteP(storageTest, emptyFile), NULL), FileRemoveError,
            "unable to remove existing '" TEST_PATH "/test.empty': libssh2 error [-31]: sftp error [99] unknown error");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - empty file, remove already existing file, storageWriteSftpRename timeout EAGAIN");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/test.empty.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
             .param = "[\"" TEST_PATH "/test.empty.pgbackrest.tmp\",\"" TEST_PATH "/test.empty\",7]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"" TEST_PATH "/test.empty\"]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"" TEST_PATH "/test.empty\"]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
             .param = "[\"" TEST_PATH "/test.empty.pgbackrest.tmp\",\"" TEST_PATH "/test.empty\",7]",
             .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
             .param = "[\"" TEST_PATH "/test.empty.pgbackrest.tmp\",\"" TEST_PATH "/test.empty\",7]",
             .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ERROR(
            storagePutP(storageNewWriteP(storageTest, emptyFile), NULL), FileWriteError,
            "timeout moving '" TEST_PATH "/test.empty.pgbackrest.tmp' to '" TEST_PATH "/test.empty'");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - empty file, remove already existing file, storageWriteSftpRename ssh error");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/test.empty.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
             .param = "[\"" TEST_PATH "/test.empty.pgbackrest.tmp\",\"" TEST_PATH "/test.empty\",7]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"" TEST_PATH "/test.empty\"]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"" TEST_PATH "/test.empty\"]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
             .param = "[\"" TEST_PATH "/test.empty.pgbackrest.tmp\",\"" TEST_PATH "/test.empty\",7]",
             .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
             .param = "[\"" TEST_PATH "/test.empty.pgbackrest.tmp\",\"" TEST_PATH "/test.empty\",7]",
             .resultInt = LIBSSH2_ERROR_SOCKET_SEND},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_OK},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ERROR(
            storagePutP(storageNewWriteP(storageTest, emptyFile), NULL), FileRemoveError,
            "unable to move '" TEST_PATH "/test.empty.pgbackrest.tmp' to '" TEST_PATH "/test.empty': libssh2 error [-7]");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - empty file, EAGAIN fail on remove already existing file");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/test.empty.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
             .param = "[\"" TEST_PATH "/test.empty.pgbackrest.tmp\",\"" TEST_PATH "/test.empty\",7]",
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"" TEST_PATH "/test.empty\"]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"" TEST_PATH "/test.empty\"]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ERROR(
            storagePutP(storageNewWriteP(storageTest, emptyFile), NULL), FileWriteError,
            "timeout unlinking '" TEST_PATH "/test.empty'");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - file with contents - timeout EAGAIN libssh2_sftp_write");

        ioBufferSizeSet(2);

        const Buffer *failBuffer = BUFSTRDEF("FAIL\n");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/test.txt.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[2]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[2]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ERROR(
            storagePutP(storageNewWriteP(storageTest, STRDEF(TEST_PATH "/test.txt")), failBuffer), FileWriteError,
            "timeout writing '" TEST_PATH "/test.txt.pgbackrest.tmp'");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - file with contents - error other than EAGAIN libssh2_sftp_write");

        failBuffer = BUFSTRDEF("FAIL\n");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/test.txt.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[2]", .resultInt = LIBSSH2_ERROR_SOCKET_SEND},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ERROR(
            storagePutP(storageNewWriteP(storageTest, STRDEF(TEST_PATH "/test.txt")), failBuffer), FileWriteError,
            "unable to write '" TEST_PATH "/test.txt.pgbackrest.tmp': libssh2 error [-7]");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - file with contents");

        const Buffer *buffer = BUFSTRDEF("TESTFILE\n");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/test.txt.pgbackrest.tmp\",26,416,0]"},
            // Not passing buffer param, see shim function, initial passed buffer contains random data
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[2]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[2]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[2]", .resultInt = 2},
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[2]", .resultInt = 2},
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[2]", .resultInt = 2},
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[2]", .resultInt = 1},
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[1]", .resultInt = 1},
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[1]", .resultInt = 1},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
             .param = "[\"" TEST_PATH "/test.txt.pgbackrest.tmp\",\"" TEST_PATH "/test.txt\",7]",
             .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, STRDEF(TEST_PATH "/test.txt")), buffer), "put test file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - ignore missing");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/BOGUS\",1,0,0]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_RESULT_PTR(
            storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/" BOGUS_STR), .ignoreMissing = true)), NULL,
            "get missing file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - empty file");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/test.empty\",1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.empty"))), "get empty");
        TEST_RESULT_UINT(bufSize(buffer), 0, "size is 0");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - file with contents");

        const Buffer *outBuffer = bufNew(2);

        ioBufferSizeSet(65536);

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/test.txt\",1,0,0]"},
            // Not passing buffer param, see shim function, initial passed buffer contains random data
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[65536]", .resultInt = 9, .readBuffer = STRDEF("TESTFILE\n")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[65527]", .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(outBuffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"))), "get text");
        TEST_RESULT_UINT(bufSize(outBuffer), 9, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(outBuffer), "TESTFILE\n", bufSize(outBuffer)) == 0, true, "check content");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - exact size smaller");

        ioBufferSizeSet(2);

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/test.txt\",1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = 2, .readBuffer = STRDEF("TE")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = 2, .readBuffer = STRDEF("ST")},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt")), .exactSize = 4), "get exact");
        TEST_RESULT_UINT(bufSize(buffer), 4, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "TEST", bufSize(buffer)) == 0, true, "check content");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - exact size larger");

        ioBufferSizeSet(4);
        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/test.txt\",1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[4]", .resultInt = 4, .readBuffer = STRDEF("TEST")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[4]", .resultInt = 4, .readBuffer = STRDEF("FILE")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[4]", .resultInt = 1, .readBuffer = STRDEF("\n")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[3]", .resultInt = 0},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ERROR(
            storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt")), .exactSize = 64), FileReadError,
            "unable to read 64 byte(s) from '" TEST_PATH "/test.txt'");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - smaller buffer size");

        ioBufferSizeSet(2);

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/test.txt\",1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = 2, .readBuffer = STRDEF("TE")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = 2, .readBuffer = STRDEF("ST")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = 2, .readBuffer = STRDEF("FI")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = 2, .readBuffer = STRDEF("LE")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = 1, .readBuffer = STRDEF("\n")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[1]", .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"))), "get text");
        TEST_RESULT_UINT(bufSize(buffer), 9, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "TESTFILE\n", bufSize(buffer)) == 0, true, "check content");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid read offset bytes");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/test.txt\",1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_SEEK64, .param = "[18446744073709551615]"},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_BAD_MESSAGE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ERROR(
            storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"), .offset = UINT64_MAX)), FileOpenError,
            "unable to seek to 18446744073709551615 in file '" TEST_PATH "/test.txt'");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid read");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/test.txt\",1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_BAD_MESSAGE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ERROR(
            storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"))), FileReadError,
            "unable to read '" TEST_PATH "/test.txt': sftp errno [5]");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read limited bytes");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/test.txt\",1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = 2, .readBuffer = STRDEF("TE")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = 2, .readBuffer = STRDEF("ST")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = 2, .readBuffer = STRDEF("FI")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[1]", .resultInt = 1, .readBuffer = STRDEF("LE")},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"), .limit = VARUINT64(7))), "get");
        TEST_RESULT_UINT(bufSize(buffer), 7, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "TESTFIL", bufSize(buffer)) == 0, true, "check content");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read offset bytes");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/test.txt\",1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_SEEK64, .param = "[4]"},
            // Simulate seeking offset 4
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = 2, .readBuffer = STRDEF("FI")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = 2, .readBuffer = STRDEF("LE")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = 1, .readBuffer = STRDEF("\n")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[1]", .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"), .offset = 4)), "get");
        TEST_RESULT_UINT(bufSize(buffer), 5, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "FILE\n", bufSize(buffer)) == 0, true, "check content");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - read timeout EAGAIN");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/test.txt\",1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ERROR(
            storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"))), FileReadError,
            "timeout reading '" TEST_PATH "/test.txt'");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("storageRemove()"))
    {
#ifdef HAVE_LIBSSH2
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove - file missing");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/missing\"]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/missing\"]", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, strZ(FSLASH_STR));
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        Storage *storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_RESULT_VOID(storageRemoveP(storageTest, STRDEF("missing")), "remove missing file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove - file missing, .errorOnMissing = true, sftp error LIBSSH2_FX_NO_SUCH_FILE");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/missing\"]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/missing\"]", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ERROR(storageRemoveP(storageTest, STRDEF("missing"), .errorOnMissing = true), FileRemoveError,
                   "unable to remove '/missing': libssh2 error [-31]: sftp error [2] no such file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove - file missing, sftp error other than LIBSSH2_FX_NO_SUCH_FILE");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/missing\"]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/missing\"]", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_CONNECTION_LOST},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_CONNECTION_LOST},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ERROR(storageRemoveP(storageTest, STRDEF("missing")), FileRemoveError,
                   "unable to remove '/missing': libssh2 error [-31]: sftp error [7] connection lost");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove - file missing, ssh error, .errorOnMissing = true");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/missing\"]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/missing\"]", .resultInt = LIBSSH2_ERROR_BAD_SOCKET},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ERROR(storageRemoveP(storageTest, STRDEF("missing"), .errorOnMissing = true), FileRemoveError,
                   "unable to remove '/missing': libssh2 error [-45]");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove - file missing, timeout EAGAIN, .errorOnMissing = true");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/missing\"]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/missing\"]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ERROR(
            storageRemoveP(storageTest, STRDEF("missing"), .errorOnMissing = true), FileRemoveError, "timeout removing '/missing'");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove - LIBSSH2_ERROR_SOCKET_SEND");

        const String *fileRemove1 = STRDEF(TEST_PATH "/remove.txt");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"" TEST_PATH "/remove.txt\"]",
             .resultInt = LIBSSH2_ERROR_SOCKET_SEND},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true),
            "new storage /");
        TEST_RESULT_VOID(storageRemoveP(storageTest, fileRemove1), "remove file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageWriteSftpOpen libssh2_sftp_open_ex timeout EAGAIN");

        StorageWrite *file = NULL;
        const String *fileName = STRDEF(TEST_PATH "/sub1/testfile");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true,
             .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub1\",488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true, .resultInt = LIBSSH2_ERROR_EAGAIN,
             .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true, .resultInt = LIBSSH2_ERROR_EAGAIN,
             .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName, .noSyncFile = true), "storageWriteSftpOpen timeout EAGAIN");
        TEST_ERROR_FMT(ioWriteOpen(storageWriteIo(file)), FileOpenError, "timeout while opening file '%s'", strZ(fileName));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageWriteSftpOpen libssh2_sftp_open_ex sftp error");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true,
             .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub1\",488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL,
             .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName, .noSyncFile = true), "storageWriteSftpOpen sftp error");
        TEST_ERROR_FMT(
            ioWriteOpen(storageWriteIo(file)), FileOpenError,
            STORAGE_ERROR_WRITE_OPEN ": libssh2 error [-31]: sftp error [3] permission denied", strZ(fileName));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageWriteSftpOpen libssh2_sftp_open_ex ssh error");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true,
             .param = "[\"" TEST_PATH "/sub1/testfile.pgbackrest.tmp\",26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SOCKET_SEND},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SOCKET_SEND},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SOCKET_SEND},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName, .noSyncFile = true), "storageWriteSftpOpen ssh error");
        TEST_ERROR_FMT(
            ioWriteOpen(storageWriteIo(file)), FileOpenError, STORAGE_ERROR_WRITE_OPEN ": libssh2 error [-7]", strZ(fileName));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("StorageRead"))
    {
#ifdef HAVE_LIBSSH2
        Storage *storageTest = NULL;
        StorageRead *file = NULL;

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // Ignore missing - file with no permission to read
            // New read file, check ignore missing, check name require no libssh2 calls
            // Permission denied
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/noperm/noperm\",1,0,0]", .resultNull = true,
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            // File missing
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/test.file\",1,0,0]", .resultNull = true,
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            // Ignore missing
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/test.file\",1,0,0]", .resultNull = true,
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("ignore missing - file with no permission to read");

        // Mimic creation of /noperm/noperm
        TEST_ASSIGN(file, storageNewReadP(storageTest, fileNoPerm, .ignoreMissing = true), "new read file");
        TEST_RESULT_BOOL(storageReadIgnoreMissing(file), true, "check ignore missing");
        TEST_RESULT_STR(storageReadName(file), fileNoPerm, "check name");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("permission denied");

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileNoPerm), "new no perm read file");
        TEST_ERROR_FMT(
            ioReadOpen(storageReadIo(file)),
            FileOpenError,
            STORAGE_ERROR_READ_OPEN ": libssh2 error [-31]: sftp error [3] permission denied", strZ(fileNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file missing");

        const String *fileName = STRDEF(TEST_PATH "/test.file");

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new missing read file");
        TEST_ERROR_FMT(ioReadOpen(storageReadIo(file)), FileMissingError, STORAGE_ERROR_READ_MISSING, strZ(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("ignore missing");

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName, .ignoreMissing = true), "new missing read file");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), false, "missing file ignored");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("incremental load, storageReadSftp(), storageReadSftpEof()");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // Open file
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/test.file\",1,0,0]",
             .resultInt = LIBSSH2_ERROR_NONE},
            // Check file name, check file type, check offset, check limit require no libssh2 calls
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[44]", .resultInt = 2, .readBuffer = STRDEF("TE")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[42]", .resultInt = 2, .readBuffer = STRDEF("ST")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[40]", .resultInt = 2, .readBuffer = STRDEF("FI")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[38]", .resultInt = 2, .readBuffer = STRDEF("LE")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[36]", .resultInt = 1, .readBuffer = STRDEF("\n")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[35]", .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, strZ(FSLASH_STR));
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        Buffer *buffer = bufNew(0);
        Buffer *outBuffer = bufNew(2);
        const Buffer *expectedBuffer = BUFSTRDEF("TESTFILE\n");

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(
                file,
                storageReadMove(storageNewReadP(storageTest, fileName, .limit = VARUINT64(44)), memContextPrior()),
                "new read file");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_RESULT_STR(storageReadName(file), fileName, "check file name");
        TEST_RESULT_UINT(storageReadType(file), STORAGE_SFTP_TYPE, "check file type");
        TEST_RESULT_UINT(storageReadOffset(file), 0, "check offset");
        TEST_RESULT_UINT(varUInt64(storageReadLimit(file)), 44, "check limit");

        TEST_RESULT_VOID(ioRead(storageReadIo(file), outBuffer), "load data");
        bufCat(buffer, outBuffer);
        bufUsedZero(outBuffer);
        TEST_RESULT_VOID(ioRead(storageReadIo(file), outBuffer), "load data");
        bufCat(buffer, outBuffer);
        bufUsedZero(outBuffer);
        TEST_RESULT_VOID(ioRead(storageReadIo(file), outBuffer), "load data");
        bufCat(buffer, outBuffer);
        bufUsedZero(outBuffer);
        TEST_RESULT_VOID(ioRead(storageReadIo(file), outBuffer), "load data");
        bufCat(buffer, outBuffer);
        bufUsedZero(outBuffer);
        TEST_RESULT_BOOL(bufEq(buffer, expectedBuffer), false, "check file contents (not all loaded yet)");

        TEST_RESULT_VOID(ioRead(storageReadIo(file), outBuffer), "load data");
        bufCat(buffer, outBuffer);
        bufUsedZero(outBuffer);

        TEST_RESULT_VOID(ioRead(storageReadIo(file), outBuffer), "no data to load");
        TEST_RESULT_UINT(bufUsed(outBuffer), 0, "buffer is empty");

        TEST_RESULT_VOID(storageReadSftp(ioReadDriver(storageReadIo(file)), outBuffer, true), "no data to load from driver either");
        TEST_RESULT_UINT(bufUsed(outBuffer), 0, "buffer is empty");

        TEST_RESULT_BOOL(bufEq(buffer, expectedBuffer), true, "check file contents (all loaded)");

        TEST_RESULT_BOOL(ioReadEof(storageReadIo(file)), true, "eof");
        TEST_RESULT_BOOL(ioReadEof(storageReadIo(file)), true, "still eof");
        TEST_RESULT_BOOL(storageReadSftpEof(ioReadDriver(storageReadIo(file))), true, "storageReadSftpEof eof true");

        TEST_RESULT_VOID(ioReadClose(storageReadIo(file)), "close file");

        TEST_RESULT_VOID(storageReadFree(storageNewReadP(storageTest, fileName)), "free file");

        TEST_RESULT_VOID(storageReadMove(NULL, memContextTop()), "move null file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("StorageWrite"))
    {
#ifdef HAVE_LIBSSH2
        Storage *storageTest = NULL;
        StorageWrite *file = NULL;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check getters");

        // mimic creation of /noperm/noperm
        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(
            file,
            storageNewWriteP(
                storageTest, fileNoPerm, .modeFile = 0444, .modePath = 0555, .noSyncFile = true, .noSyncPath = true,
                .noAtomic = true),
            "new write file");
        TEST_RESULT_BOOL(storageWriteAtomic(file), false, "check atomic");
        TEST_RESULT_BOOL(storageWriteCreatePath(file), true, "check create path");
        TEST_RESULT_INT(storageWriteModeFile(file), 0444, "check mode file");
        TEST_RESULT_INT(storageWriteModePath(file), 0555, "check mode path");
        TEST_RESULT_STR(storageWriteName(file), fileNoPerm, "check name");
        TEST_RESULT_BOOL(storageWriteSyncPath(file), false, "check sync path");
        TEST_RESULT_BOOL(storageWriteSyncFile(file), false, "check sync file");
        TEST_RESULT_BOOL(storageWriteTruncate(file), true, "file will be truncated");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("permission denied");

        // mimic creation of /noperm/noperm
        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = 0},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = 0},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_MATCH},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_INIT},
            // Permission denied
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/noperm/noperm\",26,416,0]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
#ifdef LIBSSH2_HOSTKEY_HASH_SHA256
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha256");
#else
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
#endif
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileNoPerm, .noAtomic = true), "new write file");
        TEST_ERROR_FMT(
            ioWriteOpen(storageWriteIo(file)),
            FileOpenError, STORAGE_ERROR_WRITE_OPEN ": libssh2 error [-31]: sftp error [3] permission denied",
            strZ(fileNoPerm));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file missing");

        const String *fileName = STRDEF(TEST_PATH "/sub1/test.file");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = 0},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = 0},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS2_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_MATCH},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]", .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_INIT},
            // Missing
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/sub1/test.file\",26,416,0]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/sub1/test.file\",26,416,0]", .resultNull = true,
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"" TEST_PATH "/sub1\",488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/sub1/test.file\",26,416,0]", .resultNull = true,
             .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
#ifdef LIBSSH2_HOSTKEY_HASH_SHA256
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha256");
#else
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
#endif
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, "~/.ssh/known_hosts");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, "/home/" TEST_USER "/.ssh/known_hosts2");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName, .noAtomic = true), "new write file");
        TEST_ERROR_FMT(ioWriteOpen(storageWriteIo(file)), FileMissingError, STORAGE_ERROR_WRITE_MISSING, strZ(fileName));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write file success");

        const Buffer *buffer = BUFSTRDEF("TESTFILE\n");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = 0},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = 0},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_MATCH},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_INIT},
            // Open file
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/sub1/test.file.pgbackrest.tmp\",26,416,0]"},
            // Write null and zero size buffers are noops
            // Write filled buffer to file
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[9]", .resultInt = 9},
            // Close file
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
             .param = "[\"" TEST_PATH "/sub1/test.file.pgbackrest.tmp\",\"" TEST_PATH "/sub1/test.file\",7]",
             .resultInt = LIBSSH2_ERROR_NONE},
            // Move context
            // Open file
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/sub1/test.file\",1,0,0]"},
            // Read file
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[65536]", .resultInt = 9, .readBuffer = STRDEF("TESTFILE\n")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[65527]", .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE},
            // Check path mode
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/sub1\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                      LIBSSH2_SFTP_ATTR_SIZE,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // Check file mode
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/sub1/test.file\",1]",
             .fileName = STRDEF("test.file"), .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // Remove filename
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"" TEST_PATH "/sub1/test.file\"]",
             .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "md5");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        ioBufferSizeSet(65536);

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(file, storageWriteMove(storageNewWriteP(storageTest, fileName), memContextPrior()), "new write file");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_VOID(ioWrite(storageWriteIo(file), NULL), "write null buffer to file");
        TEST_RESULT_VOID(ioWrite(storageWriteIo(file), bufNew(0)), "write zero buffer to file");
        TEST_RESULT_VOID(ioWrite(storageWriteIo(file), buffer), "write to file");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");
        TEST_RESULT_VOID(storageWriteFree(storageNewWriteP(storageTest, fileName)), "free file");
        TEST_RESULT_VOID(storageWriteMove(NULL, memContextTop()), "move null file");

        Buffer *expectedBuffer = storageGetP(storageNewReadP(storageTest, fileName));
        TEST_RESULT_BOOL(bufEq(buffer, expectedBuffer), true, "check file contents");
        TEST_RESULT_INT(storageInfoP(storageTest, strPath(fileName)).mode, 0750, "check path mode");
        TEST_RESULT_INT(storageInfoP(storageTest, fileName).mode, 0640, "check file mode");

        storageRemoveP(storageTest, fileName, .errorOnMissing = true);

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write subpath and file success");

        fileName = STRDEF(TEST_PATH "/sub2/test.file");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = 0},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = 0},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS2_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_MATCH},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_INIT},
            // Open file
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/sub2/test.file\",26,384,0]"},
            // Write filled buffer to file
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[9]", .resultInt = 9},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE},
            // Open file
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "/sub2/test.file\",1,0,0]"},
            // Read file
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[65536]", .resultInt = 9, .readBuffer = STRDEF("TESTFILE\n")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[65527]", .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE},
            // Check path mode
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/sub2\",1]", .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                      LIBSSH2_SFTP_ATTR_SIZE,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // Check file mode
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"" TEST_PATH "/sub2/test.file\",1]",
             .fileName = STRDEF("test.file"), .resultInt = LIBSSH2_ERROR_NONE,
             .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR,
             .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
             .mtime = 1656434296, .uid = TEST_USER_ID, .gid = TEST_GROUP_ID},
            // Remove filename
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"" TEST_PATH "/sub2/test.file\"]",
             .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, "/home/" TEST_USER "/.ssh/known_hosts  ");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, "      /home/" TEST_USER "/.ssh/known_hosts2");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        ioBufferSizeSet(65536);

        storageTest = storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
            .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
            .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true);

        TEST_ASSIGN(
            file,
            storageNewWriteP(
                storageTest, fileName, .modePath = 0700, .modeFile = 0600, .noSyncPath = true, .noSyncFile = true,
                .noAtomic = true),
            "new write file");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_VOID(ioWrite(storageWriteIo(file), buffer), "write to file");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");

        expectedBuffer = storageGetP(storageNewReadP(storageTest, fileName));
        TEST_RESULT_BOOL(bufEq(buffer, expectedBuffer), true, "check file contents");
        TEST_RESULT_INT(storageInfoP(storageTest, strPath(fileName)).mode, 0700, "check path mode");
        TEST_RESULT_INT(storageInfoP(storageTest, fileName).mode, 0600, "check file mode");
        TEST_RESULT_STR_Z(
            strLstGet(strLstNewVarLst(cfgOptionLst(cfgOptRepoSftpKnownHost)), 0), "/home/" TEST_USER "/.ssh/known_hosts  ",
            "check known hosts path trailing spaces");
        TEST_RESULT_STR_Z(
            strLstGet(strLstNewVarLst(cfgOptionLst(cfgOptRepoSftpKnownHost)), 1), "      /home/" TEST_USER "/.ssh/known_hosts2",
            "check known hosts path leading spaces");

        storageRemoveP(storageTest, fileName, .errorOnMissing = true);

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("storageRepo*()"))
    {
#ifdef HAVE_LIBSSH2
        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_MATCH},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_INIT},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Load configuration to set repo-path and stanza
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "accept-new");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, "   ~/.ssh/id_rsa");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, "               ~/.ssh/id_rsa.pub");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        const Storage *storage = NULL;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageRepo() - cached/not cached");

        // Set storage helper
        static const StorageHelper storageHelperList[] = {STORAGE_SFTP_HELPER, STORAGE_END_HELPER};
        storageHelperInit(storageHelperList);

        TEST_RESULT_PTR(storageHelper.storageRepo, NULL, "repo storage not cached");
        TEST_ASSIGN(storage, storageRepo(), "new storage");
        TEST_RESULT_PTR(storageHelper.storageRepo[0], storage, "repo storage cached");
        TEST_RESULT_PTR(storageRepo(), storage, "get cached storage");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageRepo() - confirm settings");

        TEST_ERROR(storagePathP(storage, STRDEF("<BOGUS>/path")), AssertError, "invalid expression '<BOGUS>'");
        TEST_ERROR(storageNewWriteP(storage, writeFile), AssertError, "assertion 'this->write' failed");

        TEST_RESULT_STR_Z(storagePathP(storage, NULL), TEST_PATH, "check base path");
        TEST_RESULT_STR_Z(
            storagePathP(storage, STORAGE_REPO_ARCHIVE_STR), TEST_PATH "/archive/db", "check archive path");
        TEST_RESULT_STR_Z(
            storagePathP(storage, STRDEF(STORAGE_REPO_ARCHIVE "/simple")), TEST_PATH "/archive/db/simple",
            "check simple path");
        TEST_RESULT_STR_Z(
            storagePathP(storage, STRDEF(STORAGE_REPO_ARCHIVE "/17-1/700000007000000070000000")),
            TEST_PATH "/archive/db/17-1/7000000070000000/700000007000000070000000", "check segment path");
        TEST_RESULT_STR_Z(
            storagePathP(storage, STRDEF(STORAGE_REPO_ARCHIVE "/17-1/00000008.history")),
            TEST_PATH "/archive/db/17-1/00000008.history", "check history path");
        TEST_RESULT_STR_Z(
            storagePathP(storage, STRDEF(STORAGE_REPO_ARCHIVE "/17-1/000000010000014C0000001A.00000028.backup")),
            TEST_PATH "/archive/db/17-1/000000010000014C/000000010000014C0000001A.00000028.backup",
            "check archive backup path");
        TEST_RESULT_STR_Z(storagePathP(storage, STORAGE_REPO_BACKUP_STR), TEST_PATH "/backup/db", "check backup path");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storage)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageRepo() - helper does not fail when stanza option not set");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_MATCH},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_INIT},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Change the stanza to NULL with the stanzaInit flag still true, make sure helper does not fail when stanza option not set
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, "/home/" TEST_USER "/.ssh/known_hosts");
        HRN_CFG_LOAD(cfgCmdInfo, argList);

        TEST_ASSIGN(storage, storageRepo(), "new repo storage no stanza");
        TEST_RESULT_STR(storageHelper.stanza, NULL, "stanza NULL");

        TEST_RESULT_STR_Z(
            storagePathP(storage, STORAGE_REPO_ARCHIVE_STR), TEST_PATH "/archive", "check archive path - NULL stanza");
        TEST_RESULT_STR_Z(
            storagePathP(storage, STRDEF(STORAGE_REPO_ARCHIVE "/simple")), TEST_PATH "/archive/simple",
            "check simple archive path - NULL stanza");
        TEST_RESULT_STR_Z(storagePathP(storage, STORAGE_REPO_BACKUP_STR), TEST_PATH "/backup", "check backup path - NULL stanza");
        TEST_RESULT_STR_Z(
            storagePathP(storage, STRDEF(STORAGE_REPO_BACKUP "/simple")), TEST_PATH "/backup/simple",
            "check simple backup path - NULL stanza");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storage)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageRepoWrite() - confirm write enabled");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_MATCH},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_INIT},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        TEST_RESULT_PTR(storageHelper.storageRepoWrite, NULL, "repo write storage not cached");
        TEST_ASSIGN(storage, storageRepoWrite(), "new write storage");
        TEST_RESULT_PTR(storageHelper.storageRepoWrite[0], storage, "repo write storage cached");
        TEST_RESULT_PTR(storageRepoWrite(), storage, "get cached storage");

        TEST_RESULT_BOOL(storage->write, true, "get write enabled");
        TEST_RESULT_UINT(storageType(storage), storage->pub.type, "check type");
        TEST_RESULT_STR_Z(strIdToStr(storageType(storage)), "sftp", "storage type is sftp");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storage)));
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("storageRepoGet() and StorageDriverCifs"))
    {
#ifdef HAVE_LIBSSH2
        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = HANDSHAKE_PARAM, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_KNOWNHOST_INIT},
            {.function = HRNLIBSSH2_KNOWNHOST_READFILE, .param = "[\"" KNOWNHOSTS_FILE_CSTR "\",1]", .resultInt = 5},
            {.function = HRNLIBSSH2_SESSION_HOSTKEY, .len = 20, .type = LIBSSH2_HOSTKEY_TYPE_RSA, .resultZ = HOSTKEY},
            {.function = HRNLIBSSH2_KNOWNHOST_CHECKP, .param = "[\"localhost\",22,\"" HOSTKEY "\",20,65537]",
             .resultInt = LIBSSH2_KNOWNHOST_CHECK_MATCH},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",null]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_INIT},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Load configuration
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid storage type");

        static const StorageHelper storageHelperListError[] = {{.type = STORAGE_CIFS_TYPE}, STORAGE_END_HELPER};
        storageHelperInit(storageHelperListError);

        TEST_ERROR(storageRepoGet(0, true), AssertError, "invalid storage type");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storage configuration");

        // Set storage helper
        static const StorageHelper storageHelperList[] = {STORAGE_SFTP_HELPER, STORAGE_END_HELPER};
        storageHelperInit(storageHelperList);

        const Storage *storage = NULL;
        TEST_ASSIGN(storage, storageRepoGet(0, true), "get sftp repo storage");
        TEST_RESULT_UINT(storageType(storage), STORAGE_SFTP_TYPE, "check storage type");
        TEST_RESULT_BOOL(storageFeature(storage, storageFeaturePath), true, "check path feature");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write object path sync false");

        // Create a FileWrite object with path sync enabled and ensure that path sync is false in the write object
        StorageWrite *file = NULL;
        TEST_ASSIGN(file, storageNewWriteP(storage, STRDEF("somefile"), .noSyncPath = false), "new file write");

        TEST_RESULT_BOOL(storageWriteSyncPath(file), false, "path sync is disabled");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path sync result is noop");

        // Test the path sync function -- pass a bogus path to ensure that this is a noop
        TEST_RESULT_VOID(storagePathSyncP(storage, STRDEF(BOGUS_STR)), "path sync is a noop");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storage)));
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("storageSftpLibSsh2SessionFreeResource()"))
    {
#ifdef HAVE_LIBSSH2
        Storage *storageTest = NULL;
        StorageSftp *storageSftp = NULL;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpLibSsh2SessionFreeResource() sftpHandle, sftpSession, session not NULL, branch tests, EAGAIN");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "\",1,0,1]"},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_SHUTDOWN, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},

            {.function = HRNLIBSSH2_SFTP_SHUTDOWN, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_DISCONNECT_EX, .param = "[11,\"pgBackRest instance shutdown\",\"\"]",
             .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SESSION_DISCONNECT_EX, .param = "[11,\"pgBackRest instance shutdown\",\"\"]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_FREE, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_NO_BLOCK_READING_WRITING},
            {.function = HRNLIBSSH2_SESSION_FREE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE},
            HRNLIBSSH2_MACRO_SHUTDOWN(),
        });

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostUser, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyHashType, "sha1");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true),
            "new storage (defaults)");
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "assign storage");

        // Populate sftpHandle, NULL sftpSession and session
        storageSftp->sftpHandle = libssh2_sftp_open_ex(storageSftp->sftpSession, TEST_PATH, 25, 1, 0, 1);

        TEST_RESULT_VOID(storageSftpLibSsh2SessionFreeResource(storageSftp), "freeResource not NULL sftpHandle");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpLibSsh2SessionFreeResource() sftpHandle, sftpSession, session all NULL");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = NULL}
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true),
            "new storage (defaults)");
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "assign storage");

        // NULL out sftpSession and session
        storageSftp->sftpSession = NULL;
        storageSftp->session = NULL;

        TEST_RESULT_VOID(memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest))), "free resource all NULL");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpLibSsh2SessionFreeResource() sftp close handle failure, sftpHandle not NULL, libssh2 error");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "\",1,0,1]"},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_SOCKET_SEND},
            {.function = NULL}
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true),
            "new storage (defaults)");
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "assign storage");

        // Populate sftpHandle, NULL out sftpSession and session
        storageSftp->sftpHandle = libssh2_sftp_open_ex(storageSftp->sftpSession, TEST_PATH, 25, 1, 0, 1);
        storageSftp->sftpSession = NULL;
        storageSftp->session = NULL;

        TEST_ERROR_FMT(
            memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest))),
            ServiceError, "failed to close sftpHandle: libssh2 errno [-7]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpLibSsh2SessionFreeResource() sftp close handle failure, sftpHandle not NULL, EAGAIN");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "\",1,0,1]"},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            {.function = NULL}
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true),
            "new storage (defaults)");
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "assign storage");

        // Populate sftpHandle, NULL out sftpSession and session
        storageSftp->sftpHandle = libssh2_sftp_open_ex(storageSftp->sftpSession, TEST_PATH, 25, 1, 0, 1);
        storageSftp->sftpSession = NULL;
        storageSftp->session = NULL;

        TEST_ERROR_FMT(
            memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest))),
            ServiceError, "timeout closing sftpHandle: libssh2 errno [-37]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpLibSsh2SessionFreeResource() sftp close handle failure, sftpHandle not NULL, libssh2 sftp error");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "\",1,0,1]"},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_CONNECTION_LOST},
            {.function = NULL}
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true),
            "new storage (defaults)");
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "assign storage");

        // Populate sftpHandle, NULL out sftpSession and session
        storageSftp->sftpHandle = libssh2_sftp_open_ex(storageSftp->sftpSession, TEST_PATH, 25, 1, 0, 1);
        storageSftp->sftpSession = NULL;
        storageSftp->session = NULL;

        TEST_ERROR_FMT(
            memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest))),
            ServiceError, "failed to close sftpHandle: libssh2 errno [-31]: sftp errno [7]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpLibSsh2SessionFreeResource() sftp shutdown failure libssh2 error");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_SHUTDOWN, .resultInt = LIBSSH2_ERROR_SOCKET_SEND},
            {.function = NULL},
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true),
            "new storage (defaults)");
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "assign storage");
        TEST_ERROR_FMT(
            memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest))), ServiceError,
            "failed to shutdown sftpSession: libssh2 errno [-7]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpLibSsh2SessionFreeResource() sftp shutdown failure libssh2 sftp error");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_SHUTDOWN, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_CONNECTION_LOST},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_CONNECTION_LOST},
            {.function = NULL},
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true),
            "new storage (defaults)");
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "assign storage");
        TEST_ERROR_FMT(
            memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest))), ServiceError,
            "failed to shutdown sftpSession: libssh2 errno [-31]: sftp errno [7] connection lost");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpLibSsh2SessionFreeResource() sftp shutdown failure EAGAIN");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_SHUTDOWN, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            {.function = NULL},
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true),
            "new storage (defaults)");
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "assign storage");
        TEST_ERROR_FMT(
            memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest))), ServiceError,
            "timeout shutting down sftpSession: libssh2 errno [-37]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpLibSsh2SessionFreeResource() session disconnect failure");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_SHUTDOWN, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_DISCONNECT_EX, .param = "[11,\"pgBackRest instance shutdown\",\"\"]",
             .resultInt = LIBSSH2_ERROR_SOCKET_DISCONNECT},
            {.function = NULL},
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true),
            "new storage (defaults)");
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "assign storage");
        TEST_ERROR_FMT(
            memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest))),
            ServiceError, "failed to disconnect libssh2 session: libssh2 errno [-13]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpLibSsh2SessionFreeResource() session disconnect failure EAGAIN");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_SHUTDOWN, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_DISCONNECT_EX, .param = "[11,\"pgBackRest instance shutdown\",\"\"]",
             .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            {.function = NULL},
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true),
            "new storage (defaults)");
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "assign storage");
        TEST_ERROR_FMT(
            memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest))),
            ServiceError, "timeout disconnecting libssh2 session: libssh2 errno [-37]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpLibSsh2SessionFreeResource() session free failure");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_SHUTDOWN, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_DISCONNECT_EX, .param = "[11,\"pgBackRest instance shutdown\",\"\"]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_FREE, .resultInt = LIBSSH2_ERROR_SOCKET_DISCONNECT},
            {.function = NULL},
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true),
            "new storage (defaults)");
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "assign storage");
        TEST_ERROR_FMT(
            memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest))), ServiceError,
            "failed to free libssh2 session: libssh2 errno [-13]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpLibSsh2SessionFreeResource() session free failure EAGAIN");

        hrnLibSsh2ScriptSet((HrnLibSsh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_SHUTDOWN, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_DISCONNECT_EX, .param = "[11,\"pgBackRest instance shutdown\",\"\"]",
             .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SESSION_FREE, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_BLOCK_DIRECTIONS, .resultInt = SSH2_BLOCK_READING_WRITING},
            {.function = NULL},
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .modeFile = STORAGE_MODE_FILE_DEFAULT,
                .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, repoIdx),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, repoIdx)), .write = true),
            "new storage (defaults)");
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "assign storage");
        TEST_ERROR_FMT(
            memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest))), ServiceError,
            "timeout freeing libssh2 session: libssh2 errno [-37]");
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("storageSftpEvalLibSsh2Error()"))
    {
#ifdef HAVE_LIBSSH2
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpEvalLibSsh2Error()");

        TEST_ERROR_FMT(
            storageSftpEvalLibSsh2Error(
                -11, 16, &FileRemoveError, strNewFmt("unable to move '%s' to '%s'", "BOGUS", "NOT BOGUS"), STRDEF("HINT")),
            FileRemoveError,
            "unable to move 'BOGUS' to 'NOT BOGUS': libssh2 error [-11]\n"
            "HINT");
        TEST_ERROR_FMT(
            storageSftpEvalLibSsh2Error(
                LIBSSH2_ERROR_SFTP_PROTOCOL, 16, &FileRemoveError,
                strNewFmt("unable to move '%s' to '%s'", "BOGUS", "NOT BOGUS"), STRDEF("HINT")),
            FileRemoveError,
            "unable to move 'BOGUS' to 'NOT BOGUS': libssh2 error [-31]: sftp error [16] unknown principal\n"
            "HINT");
        TEST_ERROR_FMT(
            storageSftpEvalLibSsh2Error(
                LIBSSH2_ERROR_SFTP_PROTOCOL, 16, &FileRemoveError,
                strNewFmt("unable to move '%s' to '%s'", "BOGUS", "NOT BOGUS"), NULL),
            FileRemoveError, "unable to move 'BOGUS' to 'NOT BOGUS': libssh2 error [-31]: sftp error [16] unknown principal");
        TEST_ERROR_FMT(
            storageSftpEvalLibSsh2Error(LIBSSH2_ERROR_SFTP_PROTOCOL, 16, &FileRemoveError, NULL, NULL),
            FileRemoveError, "libssh2 error [-31]: sftp error [16] unknown principal");
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("libssh2SftpErrorMsg()"))
    {
#ifdef HAVE_LIBSSH2
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("libssh2SftpErrorMsg()");

        TEST_RESULT_Z(libssh2SftpErrorMsg(LIBSSH2_FX_EOF), "eof", "LIBSSH2_FX_EOF");
        TEST_RESULT_Z(libssh2SftpErrorMsg(LIBSSH2_FX_FAILURE), "failure", "LIBSSH2_FX_FAILURE");
        TEST_RESULT_Z(libssh2SftpErrorMsg(LIBSSH2_FX_BAD_MESSAGE), "bad message", "LIBSSH2_FX_BAD_MESSAGE");
        TEST_RESULT_Z(libssh2SftpErrorMsg(LIBSSH2_FX_NO_CONNECTION), "no connection", "LIBSSH2_FX_NO_CONNECTION");
        TEST_RESULT_Z(libssh2SftpErrorMsg(LIBSSH2_FX_OP_UNSUPPORTED), "operation unsupported", "LIBSSH2_FX_OP_UNSUPPORTED");
        TEST_RESULT_Z(libssh2SftpErrorMsg(LIBSSH2_FX_INVALID_HANDLE), "invalid handle", "LIBSSH2_FX_INVALID_HANDLE");
        TEST_RESULT_Z(libssh2SftpErrorMsg(LIBSSH2_FX_NO_SUCH_PATH), "no such path", "LIBSSH2_FX_NO_SUCH_PATH");
        TEST_RESULT_Z(
            libssh2SftpErrorMsg(LIBSSH2_FX_FILE_ALREADY_EXISTS), "file already exists", "LIBSSH2_FX_FILE_ALREADY_EXISTS");
        TEST_RESULT_Z(libssh2SftpErrorMsg(LIBSSH2_FX_WRITE_PROTECT), "write protect", "LIBSSH2_FX_WRITE_PROTECT");
        TEST_RESULT_Z(libssh2SftpErrorMsg(LIBSSH2_FX_NO_MEDIA), "no media", "LIBSSH2_FX_NO_MEDIA");
        TEST_RESULT_Z(
            libssh2SftpErrorMsg(LIBSSH2_FX_NO_SPACE_ON_FILESYSTEM), "no space on filesystem", "LIBSSH2_FX_NO_SPACE_ON_FILESYSTEM");
        TEST_RESULT_Z(libssh2SftpErrorMsg(LIBSSH2_FX_QUOTA_EXCEEDED), "quota exceeded", "LIBSSH2_FX_QUOTA_EXCEEDED");
        TEST_RESULT_Z(libssh2SftpErrorMsg(LIBSSH2_FX_LOCK_CONFLICT), "lock conflict", "LIBSSH2_FX_LOCK_CONFLICT");
        TEST_RESULT_Z(libssh2SftpErrorMsg(LIBSSH2_FX_DIR_NOT_EMPTY), "directory not empty", "LIBSSH2_FX_DIR_NOT_EMPTY");
        TEST_RESULT_Z(libssh2SftpErrorMsg(LIBSSH2_FX_NOT_A_DIRECTORY), "not a directory", "LIBSSH2_FX_NOT_A_DIRECTORY");
        TEST_RESULT_Z(libssh2SftpErrorMsg(LIBSSH2_FX_INVALID_FILENAME), "invalid filename", "LIBSSH2_FX_INVALID_FILENAME");
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

#ifdef HAVE_LIBSSH2
    hrnFdReadyShimUninstall();
    hrnSckClientOpenShimUninstall();
#endif // HAVE_LIBSSH2

    FUNCTION_HARNESS_RETURN_VOID();
}
