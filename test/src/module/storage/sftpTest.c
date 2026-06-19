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
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

#ifdef HAVE_LIBSSH2
    // Set storage helper
    static const StorageHelper storageHelperList[] = {STORAGE_SFTP_HELPER, STORAGE_END_HELPER};
    storageHelperInit(storageHelperList);

    // Install shim to return defined fd
    hrnSckClientOpenShimInstall();

    // Install shim to manage true/false return for fdReady
    hrnFdReadyShimInstall();

    // Directory and file that cannot be accessed to test permissions errors
    const String *fileNoPerm = STRDEF(TEST_PATH "/noperm/noperm");
    String *pathNoPerm = strPath(fileNoPerm);

    // Common argument list
    StringList *const argCommonList = strLstNew();
    hrnCfgArgRawZ(argCommonList, cfgOptStanza, "test");
    hrnCfgArgRawZ(argCommonList, cfgOptPgPath, "/path/to/pg");
    hrnCfgArgRawZ(argCommonList, cfgOptRepoSftpHostUser, TEST_USER);
    hrnCfgArgRawZ(argCommonList, cfgOptRepoType, "sftp");
    hrnCfgArgRawZ(argCommonList, cfgOptRepoSftpHost, "localhost");
    hrnCfgArgRawZ(argCommonList, cfgOptRepoSftpHostKeyHashType, "sha1");
    hrnCfgArgRawZ(argCommonList, cfgOptRepoSftpPrivateKeyFile, KEYPRIV_CSTR);
    hrnCfgArgRawZ(argCommonList, cfgOptRepoSftpPublicKeyFile, KEYPUB_CSTR);
    hrnCfgArgRawZ(argCommonList, cfgOptRepoSftpKnownHost, KNOWNHOSTS_FILE_CSTR);
#endif // HAVE_LIBSSH2

    // *****************************************************************************************************************************
    if (testBegin("storageNew()"))
    {
#ifdef HAVE_LIBSSH2
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("ssh2 init failure");

        HRN_LIBSSH2_SCRIPT_SET(HRN_LIBSSH2_INIT(.resultInt = -1));

        TEST_ERROR(
            storageSftpNewP(
                TEST_PATH_STR, STRDEF("localhost"), 22, TEST_USER_STR, 5, KEYPRIV, hashTypeSha1, .keyPub = KEYPUB,
                .hostKeyCheckType = SFTP_STRICT_HOSTKEY_CHECKING_STRICT),
            ServiceError, "unable to init libssh2");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("driver session failure");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(.resultNull = true));

        TEST_ERROR(
            storageSftpNewP(
                TEST_PATH_STR, STRDEF("localhost"), 22, TEST_USER_STR, 5, KEYPRIV, hashTypeSha1, .keyPub = KEYPUB,
                .hostKeyCheckType = SFTP_STRICT_HOSTKEY_CHECKING_STRICT),
            ServiceError, "unable to init libssh2 session");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("handshake failure");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(.resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_WRITING),
            HRN_LIBSSH2_HANDSHAKE(.resultInt = LIBSSH2_ERROR_BAD_SOCKET),
            HRN_LIBSSH2_SESSION_ERROR(LIBSSH2_ERROR_BAD_SOCKET, .errMsg = "Bad socket provided"));

        TEST_ERROR(
            storageSftpNewP(
                TEST_PATH_STR, STRDEF("localhost"), 22, TEST_USER_STR, 1000, KEYPRIV, hashTypeSha1, .keyPub = KEYPUB,
                .hostKeyCheckType = SFTP_STRICT_HOSTKEY_CHECKING_STRICT),
            ServiceError, "libssh2 handshake failed [-45]: Bad socket provided");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("handshake failure - timeout during libssh2 handshake");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(.resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_HANDSHAKE(.resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING));

        TEST_ERROR(
            storageSftpNewP(
                TEST_PATH_STR, STRDEF("localhost"), 22, TEST_USER_STR, 100, KEYPRIV, hashTypeSha1, .keyPub = KEYPUB,
                .hostKeyCheckType = SFTP_STRICT_HOSTKEY_CHECKING_STRICT),
            ServiceError, "timeout during libssh2 handshake [-37]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("hostkey hash fail");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_HOSTKEY_HASH(2, .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SOCKET_SEND));

        TEST_ERROR(
            storageSftpNewP(
                TEST_PATH_STR, STRDEF("localhost"), 22, TEST_USER_STR, 1000, KEYPRIV, hashTypeSha1, .keyPub = KEYPUB,
                .hostFingerprint = STRDEF("3132333435363738393039383736353433323130"),
                .hostKeyCheckType = SFTP_STRICT_HOSTKEY_CHECKING_FINGERPRINT),
            ServiceError, "libssh2 hostkey hash failed: libssh2 errno [-7]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid hostkey hash");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE());

        TEST_ERROR(
            storageSftpNewP(
                TEST_PATH_STR, STRDEF("localhost"), 22, TEST_USER_STR, 20, KEYPRIV, cipherTypeAes256Cbc, .keyPub = KEYPUB,
                .write = true, .hostKeyCheckType = SFTP_STRICT_HOSTKEY_CHECKING_STRICT),
            ServiceError, "requested ssh2 hostkey hash type (aes-256-cbc) not available");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("public key from file auth failure leading - tilde key paths");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_HOSTKEY_HASH(2, .resultZ = "12345678909876543210"),
            HRN_LIBSSH2_USERAUTH(.resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_USERAUTH(.resultInt = -16),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_OK));

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
            storageRepo(), ServiceError,
            "public key authentication failed: libssh2 error [-16]\n"
            "HINT: libssh2 compiled against non-openssl libraries requires --repo-sftp-private-key-file and"
            " --repo-sftp-public-key-file to be provided\n"
            "HINT: libssh2 versions before 1.9.0 expect a PEM format keypair, try ssh-keygen -m PEM -t rsa -P \"\" to generate the"
            " keypair\n"
            "HINT: check authorization log on the SFTP server");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("fingerprint mismatch");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_HOSTKEY_HASH(2, .resultZ = "12345678909876543210"));

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
            storageRepo(), ServiceError,
            "host [3132333435363738393039383736353433323130] and configured fingerprint (repo-sftp-host-fingerprint)"
            " [9132333435363738393039383736353433323130] do not match");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("known host init failure");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(.resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_ALLOC),
            HRN_LIBSSH2_SESSION_ERROR(LIBSSH2_ERROR_ALLOC, .errMsg = "Unable to allocate memory for known-hosts collection"));

        argList = strLstDup(argCommonList);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ERROR(
            storageRepo(), ServiceError,
            "failure during libssh2_knownhost_init: libssh2 errno [-6] Unable to allocate memory for known-hosts collection");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("libssh2_session_hostkey fail - return NULL - hostKeyCheckType = yes");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = 5),
            HRN_LIBSSH2_HOSTKEY(.resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SOCKET_SEND));

        argList = strLstDup(argCommonList);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ERROR(storageRepo(), ServiceError, "libssh2_session_hostkey failed to get hostkey: libssh2 error [-7]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("knownhost_checkp failure LIBSSH2_KNOWNHOST_CHECK_MISMATCH");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = 5),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_MISMATCH));

        TEST_ERROR(
            storageRepo(), ServiceError,
            "known hosts failure: 'localhost' mismatch in known hosts files: LIBSSH2_KNOWNHOST_CHECK_MISMATCH [1]: check type"
            " [strict]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("knownhost_checkp failure LIBSSH2_KNOWNHOST_CHECK_FAILURE");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = 5),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_FAILURE));

        TEST_ERROR(
            storageRepo(), ServiceError,
            "known hosts failure: 'localhost' LIBSSH2_KNOWNHOST_CHECK_FAILURE [3]: check type [strict]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("knownhost_checkp failure LIBSSH2_KNOWNHOST_CHECK_NOTFOUND");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = 5),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_NOTFOUND));

        TEST_ERROR(
            storageRepo(), ServiceError,
            "known hosts failure: 'localhost' not found in known hosts files: LIBSSH2_KNOWNHOST_CHECK_NOTFOUND [2]: check type"
            " [strict]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read known_hosts file failure - empty files - log INFO on empty known_hosts files");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS2_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(ETC_KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(ETC_KNOWNHOSTS2_FILE_CSTR),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_NOTFOUND));

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
            storageRepo(), ServiceError,
            "known hosts failure: 'localhost' not found in known hosts files: LIBSSH2_KNOWNHOST_CHECK_NOTFOUND [2]: check type"
            " [strict]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("knownhost_checkp unknown failure type");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = 5),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(5));

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

        TEST_ERROR(storageRepo(), ServiceError, "known hosts failure: 'localhost' unknown failure [5]: check type [strict]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("public key from file auth failure, no public key");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = 5),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_MATCH),
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",null,\"" KEYPRIV_CSTR "\",null]", .resultInt = -16},
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_OK));

        TEST_ERROR(
            storageRepo(), ServiceError,
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

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = 5),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_MATCH),
            HRN_LIBSSH2_USERAUTH(.resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING));

        argList = strLstDup(argCommonList);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ERROR(storageRepo(), ServiceError, "timeout during public key authentication");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init failure ssh2 error");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = 5),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_MATCH),
            HRN_LIBSSH2_USERAUTH(),
            HRN_LIBSSH2_SFTP_INIT(.resultNull = true),
            // storageSftpWaitFd returns false
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SOCKET_SEND),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SOCKET_SEND),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_ERROR_NONE));

        TEST_ERROR(storageRepo(), ServiceError, "unable to init libssh2_sftp session");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init fail and timeout");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = 5),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_MATCH),
            HRN_LIBSSH2_USERAUTH(),
            HRN_LIBSSH2_SFTP_INIT(.resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_EAGAIN),
            // fdReady shim returns false --> storageSftpWaitFd returns false
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_EAGAIN));

        TEST_ERROR(storageRepo(), ServiceError, "timeout during init of libssh2_sftp session");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init fail no timeout");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = 5),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_MATCH),
            HRN_LIBSSH2_USERAUTH(),
            HRN_LIBSSH2_SFTP_INIT(.resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_EAGAIN),
            // fdReady shim returns true --> storageSftpWaitFd returns true
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING),
            HRN_LIBSSH2_SFTP_INIT(.resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SOCKET_SEND),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SOCKET_SEND),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_ERROR_NONE));

        TEST_ERROR(storageRepo(), ServiceError, "unable to init libssh2_sftp session");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init success");

        HRN_LIBSSH2_SCRIPT_SET(
            HRNLIBSSH2_MACRO_STARTUP(),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        TEST_RESULT_VOID(storageRepo(), "new storage");

        storageHelperFree();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("knownhost_init WARN host key checking disabled, insecure connections, hostKeyCheckType = no");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_USERAUTH(),
            HRN_LIBSSH2_SFTP_INIT(),
            HRNLIBSSH2_MACRO_SHUTDOWN());

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

        TEST_RESULT_VOID(storageRepo(), "new storage");

        storageHelperFree();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("knownhost_init WARN init fail for user's known_hosts file for update, hostKeyCheckType = accept-new");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_NOTFOUND),
            HRN_LIBSSH2_KNOWNHOST_INIT(.resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_ALLOC),
            HRN_LIBSSH2_SESSION_ERROR(LIBSSH2_ERROR_ALLOC, .errMsg = "Unable to allocate memory for known-hosts collection"),
            HRN_LIBSSH2_USERAUTH(),
            HRN_LIBSSH2_SFTP_INIT(),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        argList = strLstDup(argCommonList);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "accept-new");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_RESULT_VOID(storageRepo(), "new storage");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: libssh2_knownhost_init failed for '/home/" TEST_USER "/.ssh/known_hosts' for update: libssh2 errno [-6] "
            "Unable to allocate memory for known-hosts collection");

        storageHelperFree();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("knownhost_init WARN missing user's known_host file for update - hostKeyCheckType = accept-new");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_NOTFOUND),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = LIBSSH2_ERROR_FILE),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = LIBSSH2_ERROR_FILE),
            HRN_LIBSSH2_SESSION_ERROR(LIBSSH2_ERROR_FILE, .errMsg = "Failed to open file"),
            HRN_LIBSSH2_USERAUTH(),
            HRN_LIBSSH2_SFTP_INIT(),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        argList = strLstDup(argCommonList);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "accept-new");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_RESULT_VOID(storageRepo(), "new storage");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: libssh2 unable to read '/home/" TEST_USER "/.ssh/known_hosts' for update: libssh2 errno [-16] Failed "
            "to open file\n"
            "            HINT: does '/home/" TEST_USER "/.ssh/known_hosts' exist with proper permissions?");

        storageHelperFree();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("knownhost_init WARN unable to read user's known_host file for update - hostKeyCheckType = accept-new");

        // Create known_hosts file so it can be found by storageExistsP(), i.e. file exists but is unreadable
        HRN_SYSTEM_FMT("touch %s", strZ(strNewFmt("%s/.ssh/known_hosts", strZ(userHome()))));

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_NOTFOUND),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = LIBSSH2_ERROR_FILE),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = LIBSSH2_ERROR_FILE),
            HRN_LIBSSH2_SESSION_ERROR(LIBSSH2_ERROR_FILE, .errMsg = "Failed to open file"),
            HRN_LIBSSH2_USERAUTH(),
            HRN_LIBSSH2_SFTP_INIT(),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        TEST_RESULT_VOID(storageRepo(), "new storage");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: libssh2 unable to read '/home/" TEST_USER "/.ssh/known_hosts' for update: libssh2 errno [-16] Failed "
            "to open file\n"
            "            HINT: does '/home/" TEST_USER "/.ssh/known_hosts' exist with proper permissions?");

        // Remove known_hosts file
        HRN_SYSTEM_FMT("rm %s", strZ(strNewFmt("%s/.ssh/known_hosts", strZ(userHome()))));

        storageHelperFree();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("knownhost_init WARN fail to load user's known_host file with error other than LIBSSH2_ERROR_FILE");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_NOTFOUND),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = LIBSSH2_ERROR_KNOWN_HOSTS),
            HRN_LIBSSH2_SESSION_ERROR(LIBSSH2_ERROR_KNOWN_HOSTS, .errMsg = "Failed to parse known hosts file"),
            HRN_LIBSSH2_USERAUTH(),
            HRN_LIBSSH2_SFTP_INIT(),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        TEST_RESULT_VOID(storageRepo(), "new storage");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: libssh2 unable to read '/home/" TEST_USER "/.ssh/known_hosts' for update: libssh2 errno [-46] Failed to "
            "parse known hosts file\n"
            "            HINT: does '/home/" TEST_USER "/.ssh/known_hosts' exist with proper permissions?");

        storageHelperFree();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init success - hostKeyCheckType = accept-new - add host to user's known_hosts file RSA");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS2_FILE_CSTR, .resultInt = LIBSSH2_ERROR_FILE),
            HRN_LIBSSH2_SESSION_ERROR(LIBSSH2_ERROR_FILE, .errMsg = "Failed to open file"),
            HRN_LIBSSH2_KNOWNHOST_READFILE(ETC_KNOWNHOSTS_FILE_CSTR, .resultInt = LIBSSH2_ERROR_FILE),
            HRN_LIBSSH2_SESSION_ERROR(LIBSSH2_ERROR_FILE, .errMsg = "Failed to open file"),
            HRN_LIBSSH2_KNOWNHOST_READFILE(ETC_KNOWNHOSTS2_FILE_CSTR, .resultInt = LIBSSH2_ERROR_FILE),
            HRN_LIBSSH2_SESSION_ERROR(LIBSSH2_ERROR_FILE),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_NOTFOUND),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_ADD(589825),
            HRN_LIBSSH2_KNOWNHOST_WRITEFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_USERAUTH(),
            HRN_LIBSSH2_SFTP_INIT(),
            HRNLIBSSH2_MACRO_SHUTDOWN());

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

        harnessLogLevelSet(logLevelDetail);

        TEST_RESULT_VOID(storageRepo(), "new storage");
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
        storageHelperFree();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init success - hostKeyCheckType = accept-new - add host to user's known_hosts file DSS");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS2_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(ETC_KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(ETC_KNOWNHOSTS2_FILE_CSTR),
            HRN_LIBSSH2_HOSTKEY(.type = LIBSSH2_HOSTKEY_TYPE_DSS),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_NOTFOUND),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_ADD(851969),
            HRN_LIBSSH2_KNOWNHOST_WRITEFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_USERAUTH(),
            HRN_LIBSSH2_SFTP_INIT(),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        TEST_RESULT_VOID(storageRepo(), "new storage");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: pgBackRest added new host 'localhost' to '/home/" TEST_USER "/.ssh/known_hosts'");

        storageHelperFree();

        // -------------------------------------------------------------------------------------------------------------------------
#ifdef LIBSSH2_HOSTKEY_TYPE_ECDSA_256
        TEST_TITLE("sftp session init success - hostKeyCheckType = accept-new - add host to user's known_hosts ECDSA_256");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS2_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(ETC_KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(ETC_KNOWNHOSTS2_FILE_CSTR),
            HRN_LIBSSH2_HOSTKEY(.type = LIBSSH2_HOSTKEY_TYPE_ECDSA_256),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_NOTFOUND),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_ADD(1114113),
            HRN_LIBSSH2_KNOWNHOST_WRITEFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_USERAUTH(),
            HRN_LIBSSH2_SFTP_INIT(),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        TEST_RESULT_VOID(storageRepo(), "new storage");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: pgBackRest added new host 'localhost' to '/home/" TEST_USER "/.ssh/known_hosts'");

        storageHelperFree();
#endif

        // -------------------------------------------------------------------------------------------------------------------------
#ifdef LIBSSH2_HOSTKEY_TYPE_ECDSA_384
        TEST_TITLE("sftp session init success - hostKeyCheckType = accept-new - add host to user's known_hosts ECDSA_384");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS2_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(ETC_KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(ETC_KNOWNHOSTS2_FILE_CSTR),
            HRN_LIBSSH2_HOSTKEY(.type = LIBSSH2_HOSTKEY_TYPE_ECDSA_384),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_NOTFOUND),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_ADD(1376257),
            HRN_LIBSSH2_KNOWNHOST_WRITEFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_USERAUTH(),
            HRN_LIBSSH2_SFTP_INIT(),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        TEST_RESULT_VOID(storageRepo(), "new storage");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: pgBackRest added new host 'localhost' to '/home/" TEST_USER "/.ssh/known_hosts'");

        storageHelperFree();
#endif

        // -------------------------------------------------------------------------------------------------------------------------
#ifdef LIBSSH2_HOSTKEY_TYPE_ECDSA_521
        TEST_TITLE("sftp session init success - hostKeyCheckType = accept-new - add host to user's known_hosts ECDSA_521");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS2_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(ETC_KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(ETC_KNOWNHOSTS2_FILE_CSTR),
            HRN_LIBSSH2_HOSTKEY(.type = LIBSSH2_HOSTKEY_TYPE_ECDSA_521),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_NOTFOUND),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_ADD(1638401),
            HRN_LIBSSH2_KNOWNHOST_WRITEFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_USERAUTH(),
            HRN_LIBSSH2_SFTP_INIT(),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        TEST_RESULT_VOID(storageRepo(), "new storage");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: pgBackRest added new host 'localhost' to '/home/" TEST_USER "/.ssh/known_hosts'");

        storageHelperFree();
#endif

        // -------------------------------------------------------------------------------------------------------------------------
#ifdef LIBSSH2_HOSTKEY_TYPE_ED25519
        TEST_TITLE("sftp session init success - hostKeyCheckType = accept-new - add host to user's known_hosts ED25519");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS2_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(ETC_KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(ETC_KNOWNHOSTS2_FILE_CSTR),
            HRN_LIBSSH2_HOSTKEY(.type = LIBSSH2_HOSTKEY_TYPE_ED25519),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_NOTFOUND),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_ADD(1900545),
            HRN_LIBSSH2_KNOWNHOST_WRITEFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_USERAUTH(),
            HRN_LIBSSH2_SFTP_INIT(),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        TEST_RESULT_VOID(storageRepo(), "new storage");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: pgBackRest added new host 'localhost' to '/home/" TEST_USER "/.ssh/known_hosts'");

        storageHelperFree();
#endif

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init WARN - hostKeyCheckType = accept-new - fail to write user's known_hosts file");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS2_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(ETC_KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(ETC_KNOWNHOSTS2_FILE_CSTR),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_NOTFOUND),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_ADD(589825),
            HRN_LIBSSH2_KNOWNHOST_WRITEFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = LIBSSH2_ERROR_FILE),
            HRN_LIBSSH2_USERAUTH(),
            HRN_LIBSSH2_SFTP_INIT(),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        TEST_RESULT_VOID(storageRepo(), "new storage");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: pgBackRest unable to write '/home/" TEST_USER "/.ssh/known_hosts' for update");

        storageHelperFree();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init WARN - hostKeyCheckType = accept-new, add to user's known_hosts - unknown key type");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS2_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(ETC_KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(ETC_KNOWNHOSTS2_FILE_CSTR),
            HRN_LIBSSH2_HOSTKEY(.type = 9),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_NOTFOUND),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_USERAUTH(),
            HRN_LIBSSH2_SFTP_INIT(),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        TEST_RESULT_VOID(storageRepo(), "new storage");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: unsupported key type [9], unable to update knownhosts for 'localhost'");

        storageHelperFree();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init WARN - hostKeyCheckType = accept-new - add host to user's known_hosts failure");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS2_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(ETC_KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(ETC_KNOWNHOSTS2_FILE_CSTR),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_NOTFOUND),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_ADD(589825, .resultInt = LIBSSH2_ERROR_KNOWN_HOSTS),
            HRN_LIBSSH2_USERAUTH(),
            HRN_LIBSSH2_SFTP_INIT(),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        TEST_RESULT_VOID(storageRepo(), "new storage");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: pgBackRest failed to add 'localhost' to known_hosts internal list");

        storageHelperFree();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("knownhost_checkp failure LIBSSH2_KNOWNHOST_CHECK_FAILURE hostKeyCheckType = accept-new");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = 5),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_FAILURE));

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
            storageRepo(), ServiceError,
            "known hosts failure: 'localhost': LIBSSH2_KNOWNHOST_CHECK_FAILURE [3]: check type [accept-new]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("knownhost_checkp failure LIBSSH2_KNOWNHOST_CHECK_MISMATCH hostKeyCheckType = accept-new");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = 5),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_MISMATCH));

        TEST_ERROR(
            storageRepo(), ServiceError,
            "known hosts failure: 'localhost': mismatch in known hosts files: LIBSSH2_KNOWNHOST_CHECK_MISMATCH [1]: check type"
            " [accept-new]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init success LIBSSH2_KNOWNHOST_CHECK_MISMATCH hostKeyCheckType = accept-new");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = 5),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_MATCH),
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",null,\"" KEYPRIV_CSTR "\",null]", .resultInt = 0},
            HRN_LIBSSH2_SFTP_INIT(),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        Storage *storageTest;

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                cfgOptionIdxStr(cfgOptRepoPath, 0), cfgOptionIdxStr(cfgOptRepoSftpHost, 0),
                cfgOptionIdxUInt(cfgOptRepoSftpHostPort, 0), cfgOptionIdxStr(cfgOptRepoSftpHostUser, 0),
                cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, 0),
                cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, 0), .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, 0),
                .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, 0),
                .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, 0),
                .hostKeyCheckType = cfgOptionIdxStrId(cfgOptRepoSftpHostKeyCheckType, 0),
                .knownHosts = strLstNewVarLst(cfgOptionIdxLst(cfgOptRepoSftpKnownHost, 0)), .write = true,
                .pathExpressionFunction = storageRepoPathExpression),
            "new storage");

        TEST_RESULT_UINT(storageTest->modeFile, 0640, "check file mode");
        TEST_RESULT_UINT(storageTest->modePath, 0750, "check path mode");
        TEST_RESULT_PTR(storageTest->pathExpressionFunction, storageRepoPathExpression, "check expression");
        TEST_RESULT_BOOL(storageTest->write, true, "check write");

        objFree(storageTest);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init WARN - hostKeyCheckType = accept-new - add host to user's known_hosts");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS2_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(ETC_KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_READFILE(ETC_KNOWNHOSTS2_FILE_CSTR),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_NOTFOUND),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_KNOWNHOST_ADD(589825),
            HRN_LIBSSH2_KNOWNHOST_WRITEFILE(KNOWNHOSTS_FILE_CSTR),
            HRN_LIBSSH2_USERAUTH(),
            HRN_LIBSSH2_SFTP_INIT(),
            HRNLIBSSH2_MACRO_SHUTDOWN());

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

        TEST_RESULT_VOID(storageRepo(), "new storage");
        TEST_RESULT_LOG(
            "P00   WARN: host 'localhost' not found in known hosts files, attempting to add host to "
            "'/home/" TEST_USER "/.ssh/known_hosts'\n"
            "P00   WARN: pgBackRest added new host 'localhost' to '/home/" TEST_USER "/.ssh/known_hosts'");

        storageHelperFree();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session failure - libssh2_session_hostkey fail - hostKeyCheckType = accept-new");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = 5),
            HRN_LIBSSH2_HOSTKEY(.resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SOCKET_SEND));

        argList = strLstDup(argCommonList);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "accept-new");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ERROR(storageRepo(), ServiceError, "libssh2_session_hostkey failed to get hostkey: libssh2 error [-7]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init success timeout");

        // Shim sets FD for tests.
        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = 5),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_MATCH),
            HRN_LIBSSH2_USERAUTH(),
            HRN_LIBSSH2_SFTP_INIT(),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        argList = strLstDup(argCommonList);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "accept-new");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_RESULT_VOID(storageRepo(), "new storage");

        storageHelperFree();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create new storage with defaults && a single known_hosts file with leading tilde");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE("/home/" TEST_USER "/.ssh/pgbackrest_known_hosts", .resultInt = 5),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_MATCH),
            HRN_LIBSSH2_USERAUTH(),
            HRN_LIBSSH2_SFTP_INIT(),
            HRNLIBSSH2_MACRO_SHUTDOWN());

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

        TEST_RESULT_VOID(storageRepo(), "new storage");
        TEST_RESULT_STR_Z(
            strLstGet(strLstNewVarLst(cfgOptionLst(cfgOptRepoSftpKnownHost)), 0), "~/.ssh/pgbackrest_known_hosts",
            "check known hosts path");

        storageHelperFree();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create new storage - override defaults");

        HRN_LIBSSH2_SCRIPT_SET(
            HRNLIBSSH2_MACRO_STARTUP(),
            HRNLIBSSH2_MACRO_SHUTDOWN());

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

        TEST_RESULT_VOID(storageRepoWrite(), "new storage");

        storageHelperFree();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("public key from file auth failure, key passphrase");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = 5),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_MATCH),
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
             .param = "[\"" TEST_USER "\"," TEST_USER_LEN ",\"" KEYPUB_CSTR "\",\"" KEYPRIV_CSTR "\",\"keyPassphrase\"]",
             .resultInt = -16},
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_OK));

        argList = strLstDup(argCommonList);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgEnvKeyRawZ(cfgOptRepoSftpPrivateKeyPassphrase, 1, "keyPassphrase");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ERROR(
            storageRepo(),
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
        HRN_LIBSSH2_SCRIPT_SET(HRNLIBSSH2_MACRO_STARTUP());

        StringList *argList = strLstDup(argCommonList);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "strict");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        const Storage *storageTest = storageRepoWrite();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file missing");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_STAT(TEST_PATH "/missing", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE));

        TEST_RESULT_BOOL(storageExistsP(storageTest, STRDEF("missing")), false, "file does not exist");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file missing with timeout");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_STAT(TEST_PATH "/missing", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE, .sleep = 100),
            HRN_LIBSSH2_STAT(TEST_PATH "/missing", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE),
            HRN_LIBSSH2_STAT(TEST_PATH "/missing", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE));

        TEST_RESULT_BOOL(storageExistsP(storageTest, STRDEF("missing"), .timeout = 100), false, "file does not exist with timeout");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path missing");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_STAT(TEST_PATH "/missing", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE),);

        TEST_RESULT_BOOL(storagePathExistsP(storageTest, STRDEF("missing")), false, "path does not exist");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("permission denied");

        HRN_LIBSSH2_SCRIPT_SET(
            // Permission denied
            HRN_LIBSSH2_STAT(TEST_PATH "/noperm/noperm", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_PERMISSION_DENIED),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_PERMISSION_DENIED));

        TEST_ERROR_FMT(
            storagePathExistsP(storageTest, fileNoPerm),
            FileOpenError,
            STORAGE_ERROR_INFO ": libssh2 error [-31]: sftp error [3] permission denied", strZ(fileNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file exists");

        HRN_LIBSSH2_SCRIPT_SET(HRN_LIBSSH2_STAT(TEST_PATH "/exists", .flags = HRN_LIBSSH2_ATTR_EXISTENCE));

        TEST_RESULT_BOOL(storageExistsP(storageTest, STRDEF(TEST_PATH "/exists")), true, "file exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path exists");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_STAT(TEST_PATH "/pathExists", .attr = HRN_LIBSSH2_DIR(0), .flags = HRN_LIBSSH2_ATTR_EXISTENCE));

        TEST_RESULT_BOOL(storagePathExistsP(storageTest, STRDEF(TEST_PATH "/pathExists")), true, "path exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file exists after wait");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_STAT(TEST_PATH "/exists", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE),
            HRN_LIBSSH2_STAT(TEST_PATH "/exists", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE),
            HRN_LIBSSH2_STAT(TEST_PATH "/exists", .flags = HRN_LIBSSH2_ATTR_EXISTENCE),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                sleepMSec(250);
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                TEST_RESULT_BOOL(
                    storageExistsP(storageTest, STRDEF(TEST_PATH "/exists"), .timeout = 1000), true, "file exists after wait");
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

        storageHelperFree();
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("storageInfo()"))
    {
#ifdef HAVE_LIBSSH2
        // File open error via permission denied sftp error
        HRN_LIBSSH2_SCRIPT_SET(
            HRNLIBSSH2_MACRO_STARTUP(),
            HRN_LIBSSH2_STAT(TEST_PATH "/noperm/noperm", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_PERMISSION_DENIED),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_PERMISSION_DENIED));

        // Create storage object for testing
        Storage *storageTest = storageSftpHelper(0, true, storageRepoPathExpression);

        TEST_ERROR_FMT(
            storageInfoP(storageTest, fileNoPerm),
            FileOpenError,
            STORAGE_ERROR_INFO ": libssh2 error [-31]: sftp error [3] permission denied", strZ(fileNoPerm));

        // File open error via ssh error - covers storageSftpLibSsh2FxNoSuchFile where rc != LIBSSH2_ERROR_SFTP_PROTOCOL
        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_STAT(TEST_PATH "/noperm/noperm", .resultInt = LIBSSH2_ERROR_INVAL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_ERROR_NONE));

        TEST_ERROR_FMT(
            storageInfoP(storageTest, fileNoPerm), FileOpenError, STORAGE_ERROR_INFO ": libssh2 error [-34]", strZ(fileNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file does not exist");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_STAT(TEST_PATH "/fileinfo", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE));

        TEST_RESULT_BOOL(
            storageInfoP(
                storageTest, STRDEF(TEST_PATH "/fileinfo"), .ignoreMissing = true).exists, false, "get file info (missing)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info outside of base path");

        HRN_LIBSSH2_SCRIPT_SET(HRN_LIBSSH2_STAT("/etc", .flags = HRN_LIBSSH2_ATTR_EXISTENCE));

        TEST_RESULT_BOOL(
            storageInfoP(storageTest, STRDEF("/etc"), .ignoreMissing = true, .noPathEnforce = true).exists, true,
            "path not enforced");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info - path");

        HRN_LIBSSH2_SCRIPT_SET(HRN_LIBSSH2_STAT(TEST_PATH, .attr = HRN_LIBSSH2_DIR(0770), .mtime = 1555160000));

        StorageInfo info;

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

        HRN_LIBSSH2_SCRIPT_SET(HRN_LIBSSH2_STAT(TEST_PATH, .attr = HRN_LIBSSH2_DIR(0770), .mtime = 1555160000));

        TEST_ASSIGN(info, storageInfoP(storageTest, TEST_PATH_STR, .level = storageInfoLevelExists), "path exists");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_UINT(info.size, 0, "check exists");
        TEST_RESULT_INT(info.timeModified, 0, "check time");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info - file");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_STAT(
                TEST_PATH "/fileinfo", .attr = HRN_LIBSSH2_FILE(0640), .mtime = 1555155555, .uid = 99999, .gid = 99999,
                .filesize = 8));

        TEST_ASSIGN(info, storageInfoP(storageTest, STRDEF(TEST_PATH "/fileinfo")), "get file info");
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

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_STAT(TEST_PATH "/testlink", .attr = HRN_LIBSSH2_LINK(0777)),
            HRN_LIBSSH2_READLINK(TEST_PATH "/testlink", .target = STRDEF("/tmp")));

        const String *linkName = STRDEF(TEST_PATH "/testlink");

        TEST_ASSIGN(info, storageInfoP(storageTest, linkName), "get link info");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypeLink, "check type");
        TEST_RESULT_UINT(info.size, 0, "check size");
        TEST_RESULT_INT(info.mode, 0777, "check mode");
        TEST_RESULT_STR_Z(info.linkDestination, "/tmp", "check link destination");
        TEST_RESULT_STR(info.user, NULL, "check user");
        TEST_RESULT_STR(info.group, NULL, "check group");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info - link, followLink = true");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_STAT(TEST_PATH "/testlink", .resultInt = LIBSSH2_ERROR_EAGAIN, .follow = true),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_STAT(
                TEST_PATH "/testlink", .follow = true, .attr = HRN_LIBSSH2_DIR(0777), .uid = HRN_LIBSSH2_OWNER_ROOT,
                .gid = HRN_LIBSSH2_OWNER_ROOT));

        TEST_ASSIGN(info, storageInfoP(storageTest, linkName, .followLink = true), "get info from path pointed to by link");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypePath, "check type");
        TEST_RESULT_UINT(info.size, 0, "check size");
        TEST_RESULT_INT(info.mode, 0777, "check mode");
        TEST_RESULT_STR(info.linkDestination, NULL, "check link destination");
        TEST_RESULT_STR_Z(info.user, NULL, "check user");
        TEST_RESULT_STR_Z(info.group, NULL, "check group");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info - link, followLink = false, stat timeout EAGAIN");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_STAT(TEST_PATH "/testlink", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING));

        TEST_ERROR_FMT(
            storageInfoP(storageTest, linkName, .followLink = false), FileOpenError, "timeout opening '" TEST_PATH "/testlink'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info - link, followLink = false, symlink destination timeout EAGAIN");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_STAT(TEST_PATH "/testlink", .attr = HRN_LIBSSH2_LINK(0777)),
            HRN_LIBSSH2_READLINK(TEST_PATH "/testlink", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING));

        TEST_ERROR_FMT(
            storageInfoP(storageTest, linkName, .followLink = false), FileReadError,
            "timeout getting destination for link '" TEST_PATH "/testlink'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info - link, followLink = false, symlink destination fail (link loop)");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_STAT(TEST_PATH "/testlink", .attr = HRN_LIBSSH2_LINK(0777)),
            HRN_LIBSSH2_READLINK(TEST_PATH "/testlink", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_READLINK(TEST_PATH "/testlink", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_LINK_LOOP));

        TEST_ERROR_FMT(
            storageInfoP(storageTest, linkName, .followLink = false), FileReadError,
            "unable to get destination for link '" TEST_PATH "/testlink': libssh2 error [-31]: sftp error [21] link loop");

        // --------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info - pipe");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_STAT(TEST_PATH "/testpipe", .attr = HRN_LIBSSH2_FIFO(0666)),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        TEST_ASSIGN(info, storageInfoP(storageTest, STRDEF(TEST_PATH "/testpipe")), "get info from pipe (special file)");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypeSpecial, "check type");
        TEST_RESULT_UINT(info.size, 0, "check size");
        TEST_RESULT_INT(info.mode, 0666, "check mode");
        TEST_RESULT_STR(info.linkDestination, NULL, "check link destination");
        TEST_RESULT_STR(info.user, NULL, "check user");
        TEST_RESULT_STR(info.group, NULL, "check group");

        objFree(storageTest);
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNewItrP()"))
    {
#ifdef HAVE_LIBSSH2
        HRN_LIBSSH2_SCRIPT_SET(HRNLIBSSH2_MACRO_STARTUP());

        StringList *argList = strLstDup(argCommonList);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHostKeyCheckType, "strict");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        Storage *storageTest = storageSftpHelper(0, false, storageRepoPathExpression);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path missing, errorOnMissing");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/BOGUS", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/BOGUS", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_NONE),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE));

        TEST_ERROR_FMT(
            storageNewItrP(storageTest, STRDEF(BOGUS_STR), .errorOnMissing = true), PathMissingError,
            STORAGE_ERROR_LIST_INFO_MISSING, TEST_PATH "/BOGUS");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path missing, ignore missing dir");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/BOGUS", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_NONE),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE));

        TEST_RESULT_PTR(storageNewItrP(storageTest, STRDEF(BOGUS_STR), .nullOnMissing = true), NULL, "ignore missing dir");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path no perm");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/noperm", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_PERMISSION_DENIED),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_PERMISSION_DENIED));

        TEST_ERROR_FMT(
            storageNewItrP(storageTest, pathNoPerm, .errorOnMissing = true), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": libssh2 error [-31]: sftp error [3] permission denied", strZ(pathNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path no perm, ignore missing");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/noperm", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_PERMISSION_DENIED),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_PERMISSION_DENIED));

        TEST_ERROR_FMT(
            storageNewItrP(storageTest, pathNoPerm), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": libssh2 error [-31]: sftp error [3] permission denied", strZ(pathNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path with only dot");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_STAT(TEST_PATH "/pg", .attr = HRN_LIBSSH2_DIR(0766), .mtime = 1555160000),
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/pg"),
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg", .attr = HRN_LIBSSH2_DIR(0766), .mtime = 1555160000));

        // NOTE: if operating against an actual sftp server, a neutral umask is required to get the proper permissions.
        // Without the neutral umask, permissions were 0764.
        TEST_STORAGE_LIST(
            storageTest, "pg",
            "./ {u=" TEST_USER_ID_Z ", g=" TEST_GROUP_ID_Z ", m=0766}\n",
            .level = storageInfoLevelDetail, .includeDot = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path with file, link, pipe");

        HRN_LIBSSH2_SCRIPT_SET(
            // Path with file, link, pipe, dot dotdot
            HRN_LIBSSH2_STAT(TEST_PATH "/pg", .attr = HRN_LIBSSH2_DIR(0766), .mtime = 1555160000),
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/pg"),
            // readdir returns .
            HRN_LIBSSH2_READDIR("."),
            // readdir returns ..
            HRN_LIBSSH2_READDIR(".."),
            // readdir returns .include
            HRN_LIBSSH2_READDIR(".include"),
            HRN_LIBSSH2_STAT(
                TEST_PATH "/pg/.include", .attr = HRN_LIBSSH2_DIR(0755), .mtime = 1555160000, .uid = 77777, .gid = 77777),
            // readdir returns file
            HRN_LIBSSH2_READDIR("file"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/file", .attr = HRN_LIBSSH2_FILE(0660), .mtime = 1656433838, .filesize = 8),
            // readdir returns link
            HRN_LIBSSH2_READDIR("link"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/link", .attr = HRN_LIBSSH2_LINK(0764), .mtime = 1656433838),
            HRN_LIBSSH2_READLINK(TEST_PATH "/pg/link", .target = STRDEF("../file")),
            // readdir returns pipe
            HRN_LIBSSH2_READDIR("pipe"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/pipe", .attr = HRN_LIBSSH2_FIFO(0764), .mtime = 1656433838),
            // readdir returns empty
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            // readdir returns empty
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/pg/.include"),
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            // link stat
            HRN_LIBSSH2_STAT(TEST_PATH "/pg", .attr = HRN_LIBSSH2_DIR(0766), .mtime = 1555160000),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/link", .attr = HRN_LIBSSH2_LINK(0764), .mtime = 1656433838),
            HRN_LIBSSH2_READLINK(TEST_PATH "/pg/link", .target = STRDEF("../file")));

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

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_STAT("pg/missing", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_STAT("pg/missing", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE));

        TEST_RESULT_VOID(
            storageSftpListEntry(
                (StorageSftp *)storageDriver(storageTest), storageLstNew(storageInfoLevelBasic), STRDEF("pg"), "missing",
                storageInfoLevelBasic),
            "missing path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageItrMore() twice in a row");

        StorageIterator *storageItr = NULL;

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/pg"),
            HRN_LIBSSH2_READDIR("."),
            HRN_LIBSSH2_READDIR(".include"),
            HRN_LIBSSH2_STAT(
                TEST_PATH "/pg/.include", .attr = HRN_LIBSSH2_DIR(0755), .mtime = 1555160000, .uid = 77777, .gid = 77777),
            // readdir returns file
            HRN_LIBSSH2_READDIR("file"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/file", .attr = HRN_LIBSSH2_FILE(0660), .mtime = 1656433838, .filesize = 8),
            // readdir returns link
            HRN_LIBSSH2_READDIR("link"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/link", .attr = HRN_LIBSSH2_LINK(0764), .mtime = 1656433838),
            HRN_LIBSSH2_READLINK(TEST_PATH "/pg/link", .target = STRDEF("../file")),
            // readdir returns pipe
            HRN_LIBSSH2_READDIR("pipe"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/pipe", .attr = HRN_LIBSSH2_FIFO(0764), .mtime = 1656433838),
            // readdir returns empty
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE());

        // Create storage object for testing
        TEST_ASSIGN(storageItr, storageNewItrP(storageTest, STRDEF("pg")), "new iterator");
        TEST_RESULT_BOOL(storageItrMore(storageItr), true, "check more");
        TEST_RESULT_BOOL(storageItrMore(storageItr), true, "check more again");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path - recurse desc");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_STAT(TEST_PATH "/pg", .attr = HRN_LIBSSH2_DIR(0764), .mtime = 1555160000),
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/pg"),
            HRN_LIBSSH2_READDIR("."),
            HRN_LIBSSH2_READDIR("path"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/path", .attr = HRN_LIBSSH2_DIR(0755), .mtime = 1555160000, .uid = 77777, .gid = 77777),
            // readdir returns file
            HRN_LIBSSH2_READDIR("file"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/file", .attr = HRN_LIBSSH2_FILE(0660), .mtime = 1656433838, .filesize = 8),
            // readdir returns link
            HRN_LIBSSH2_READDIR("link"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/link", .attr = HRN_LIBSSH2_LINK(0764), .mtime = 1656433838),
            // readdir returns pipe
            HRN_LIBSSH2_READDIR("pipe"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/pipe", .attr = HRN_LIBSSH2_FIFO(0764), .mtime = 1656433838),
            // readdir returns empty
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            // Recurse readdir path
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/pg/path"),
            // readdir returns file
            HRN_LIBSSH2_READDIR("file"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/path/file", .attr = HRN_LIBSSH2_FILE(0660), .mtime = 1656434296, .filesize = 8),
            // readdir returns empty
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            // link stat
            HRN_LIBSSH2_STAT(TEST_PATH "/pg", .attr = HRN_LIBSSH2_DIR(0764), .mtime = 1555160000),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/link", .attr = HRN_LIBSSH2_LINK(0764), .mtime = 1656433838),
            HRN_LIBSSH2_READLINK(TEST_PATH "/pg/link", .target = STRDEF("../file")));

        TEST_STORAGE_LIST(
            storageTest, "pg",
            "pipe*\n"
            "path/file {s=8, t=1656434296}\n"
            "path/\n"
            "link> {d=../file}\n"
            "file {s=8, t=1656433838}\n"
            "./\n",
            .level = storageInfoLevelBasic, .includeDot = true, .sortOrder = sortOrderDesc);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path - recurse asc");

        // Create a path with a subpath that will always be last to make sure lists are not freed too early in the iterator
        // Mimic creation of "pg/zzz/yyy" mode 0700
        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_STAT(TEST_PATH "/pg", .attr = HRN_LIBSSH2_DIR(0764), .mtime = 1555160000),
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/pg"),
            // readdir returns dot
            HRN_LIBSSH2_READDIR("."),
            // readdir returns path
            HRN_LIBSSH2_READDIR("path"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/path", .attr = HRN_LIBSSH2_DIR(0755), .mtime = 1555160000, .uid = 77777, .gid = 77777),
            // readdir returns file
            HRN_LIBSSH2_READDIR("file"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/file", .attr = HRN_LIBSSH2_FILE(0660), .mtime = 1656433838, .filesize = 8),
            // readdir returns link
            HRN_LIBSSH2_READDIR("link"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/link", .attr = HRN_LIBSSH2_LINK(0764), .mtime = 1656433838),
            // readdir returns pipe
            HRN_LIBSSH2_READDIR("pipe"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/pipe", .attr = HRN_LIBSSH2_FIFO(0764), .mtime = 1656433838),
            // readdir returns zzz
            HRN_LIBSSH2_READDIR("zzz"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/zzz", .attr = HRN_LIBSSH2_DIR(0764), .mtime = 1656433838),
            // readdir return empty
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            // Recurse readdir path
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/pg/path"),
            // readdir returns file
            HRN_LIBSSH2_READDIR("file"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/path/file", .attr = HRN_LIBSSH2_FILE(0660), .mtime = 1656434296, .filesize = 8),
            // readdir returns empty
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            // Recurse zzz
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/pg/zzz"),
            // readdir returns yyy
            HRN_LIBSSH2_READDIR("yyy"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/zzz/yyy", .attr = HRN_LIBSSH2_DIR(0764), .mtime = 1656433838),
            // readdir returns empty
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            // Recurse yyy
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/pg/zzz/yyy"),
            // readdir returns empty
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            // link stat
            HRN_LIBSSH2_STAT(TEST_PATH "/pg", .attr = HRN_LIBSSH2_DIR(0764), .mtime = 1555160000),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/link", .attr = HRN_LIBSSH2_LINK(0764), .mtime = 1656433838),
            HRN_LIBSSH2_READLINK(TEST_PATH "/pg/link", .target = STRDEF("../file")));

        // Create storage object for testing
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

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("empty path - filter");

        // Mimic "pg/empty" mode 0700
        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_STAT(TEST_PATH "/pg", .attr = HRN_LIBSSH2_DIR(0764), .mtime = 1555160000),
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/pg"),
            // readdir returns dot
            HRN_LIBSSH2_READDIR("."),
            // readdir returns empty
            HRN_LIBSSH2_READDIR("empty"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/empty", .attr = HRN_LIBSSH2_DIR(0755), .mtime = 1555160000, .uid = 77777, .gid = 77777),
            // readdir returns path
            HRN_LIBSSH2_READDIR("path"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/path", .attr = HRN_LIBSSH2_DIR(0755), .mtime = 1555160000, .uid = 77777, .gid = 77777),
            // readdir returns file
            HRN_LIBSSH2_READDIR("file"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/file", .attr = HRN_LIBSSH2_FILE(0660), .mtime = 1656433838, .filesize = 8),
            // readdir returns link
            HRN_LIBSSH2_READDIR("link"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/link", .attr = HRN_LIBSSH2_LINK(0764), .mtime = 1656433838),
            // readdir returns pipe
            HRN_LIBSSH2_READDIR("pipe"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/pipe", .attr = HRN_LIBSSH2_FIFO(0764), .mtime = 1656433838),
            // readdir returns zzz
            HRN_LIBSSH2_READDIR("zzz"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/zzz", .attr = HRN_LIBSSH2_DIR(0764), .mtime = 1656433838),
            // readdir return empty
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            // Recurse empty
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/pg/empty"),
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            // Recurse path
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/pg/path"),
            // readdir returns file
            HRN_LIBSSH2_READDIR("file"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/path/file", .attr = HRN_LIBSSH2_FILE(0660), .mtime = 1656434296, .filesize = 8),
            // readdir returns empty
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            // Recurse zzz
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/pg/zzz"),
            // readdir returns yyy
            HRN_LIBSSH2_READDIR("yyy"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/zzz/yyy", .attr = HRN_LIBSSH2_DIR(0764), .mtime = 1656433838),
            // readdir returns empty
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            // Recurse yyy
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/pg/zzz/yyy"),
            // readdir returns empty
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE());

        // Create storage object for testing
        TEST_STORAGE_LIST(
            storageTest, "pg",
            "empty/\n",
            .level = storageInfoLevelType, .expression = "^empty");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("filter in subpath during recursion");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_STAT(TEST_PATH "/pg", .attr = HRN_LIBSSH2_DIR(0764), .mtime = 1555160000),
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/pg"),
            // readdir returns dot
            HRN_LIBSSH2_READDIR("."),
            // readdir returns path
            HRN_LIBSSH2_READDIR("path"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/path", .attr = HRN_LIBSSH2_DIR(0755), .mtime = 1555160000, .uid = 77777, .gid = 77777),
            // readdir returns file
            HRN_LIBSSH2_READDIR("file"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/file", .attr = HRN_LIBSSH2_FILE(0660), .mtime = 1656433838, .filesize = 8),
            // readdir returns link
            HRN_LIBSSH2_READDIR("link"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/link", .attr = HRN_LIBSSH2_LINK(0764), .mtime = 1656433838),
            // readdir returns pipe
            HRN_LIBSSH2_READDIR("pipe"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/pipe", .attr = HRN_LIBSSH2_FIFO(0764), .mtime = 1656433838),
            // readdir returns zzz
            HRN_LIBSSH2_READDIR("zzz"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/zzz", .attr = HRN_LIBSSH2_DIR(0764), .mtime = 1656433838),
            // readdir return empty
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            // Recurse path
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/pg/path"),
            // readdir returns file
            HRN_LIBSSH2_READDIR("file"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/path/file", .attr = HRN_LIBSSH2_FILE(0660), .mtime = 1656434296, .filesize = 8),
            // readdir returns empty
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            // Recurse zzz
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/pg/zzz"),
            // readdir returns yyy
            HRN_LIBSSH2_READDIR("yyy"),
            HRN_LIBSSH2_STAT(TEST_PATH "/pg/zzz/yyy", .attr = HRN_LIBSSH2_DIR(0764), .mtime = 1656433838),
            // readdir returns empty
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            // Recurse yyy
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/pg/zzz/yyy"),
            // readdir returns empty
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        // Create storage object for testing
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
        TEST_TITLE("path missing, errorOnMissing");

        HRN_LIBSSH2_SCRIPT_SET(HRNLIBSSH2_MACRO_STARTUP());

        const Storage *storageTest = storageRepoWrite();

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/BOGUS", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_NONE),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE));

        TEST_ERROR_FMT(
            storageListP(storageTest, STRDEF(BOGUS_STR), .errorOnMissing = true), PathMissingError, STORAGE_ERROR_LIST_INFO_MISSING,
            TEST_PATH "/BOGUS");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path missing, timeout on sftp_open_ex");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/BOGUS", .resultNull = true, .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/BOGUS", .resultNull = true, .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_EAGAIN));

        TEST_ERROR_FMT(
            storageListP(storageTest, STRDEF(BOGUS_STR), .errorOnMissing = true), FileReadError,
            "timeout opening directory '" TEST_PATH "/BOGUS'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on missing, ignore missing");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/noperm", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_PERMISSION_DENIED),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_PERMISSION_DENIED));

        // Should still error even when ignore missing
        TEST_ERROR_FMT(
            storageListP(storageTest, pathNoPerm), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": libssh2 error [-31]: sftp error [3] permission denied", strZ(pathNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("list - path with files");

        ioBufferSizeSet(65536);

        HRN_LIBSSH2_SCRIPT_SET(
            // Write aaa.txt
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/.aaa.txt.pgbackrest.tmp"),
            HRN_LIBSSH2_WRITE(14),
            HRN_LIBSSH2_FSYNC(.resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_FSYNC(),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_RENAME(TEST_PATH "/.aaa.txt.pgbackrest.tmp", TEST_PATH "/.aaa.txt", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_RENAME(
                TEST_PATH "/.aaa.txt.pgbackrest.tmp", TEST_PATH "/.aaa.txt", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_FAILURE),
            HRN_LIBSSH2_UNLINK(TEST_PATH "/.aaa.txt"),
            HRN_LIBSSH2_RENAME(TEST_PATH "/.aaa.txt.pgbackrest.tmp", TEST_PATH "/.aaa.txt"),
            // read dir
            HRN_LIBSSH2_OPENDIR(TEST_PATH),
            HRN_LIBSSH2_READDIR(".aaa.txt"),
            HRN_LIBSSH2_READDIR("noperm"),
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            // write bbb.txt
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/bbb.txt.pgbackrest.tmp"),
            HRN_LIBSSH2_WRITE(7),
            HRN_LIBSSH2_FSYNC(),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_RENAME(TEST_PATH "/bbb.txt.pgbackrest.tmp", TEST_PATH "/bbb.txt"),
            // read dir
            HRN_LIBSSH2_OPENDIR(TEST_PATH),
            HRN_LIBSSH2_READDIR("bbb.txt"),
            HRN_LIBSSH2_READDIR("noperm"),
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(.resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_CLOSE());

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, STRDEF(".aaa.txt")), BUFSTRDEF("aaaaaaaaaaaaaa")), "write aaa.text");
        TEST_RESULT_STRLST_Z(strLstSort(storageListP(storageTest, NULL), sortOrderAsc), ".aaa.txt\n" "noperm\n", "dir list");

        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, STRDEF("bbb.txt")), BUFSTRDEF("bbb.txt")), "write bbb.text");
        TEST_RESULT_STRLST_Z(storageListP(storageTest, NULL, .expression = STRDEF("^bbb")), "bbb.txt\n", "dir list");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpList() readdir EAGAIN timeout");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPENDIR(TEST_PATH),
            HRN_LIBSSH2_READDIR("", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_READDIR("", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING),
            HRN_LIBSSH2_CLOSE());

        TEST_RESULT_VOID(storageListP(storageTest, NULL, .errorOnMissing = true), "storageSftpList readdir EAGAIN timeout");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpList() fail to close path after listing EAGAIN timeout");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPENDIR(TEST_PATH),
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(.resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING));

        TEST_ERROR_FMT(
            storageListP(storageTest, NULL, .errorOnMissing = true), PathCloseError,
            "timeout closing path '" TEST_PATH "' after listing");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpList() fail to close path after listing");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPENDIR(TEST_PATH),
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(.resultInt = LIBSSH2_ERROR_SOCKET_RECV),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_ERROR_NONE),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        TEST_ERROR_FMT(
            storageListP(storageTest, NULL, .errorOnMissing = true), PathCloseError,
            "unable to close path '" TEST_PATH "' after listing: libssh2 error [-43]");

        storageHelperFree();
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathCreate()"))
    {
#ifdef HAVE_LIBSSH2
        const Storage *storageTest;

        HRN_LIBSSH2_SCRIPT_SET(HRNLIBSSH2_MACRO_STARTUP());

        StringList *argList = strLstDup(argCommonList);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ASSIGN(storageTest, storageRepoWrite(), "new storage /");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create /sub1");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub1", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub1"),
            HRN_LIBSSH2_STAT(TEST_PATH "/sub1", .attr = HRN_LIBSSH2_DIR(0750), .mtime = 1656434296));

        TEST_RESULT_VOID(storagePathCreateP(storageTest, STRDEF("sub1")), "create sub1");
        TEST_RESULT_INT(storageInfoP(storageTest, STRDEF("sub1")).mode, 0750, "check sub1 dir mode");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create /sub1 again fails mkdir_ex ssh error");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub1", .resultInt = LIBSSH2_ERROR_SOCKET_SEND),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_ERROR_NONE));

        TEST_ERROR(
            storagePathCreateP(storageTest, STRDEF("sub1")), PathCreateError,
            "ssh2 error [-7] unable to create path '" TEST_PATH "/sub1': libssh2 error [-7]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create /sub1 again no error on path already exists");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub1", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_FAILURE),
            HRN_LIBSSH2_STAT(TEST_PATH "/sub1", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_STAT(TEST_PATH "/sub1", .attr = HRN_LIBSSH2_DIR(0750), .mtime = 1656434296));

        TEST_RESULT_VOID(storagePathCreateP(storageTest, STRDEF("sub1")), "create sub1 again");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create /sub1 again fails stat_ex, path doesn't already exist");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub1", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_FAILURE),
            HRN_LIBSSH2_STAT(TEST_PATH "/sub1", .resultInt = LIBSSH2_ERROR_SOCKET_SEND));

        TEST_RESULT_VOID(
            storagePathCreateP(storageTest, STRDEF("sub1")), "create sub1 again");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create /sub1 again fail timeout EAGAIN on stat_ex");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub1", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_FAILURE),
            HRN_LIBSSH2_STAT(TEST_PATH "/sub1", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING));

        TEST_ERROR(storagePathCreateP(storageTest, STRDEF("sub1")), PathCreateError, "timeout stat'ing path '" TEST_PATH "/sub1'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sub2 custom mode");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub2", .mode = 0777),
            HRN_LIBSSH2_STAT(TEST_PATH "/sub2", .attr = HRN_LIBSSH2_DIR(0777), .mtime = 1656434296));

        // NOTE: if operating against an actual sftp server, a neutral umask is required to get the proper permissions.
        // Without the neutral umask, permissions were 0775.
        TEST_RESULT_VOID(storagePathCreateP(storageTest, STRDEF("sub2"), .mode = 0777), "create sub2 with custom mode");
        TEST_RESULT_INT(storageInfoP(storageTest, STRDEF("sub2")).mode, 0777, "check sub2 dir mode");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sub3/sub4 .noParentCreate fail");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub3/sub4", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE));

        TEST_ERROR(
            storagePathCreateP(storageTest, STRDEF("sub3/sub4"), .noParentCreate = true), PathCreateError,
            "sftp error unable to create path '" TEST_PATH "/sub3/sub4': libssh2 error [-31]: sftp error [2] no such file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sub3/sub4 success");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub3/sub4", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE),
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub3"),
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub3/sub4"));

        TEST_RESULT_VOID(storagePathCreateP(storageTest, STRDEF("sub3/sub4")), "create sub3/sub4");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("subfail EAGAIN timeout");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_MKDIR(TEST_PATH "/subfail", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_MKDIR(TEST_PATH "/subfail", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING));

        // LIBSSH2_ERROR_EAGAIN timeout fail
        TEST_ERROR(
            storagePathCreateP(storageTest, STRDEF("subfail")), PathCreateError,
            "timeout creating path '" TEST_PATH "/subfail'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path create, timeout success on stat");

        HRN_LIBSSH2_SCRIPT_SET(
            // Timeout success
            HRN_LIBSSH2_MKDIR(TEST_PATH "/subfail", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_FAILURE),
            HRN_LIBSSH2_STAT(TEST_PATH "/subfail", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_STAT(TEST_PATH "/subfail", .flags = HRN_LIBSSH2_ATTR_EXISTENCE));

        TEST_RESULT_VOID(storagePathCreateP(storageTest, STRDEF("subfail")), "timeout success");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error other than no such file, no parent create");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_MKDIR(TEST_PATH "/subfail", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_PERMISSION_DENIED));

        TEST_ERROR(
            storagePathCreateP(storageTest, STRDEF("subfail"), .noParentCreate = true), PathCreateError,
            "sftp error unable to create path '" TEST_PATH "/subfail': libssh2 error [-31]: sftp error [3] permission denied");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no error on already exists");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_MKDIR(TEST_PATH "/subfail", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_FILE_ALREADY_EXISTS),
            HRN_LIBSSH2_STAT(TEST_PATH "/subfail", .flags = HRN_LIBSSH2_ATTR_EXISTENCE));

        TEST_RESULT_VOID(storagePathCreateP(storageTest, STRDEF("subfail")), "do not throw error on already exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on already exists");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_MKDIR(TEST_PATH "/subfail", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_FILE_ALREADY_EXISTS),
            HRN_LIBSSH2_STAT(TEST_PATH "/subfail", .flags = HRN_LIBSSH2_ATTR_EXISTENCE),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_FILE_ALREADY_EXISTS),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        TEST_ERROR(
            storagePathCreateP(storageTest, STRDEF("subfail"), .errorOnExists = true), PathCreateError,
            "sftp error unable to create path '" TEST_PATH "/subfail': path already exists");

        storageHelperFree();
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathRemove()"))
    {
#ifdef HAVE_LIBSSH2
        const Storage *storageTest = NULL;

        HRN_LIBSSH2_SCRIPT_SET(HRNLIBSSH2_MACRO_STARTUP());

        StringList *argList = strLstDup(argCommonList);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ASSIGN(storageTest, storageRepoWrite(), "new storage /");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - missing, errorOnMissing");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_RMDIR(TEST_PATH "/remove1", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_RMDIR(TEST_PATH "/remove1", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE));

        const String *pathRemove1 = STRDEF(TEST_PATH "/remove1");

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .errorOnMissing = true), PathRemoveError,
            STORAGE_ERROR_PATH_REMOVE_MISSING, strZ(pathRemove1));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - missing, recurse ignores missing");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/remove1", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE),
            HRN_LIBSSH2_RMDIR(TEST_PATH "/remove1", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE));

        TEST_RESULT_VOID(storagePathRemoveP(storageTest, pathRemove1, .recurse = true), "ignore missing path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - recurse subpath permission denied");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/remove1/remove2", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_PERMISSION_DENIED),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_PERMISSION_DENIED));

        String *pathRemove2 = strNewFmt("%s/remove2", strZ(pathRemove1));

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove2, .recurse = true), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": libssh2 error [-31]: sftp error [3] permission denied", strZ(pathRemove2));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - file in subpath, permission denied");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/remove1"),
            HRN_LIBSSH2_READDIR("remove2"),
            HRN_LIBSSH2_READDIR("remove.txt"),
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_UNLINK(TEST_PATH "/remove1/remove2", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_UNLINK(TEST_PATH "/remove1/remove2", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_FAILURE),
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/remove1/remove2"),
            HRN_LIBSSH2_READDIR("remove.txt"),
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_UNLINK(TEST_PATH "/remove1/remove2/remove.txt", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_PERMISSION_DENIED),
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/remove1/remove2/remove.txt", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE),
            HRN_LIBSSH2_RMDIR(TEST_PATH "/remove1/remove2/remove.txt", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE),
            HRN_LIBSSH2_RMDIR(TEST_PATH "/remove1/remove2", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_PERMISSION_DENIED));

        String *fileRemove = strNewFmt("%s/remove.txt", strZ(pathRemove2));

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), PathRemoveError,
            STORAGE_ERROR_PATH_REMOVE " sftp error [3] permission denied", strZ(pathRemove2));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - path with subpath and file removed");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/remove1"),
            HRN_LIBSSH2_READDIR("remove2"),
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_UNLINK(TEST_PATH "/remove1/remove2", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_FAILURE),
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/remove1/remove2"),
            HRN_LIBSSH2_READDIR("remove.txt"),
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_UNLINK(TEST_PATH "/remove1/remove2/remove.txt"),
            HRN_LIBSSH2_RMDIR(TEST_PATH "/remove1/remove2", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_RMDIR(TEST_PATH "/remove1/remove2"),
            HRN_LIBSSH2_RMDIR(TEST_PATH "/remove1", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_RMDIR(TEST_PATH "/remove1"));

        TEST_RESULT_VOID(storagePathRemoveP(storageTest, pathRemove1, .recurse = true), "remove path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - path with subpath ssh fail on unlink");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/remove1"),
            HRN_LIBSSH2_READDIR("remove2"),
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_UNLINK(TEST_PATH "/remove1/remove2", .resultInt = LIBSSH2_ERROR_SOCKET_SEND));

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), PathRemoveError,
            "unable to remove file '%s': libssh ssh [-7]", strZ(pathRemove2));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - other than LIBSSH2_FX_FAILURE/LIBSSH2_FX_PERMISSION_DENIED");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/remove1"),
            HRN_LIBSSH2_READDIR("remove2"),
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_UNLINK(TEST_PATH "/remove1/remove2", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_CONNECTION_LOST));

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), PathRemoveError,
            "unable to remove file '%s': libssh sftp [7] connection lost", strZ(pathRemove2));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - unlink LIBSSH2_ERROR_EAGAIN timeout");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/remove1"),
            HRN_LIBSSH2_READDIR("remove2"),
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_UNLINK(TEST_PATH "/remove1/remove2", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_FAILURE),
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/remove1/remove2"),
            HRN_LIBSSH2_READDIR("remove.txt"),
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_UNLINK(TEST_PATH "/remove1/remove2/remove.txt", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_UNLINK(TEST_PATH "/remove1/remove2/remove.txt", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING));

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), PathRemoveError, "timeout removing file '%s'",
            strZ(fileRemove));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - rmdir LIBSSH2_ERROR_EAGAIN timeout");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/remove1"),
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_RMDIR(TEST_PATH "/remove1", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING));

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), PathRemoveError, "timeout removing path '%s'",
            strZ(pathRemove1));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - rmdir ssh error");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPENDIR(TEST_PATH "/remove1"),
            HRN_LIBSSH2_READDIR(""),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_RMDIR(TEST_PATH "/remove1", .resultInt = LIBSSH2_ERROR_SOCKET_SEND),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_ERROR_NONE),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), PathRemoveError,
            "unable to remove path '%s': libssh2 error [-7]", strZ(pathRemove1));

        storageHelperFree();
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("StorageRead"))
    {
#ifdef HAVE_LIBSSH2
        StorageRead *file = NULL;
        const String *fileName = STRDEF(TEST_PATH "/readtest.txt");

        HRN_LIBSSH2_SCRIPT_SET(HRNLIBSSH2_MACRO_STARTUP());

        StringList *argList = strLstDup(argCommonList);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        const Storage *storageTest = storageRepo();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read missing - no such file");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/readtest.txt", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE));

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_ERROR_FMT(ioReadOpen(storageReadIo(file)), FileMissingError, STORAGE_ERROR_READ_MISSING, strZ(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read missing - EAGAIN timeout");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/readtest.txt", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_WRITING),
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/readtest.txt", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_OK));

        TEST_ERROR_FMT(
            ioReadOpen(storageReadIo(file)), FileOpenError, STORAGE_ERROR_READ_OPEN ": libssh2 error [-37]", strZ(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read open error - not sftp, not EAGAIN");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/readtest.txt", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_METHOD_NOT_SUPPORTED),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_METHOD_NOT_SUPPORTED),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_OK));

        TEST_ERROR_FMT(
            ioReadOpen(storageReadIo(file)), FileOpenError, STORAGE_ERROR_READ_OPEN ": libssh2 error [-33]", strZ(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read open error - socket error is not masked as missing");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/readtest.txt", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SOCKET_RECV),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SOCKET_RECV),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_OK));

        TEST_ERROR_FMT(
            ioReadOpen(storageReadIo(file)), FileOpenError, STORAGE_ERROR_READ_OPEN ": libssh2 error [-43]", strZ(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read open error - sftp error other than no such file is not masked as missing");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/readtest.txt", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_PERMISSION_DENIED));

        TEST_ERROR_FMT(
            ioReadOpen(storageReadIo(file)), FileOpenError,
            STORAGE_ERROR_READ_OPEN ": libssh2 error [-31]: sftp error [3] permission denied", strZ(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read success");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/readtest.txt"),
            HRN_LIBSSH2_CLOSE());

        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_RESULT_INT(ioReadFd(storageReadIo(file)), -1, "check read fd");
        TEST_RESULT_VOID(ioReadClose(storageReadIo(file)), "close file");

        // ------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("close success via storageReadSftpClose()");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/readtest.txt"),
            HRN_LIBSSH2_CLOSE());

        Buffer *outBuffer = bufNew(2);

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_RESULT_VOID(storageReadSftpClose((StorageReadSftp *)file->driver), "close file");

        // ------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("close, null sftpHandle");

        HRN_LIBSSH2_SCRIPT_SET(HRN_LIBSSH2_OPEN_READ(TEST_PATH "/readtest.txt"));

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        ((StorageReadSftp *)file->driver)->sftpHandle = NULL;
        TEST_RESULT_VOID(storageReadSftpClose(file->driver), "close file null sftpHandle");

        // ------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("close fail via storageReadSftpClose() EAGAIN");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/readtest.txt"),
            HRN_LIBSSH2_CLOSE(.resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_ERROR_NONE));

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_ERROR(
            storageReadSftpClose(file->driver), FileCloseError,
            "timeout closing file '" TEST_PATH "/readtest.txt': libssh2 error [-37]");

        // ------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("close fail via storageReadSftpClose() sftp error");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/readtest.txt"),
            HRN_LIBSSH2_CLOSE(.resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_CLOSE(.resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_FAILURE));

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_ERROR(
            storageReadSftpClose(file->driver), FileCloseError,
            "unable to close file '" TEST_PATH "/readtest.txt' after read: libssh2 errno [-31]: sftp errno [4]");

        // ------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("close fail via storageReadSftpClose() ssh error");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/readtest.txt"),
            HRN_LIBSSH2_CLOSE(.resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_CLOSE(.resultInt = LIBSSH2_ERROR_ZLIB));

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_ERROR(
            storageReadSftpClose(file->driver), FileCloseError,
            "unable to close file '" TEST_PATH "/readtest.txt' after read: libssh2 errno [-29]");

        // ------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageReadSftp() sftp error");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/readtest.txt"),
            HRN_LIBSSH2_READ(2, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_FAILURE));

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_ERROR(
            storageReadSftp(file->driver, outBuffer, false), FileReadError,
            "unable to read '" TEST_PATH "/readtest.txt': sftp errno [4]");

        // ------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageReadSftp() ssh error");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/readtest.txt"),
            HRN_LIBSSH2_READ(2, .resultInt = LIBSSH2_ERROR_ZLIB),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_ERROR_NONE));

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_ERROR(
            storageReadSftp(file->driver, outBuffer, false), FileReadError,
            "unable to read '" TEST_PATH "/readtest.txt': libssh2 error [-29]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("incremental load, storageReadSftp(), storageReadSftpEof()");

        fileName = STRDEF(TEST_PATH "/test.file");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/test.file"),
            HRN_LIBSSH2_READ(44, .readBuffer = STRDEF("TE")),
            HRN_LIBSSH2_READ(42, .readBuffer = STRDEF("ST")),
            HRN_LIBSSH2_READ(40, .readBuffer = STRDEF("FI")),
            HRN_LIBSSH2_READ(38, .readBuffer = STRDEF("LE")),
            HRN_LIBSSH2_READ(36, .readBuffer = STRDEF("\n")),
            HRN_LIBSSH2_READ(35),
            HRN_LIBSSH2_CLOSE(),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        Buffer *buffer = bufNew(0);
        outBuffer = bufNew(2);

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName, .limit = VARUINT64(44)), "new read file");

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
        TEST_RESULT_VOID(ioRead(storageReadIo(file), outBuffer), "load data");
        bufCat(buffer, outBuffer);
        bufUsedZero(outBuffer);

        TEST_RESULT_VOID(ioRead(storageReadIo(file), outBuffer), "no data to load");
        TEST_RESULT_UINT(bufUsed(outBuffer), 0, "buffer is empty");

        TEST_RESULT_VOID(storageReadSftp(file->driver, outBuffer, true), "no data to load from driver either");
        TEST_RESULT_UINT(bufUsed(outBuffer), 0, "buffer is empty");

        TEST_RESULT_BOOL(bufEq(buffer, BUFSTRDEF("TESTFILE\n")), true, "check file contents (all loaded)");

        TEST_RESULT_BOOL(ioReadEof(storageReadIo(file)), true, "eof");
        TEST_RESULT_BOOL(ioReadEof(storageReadIo(file)), true, "still eof");
        TEST_RESULT_BOOL(storageReadSftpEof(file->driver), true, "storageReadSftpEof eof true");

        TEST_RESULT_VOID(ioReadClose(storageReadIo(file)), "close file");

        TEST_RESULT_VOID(storageReadFree(storageNewReadP(storageTest, fileName)), "free file");

        storageHelperFree();
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("StorageWrite"))
    {
#ifdef HAVE_LIBSSH2
        const String *fileName = STRDEF(TEST_PATH "/sub1/testfile");
        StorageWrite *file = NULL;

        HRN_LIBSSH2_SCRIPT_SET(HRNLIBSSH2_MACRO_STARTUP());

        StringList *argList = strLstDup(argCommonList);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        const Storage *storageTest = storageRepoWrite();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("permission denied");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/noperm/noperm", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/noperm/noperm", .resultNull = true, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_PERMISSION_DENIED),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_PERMISSION_DENIED));

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileNoPerm, .noAtomic = true, .noCreatePath = false), "new write file");
        TEST_ERROR_FMT(
            ioWriteOpen(storageWriteIo(file)), FileOpenError,
            STORAGE_ERROR_WRITE_OPEN ": libssh2 error [-31]: sftp error [3] permission denied", strZ(fileNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("timeout");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/noperm/noperm", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/noperm/noperm", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_EAGAIN));

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileNoPerm, .noAtomic = true, .noCreatePath = false), "new write file");
        TEST_ERROR_FMT(ioWriteOpen(storageWriteIo(file)), FileOpenError, "timeout while opening file '%s'", strZ(fileNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("missing");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/missing", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE),
            HRN_LIBSSH2_MKDIR(TEST_PATH),
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/missing", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE));

        TEST_ASSIGN(
            file, storageNewWriteP(storageTest, STRDEF("missing"), .noAtomic = true, .noCreatePath = false), "new write file");
        TEST_ERROR_FMT(ioWriteOpen(storageWriteIo(file)), FileMissingError, STORAGE_ERROR_WRITE_MISSING, TEST_PATH "/missing");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write file - defaults");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile.pgbackrest.tmp", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE),
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub1"),
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile.pgbackrest.tmp"),
            HRN_LIBSSH2_FSYNC(.resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_FSYNC(),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_RENAME(TEST_PATH "/sub1/testfile.pgbackrest.tmp", TEST_PATH "/sub1/testfile"));

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageWriteSftpClose timeout on fsync");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile.pgbackrest.tmp", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE),
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub1"),
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile.pgbackrest.tmp"),
            HRN_LIBSSH2_FSYNC(.resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING));

        const String *fileNameTmp = STRDEF(TEST_PATH "/sub1/testfile.pgbackrest.tmp");

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_ERROR_FMT(ioWriteClose(storageWriteIo(file)), FileSyncError, "timeout syncing file '%s'", strZ(fileNameTmp));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageWriteSftpClose error on fsync");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile.pgbackrest.tmp", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE),
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub1"),
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile.pgbackrest.tmp"),
            HRN_LIBSSH2_FSYNC(.resultInt = LIBSSH2_ERROR_SOCKET_SEND),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_ERROR_NONE));

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_ERROR_FMT(
            ioWriteClose(storageWriteIo(file)),
            FileSyncError, "unable to sync file '%s' after write: libssh2 error [-7]", strZ(fileNameTmp));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageWriteSftpClose error on close");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile.pgbackrest.tmp", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE),
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub1"),
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile.pgbackrest.tmp"),
            HRN_LIBSSH2_FSYNC(),
            HRN_LIBSSH2_CLOSE(.resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING),
            HRN_LIBSSH2_CLOSE(.resultInt = LIBSSH2_ERROR_SOCKET_SEND),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_OK));

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_ERROR_FMT(
            ioWriteClose(storageWriteIo(file)), FileCloseError,
            "unable to close file '%s' after write: libssh2 error [-7]", strZ(fileNameTmp));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("timeout on rename");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile.pgbackrest.tmp", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE),
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub1"),
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile.pgbackrest.tmp"),
            HRN_LIBSSH2_FSYNC(),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_RENAME(
                TEST_PATH "/sub1/testfile.pgbackrest.tmp", TEST_PATH "/sub1/testfile", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING));

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_ERROR_FMT(ioWriteClose(storageWriteIo(file)), FileCloseError, "timeout renaming file '%s'", strZ(fileNameTmp));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp error on rename, other than LIBSSH2_FX_FAILURE");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile.pgbackrest.tmp", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE),
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub1"),
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile.pgbackrest.tmp"),
            HRN_LIBSSH2_FSYNC(),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_RENAME(
                TEST_PATH "/sub1/testfile.pgbackrest.tmp", TEST_PATH "/sub1/testfile", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_CONNECTION_LOST));

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_ERROR_FMT(
            ioWriteClose(storageWriteIo(file)), FileCloseError,
            "unable to move '%s' to '%s': libssh2 error [-31]: sftp error [7] connection lost", strZ(fileNameTmp),
            strZ(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp remove existing file on rename - LIBSSH2_FX_FILE_ALREADY_EXISTS");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile.pgbackrest.tmp", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE),
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub1"),
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile.pgbackrest.tmp"),
            HRN_LIBSSH2_FSYNC(),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_RENAME(
                TEST_PATH "/sub1/testfile.pgbackrest.tmp", TEST_PATH "/sub1/testfile", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_FILE_ALREADY_EXISTS),
            HRN_LIBSSH2_UNLINK(TEST_PATH "/sub1/testfile"),
            HRN_LIBSSH2_RENAME(TEST_PATH "/sub1/testfile.pgbackrest.tmp", TEST_PATH "/sub1/testfile"));

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("ssh error on rename");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile.pgbackrest.tmp", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE),
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub1"),
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile.pgbackrest.tmp"),
            HRN_LIBSSH2_FSYNC(),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_RENAME(
                TEST_PATH "/sub1/testfile.pgbackrest.tmp", TEST_PATH "/sub1/testfile", .resultInt = LIBSSH2_ERROR_SOCKET_SEND),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_OK));

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_ERROR_FMT(
            ioWriteClose(storageWriteIo(file)), FileCloseError,
            "unable to move '%s' to '%s': libssh2 error [-7]", strZ(fileNameTmp), strZ(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageWriteSftpClose() timeout EAGAIN on libssh2_sftp_close");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile.pgbackrest.tmp", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE),
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub1"),
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile.pgbackrest.tmp"),
            HRN_LIBSSH2_FSYNC(.resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_FSYNC(),
            HRN_LIBSSH2_CLOSE(.resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING));

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_ERROR_FMT(ioWriteClose(storageWriteIo(file)), FileCloseError, "timeout closing file '%s'", strZ(fileNameTmp));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write file - no atomic");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE),
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub1"),
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile"),
            HRN_LIBSSH2_FSYNC(),
            HRN_LIBSSH2_CLOSE());

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName, .noAtomic = true), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("open libssh2_sftp_open_ex timeout EAGAIN");

        fileName = STRDEF(TEST_PATH "/sub1/testfile");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile.pgbackrest.tmp", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE),
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub1"),
            HRN_LIBSSH2_OPEN_WRITE(
                TEST_PATH "/sub1/testfile.pgbackrest.tmp", .resultNull = true, .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING),
            HRN_LIBSSH2_OPEN_WRITE(
                TEST_PATH "/sub1/testfile.pgbackrest.tmp", .resultNull = true, .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_EAGAIN));

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName, .noSyncFile = true), "storageWriteSftpOpen timeout EAGAIN");
        TEST_ERROR_FMT(ioWriteOpen(storageWriteIo(file)), FileOpenError, "timeout while opening file '%s'", strZ(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("open libssh2_sftp_open_ex sftp error");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile.pgbackrest.tmp", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE),
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub1"),
            HRN_LIBSSH2_OPEN_WRITE(
                TEST_PATH "/sub1/testfile.pgbackrest.tmp", .resultNull = true, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_PERMISSION_DENIED));

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName, .noSyncFile = true), "storageWriteSftpOpen sftp error");
        TEST_ERROR_FMT(
            ioWriteOpen(storageWriteIo(file)), FileOpenError,
            STORAGE_ERROR_WRITE_OPEN ": libssh2 error [-31]: sftp error [3] permission denied", strZ(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("open libssh2_sftp_open_ex ssh error");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile.pgbackrest.tmp", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SOCKET_SEND),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SOCKET_SEND),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SOCKET_SEND),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_ERROR_NONE));

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName, .noSyncFile = true), "storageWriteSftpOpen ssh error");
        TEST_ERROR_FMT(
            ioWriteOpen(storageWriteIo(file)), FileOpenError, STORAGE_ERROR_WRITE_OPEN ": libssh2 error [-7]", strZ(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageWriteSftpClose() - null sftpHandle");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE),
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub1"),
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/testfile"),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName, .noAtomic = true, .noSyncPath = false), "new write file");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");

        ((StorageWriteSftp *)file->driver)->sftpHandle = NULL;

        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");

        storageHelperFree();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("permission denied");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = 5),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_MATCH),
            HRN_LIBSSH2_USERAUTH(),
            HRN_LIBSSH2_SFTP_INIT(),
            // Permission denied
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/noperm/noperm", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_PERMISSION_DENIED),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_PERMISSION_DENIED),
            HRNLIBSSH2_MACRO_SHUTDOWN());

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

        storageTest = storageRepoWrite();

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileNoPerm, .noAtomic = true), "new write file");
        TEST_ERROR_FMT(
            ioWriteOpen(storageWriteIo(file)),
            FileOpenError, STORAGE_ERROR_WRITE_OPEN ": libssh2 error [-31]: sftp error [3] permission denied",
            strZ(fileNoPerm));

        storageHelperFree();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file missing");

        fileName = STRDEF(TEST_PATH "/sub1/test.file");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = 5),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS2_FILE_CSTR, .resultInt = 5),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_MATCH),
            HRN_LIBSSH2_USERAUTH(),
            HRN_LIBSSH2_SFTP_INIT(),
            // Missing
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/test.file", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/test.file", .resultNull = true, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE),
            HRN_LIBSSH2_MKDIR(TEST_PATH "/sub1"),
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/test.file", .resultNull = true, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE),
            HRNLIBSSH2_MACRO_SHUTDOWN());

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

        storageTest = storageRepoWrite();

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName, .noAtomic = true), "new write file");
        TEST_ERROR_FMT(ioWriteOpen(storageWriteIo(file)), FileMissingError, STORAGE_ERROR_WRITE_MISSING, strZ(fileName));

        storageHelperFree();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write file success");

        const Buffer *buffer = BUFSTRDEF("TESTFILE\n");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = 5),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_MATCH),
            HRN_LIBSSH2_USERAUTH(),
            HRN_LIBSSH2_SFTP_INIT(),
            // Open file
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub1/test.file.pgbackrest.tmp"),
            // Write filled buffer to file
            HRN_LIBSSH2_WRITE(9),
            // Close file
            HRN_LIBSSH2_FSYNC(),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_RENAME(TEST_PATH "/sub1/test.file.pgbackrest.tmp", TEST_PATH "/sub1/test.file"),
            // Open file
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/sub1/test.file"),
            // Read file
            HRN_LIBSSH2_READ(65536, .readBuffer = STRDEF("TESTFILE\n")),
            HRN_LIBSSH2_READ(65527),
            HRN_LIBSSH2_CLOSE(),
            // Check path mode
            HRN_LIBSSH2_STAT(TEST_PATH "/sub1", .attr = HRN_LIBSSH2_DIR(0750), .mtime = 1656434296),
            // Check file mode
            HRN_LIBSSH2_STAT(TEST_PATH "/sub1/test.file", .attr = HRN_LIBSSH2_FILE(0640), .mtime = 1656434296),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptBufferSize, "64KiB");
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

        storageTest = storageRepoWrite();

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

        storageHelperFree();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write subpath and file success");

        fileName = STRDEF(TEST_PATH "/sub2/test.file");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_INIT(),
            HRN_LIBSSH2_SESSION_INIT(),
            HRN_LIBSSH2_HANDSHAKE(),
            HRN_LIBSSH2_KNOWNHOST_INIT(),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = 5),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS_FILE_CSTR, .resultInt = 5),
            HRN_LIBSSH2_KNOWNHOST_READFILE(KNOWNHOSTS2_FILE_CSTR, .resultInt = 5),
            HRN_LIBSSH2_HOSTKEY(),
            HRN_LIBSSH2_KNOWNHOST_CHECK(LIBSSH2_KNOWNHOST_CHECK_MATCH),
            HRN_LIBSSH2_USERAUTH(),
            HRN_LIBSSH2_SFTP_INIT(),
            // Open file
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/sub2/test.file", .mode = 0600),
            // Write filled buffer to file
            HRN_LIBSSH2_WRITE(9),
            HRN_LIBSSH2_CLOSE(),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        argList = strLstDup(argCommonList);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, "/home/" TEST_USER "/.ssh/known_hosts  ");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpKnownHost, "      /home/" TEST_USER "/.ssh/known_hosts2");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        storageTest = storageRepoWrite();

        TEST_RESULT_STR_Z(
            strLstGet(strLstNewVarLst(cfgOptionLst(cfgOptRepoSftpKnownHost)), 1), "/home/" TEST_USER "/.ssh/known_hosts  ",
            "check known hosts path trailing spaces");
        TEST_RESULT_STR_Z(
            strLstGet(strLstNewVarLst(cfgOptionLst(cfgOptRepoSftpKnownHost)), 2), "      /home/" TEST_USER "/.ssh/known_hosts2",
            "check known hosts path leading spaces");

        TEST_ASSIGN(
            file,
            storageNewWriteP(
                storageTest, fileName, .modePath = 0700, .modeFile = 0600, .noSyncPath = true, .noSyncFile = true,
                .noAtomic = true),
            "new write file");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_VOID(ioWrite(storageWriteIo(file), buffer), "write to file");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");

        storageHelperFree();
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePut() and storageGet()"))
    {
#ifdef HAVE_LIBSSH2
        HRN_LIBSSH2_SCRIPT_SET(HRNLIBSSH2_MACRO_STARTUP());

        StringList *argList = strLstDup(argCommonList);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, strZ(FSLASH_STR));
        hrnCfgArgRawZ(argList, cfgOptBufferSize, "64KiB");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        const Storage *storageTest = storageRepoWrite();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get error - attempt to get directory");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH),
            HRN_LIBSSH2_READ(65536, .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_READ(65536, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_FAILURE));

        TEST_ERROR(
            storageGetP(storageNewReadP(storageTest, TEST_PATH_STR, .ignoreMissing = false)), FileReadError,
            "unable to read '" TEST_PATH "': sftp errno [4]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - empty file");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/test.empty.pgbackrest.tmp"),
            HRN_LIBSSH2_FSYNC(),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_RENAME(TEST_PATH "/test.empty.pgbackrest.tmp", TEST_PATH "/test.empty"));

        const String *emptyFile = STRDEF(TEST_PATH "/test.empty");

        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, emptyFile), NULL), "put empty file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - empty file, already exists");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/test.empty.pgbackrest.tmp"),
            HRN_LIBSSH2_FSYNC(),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_RENAME(
                TEST_PATH "/test.empty.pgbackrest.tmp", TEST_PATH "/test.empty", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_FAILURE),
            HRN_LIBSSH2_UNLINK(TEST_PATH "/test.empty", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_UNLINK(TEST_PATH "/test.empty", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(99));

        TEST_ERROR(
            storagePutP(storageNewWriteP(storageTest, emptyFile), NULL), FileRemoveError,
            "unable to remove existing '" TEST_PATH "/test.empty': libssh2 error [-31]: sftp error [99] unknown error");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - empty file, remove already existing file, storageWriteSftpRename timeout EAGAIN");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/test.empty.pgbackrest.tmp"),
            HRN_LIBSSH2_FSYNC(),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_RENAME(
                TEST_PATH "/test.empty.pgbackrest.tmp", TEST_PATH "/test.empty", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_FAILURE),
            HRN_LIBSSH2_UNLINK(TEST_PATH "/test.empty", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_UNLINK(TEST_PATH "/test.empty"),
            HRN_LIBSSH2_RENAME(TEST_PATH "/test.empty.pgbackrest.tmp", TEST_PATH "/test.empty", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_RENAME(TEST_PATH "/test.empty.pgbackrest.tmp", TEST_PATH "/test.empty", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING));

        TEST_ERROR(
            storagePutP(storageNewWriteP(storageTest, emptyFile), NULL), FileWriteError,
            "timeout moving '" TEST_PATH "/test.empty.pgbackrest.tmp' to '" TEST_PATH "/test.empty'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - empty file, remove already existing file, storageWriteSftpRename ssh error");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/test.empty.pgbackrest.tmp"),
            HRN_LIBSSH2_FSYNC(),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_RENAME(
                TEST_PATH "/test.empty.pgbackrest.tmp", TEST_PATH "/test.empty", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_FAILURE),
            HRN_LIBSSH2_UNLINK(TEST_PATH "/test.empty", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_UNLINK(TEST_PATH "/test.empty"),
            HRN_LIBSSH2_RENAME(TEST_PATH "/test.empty.pgbackrest.tmp", TEST_PATH "/test.empty", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_RENAME(
                TEST_PATH "/test.empty.pgbackrest.tmp", TEST_PATH "/test.empty", .resultInt = LIBSSH2_ERROR_SOCKET_SEND),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_OK));

        TEST_ERROR(
            storagePutP(storageNewWriteP(storageTest, emptyFile), NULL), FileRemoveError,
            "unable to move '" TEST_PATH "/test.empty.pgbackrest.tmp' to '" TEST_PATH "/test.empty': libssh2 error [-7]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - empty file, EAGAIN fail on remove already existing file");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/test.empty.pgbackrest.tmp"),
            HRN_LIBSSH2_FSYNC(),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_RENAME(
                TEST_PATH "/test.empty.pgbackrest.tmp", TEST_PATH "/test.empty", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_FAILURE),
            HRN_LIBSSH2_UNLINK(TEST_PATH "/test.empty", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_UNLINK(TEST_PATH "/test.empty", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING));

        TEST_ERROR(
            storagePutP(storageNewWriteP(storageTest, emptyFile), NULL), FileWriteError,
            "timeout unlinking '" TEST_PATH "/test.empty'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - file with contents - timeout EAGAIN libssh2_sftp_write");

        ioBufferSizeSet(2);

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/test.txt.pgbackrest.tmp"),
            HRN_LIBSSH2_WRITE(2, .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_WRITE(2, .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING));

        TEST_ERROR(
            storagePutP(storageNewWriteP(storageTest, STRDEF(TEST_PATH "/test.txt")), BUFSTRDEF("FAIL\n")), FileWriteError,
            "timeout writing '" TEST_PATH "/test.txt.pgbackrest.tmp'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - file with contents - error other than EAGAIN libssh2_sftp_write");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/test.txt.pgbackrest.tmp"),
            HRN_LIBSSH2_WRITE(2, .resultInt = LIBSSH2_ERROR_SOCKET_SEND),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_ERROR_NONE));

        TEST_ERROR(
            storagePutP(storageNewWriteP(storageTest, STRDEF(TEST_PATH "/test.txt")), BUFSTRDEF("FAIL\n")), FileWriteError,
            "unable to write '" TEST_PATH "/test.txt.pgbackrest.tmp': libssh2 error [-7]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - file with contents");

        const Buffer *buffer = BUFSTRDEF("TESTFILE\n");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_WRITE(TEST_PATH "/test.txt.pgbackrest.tmp"),
            HRN_LIBSSH2_WRITE(2, .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_WRITE(2, .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_WRITE(2),
            HRN_LIBSSH2_WRITE(2),
            HRN_LIBSSH2_WRITE(2),
            HRN_LIBSSH2_WRITE(2, .resultInt = 1),
            HRN_LIBSSH2_WRITE(1),
            HRN_LIBSSH2_WRITE(1),
            HRN_LIBSSH2_FSYNC(),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_RENAME(TEST_PATH "/test.txt.pgbackrest.tmp", TEST_PATH "/test.txt"));

        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, STRDEF(TEST_PATH "/test.txt")), buffer), "put test file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - ignore missing");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/BOGUS", .resultNull = true),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_ERRNO(LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE));

        TEST_RESULT_PTR(
            storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/" BOGUS_STR), .ignoreMissing = true)), NULL,
            "get missing file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - empty file");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/test.empty"),
            HRN_LIBSSH2_READ(2),
            HRN_LIBSSH2_CLOSE());

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.empty"))), "get empty");
        TEST_RESULT_UINT(bufSize(buffer), 0, "size is 0");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - file with contents");

        const Buffer *outBuffer = bufNew(2);

        ioBufferSizeSet(65536);

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/test.txt"),
            HRN_LIBSSH2_READ(65536, .readBuffer = STRDEF("TESTFILE\n")),
            HRN_LIBSSH2_READ(65527),
            HRN_LIBSSH2_CLOSE());

        TEST_ASSIGN(outBuffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"))), "get text");
        TEST_RESULT_UINT(bufSize(outBuffer), 9, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(outBuffer), "TESTFILE\n", bufSize(outBuffer)) == 0, true, "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - exact size smaller");

        ioBufferSizeSet(2);

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/test.txt"),
            HRN_LIBSSH2_READ(2, .readBuffer = STRDEF("TE")),
            HRN_LIBSSH2_READ(2, .readBuffer = STRDEF("ST")),
            HRN_LIBSSH2_CLOSE());

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt")), .exactSize = 4), "get exact");
        TEST_RESULT_UINT(bufSize(buffer), 4, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "TEST", bufSize(buffer)) == 0, true, "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - exact size larger");

        ioBufferSizeSet(4);
        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/test.txt"),
            HRN_LIBSSH2_READ(4, .readBuffer = STRDEF("TEST")),
            HRN_LIBSSH2_READ(4, .readBuffer = STRDEF("FILE")),
            HRN_LIBSSH2_READ(4, .readBuffer = STRDEF("\n")),
            HRN_LIBSSH2_READ(3));

        TEST_ERROR(
            storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt")), .exactSize = 64), FileReadError,
            "unable to read 64 byte(s) from '" TEST_PATH "/test.txt'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - smaller buffer size");

        ioBufferSizeSet(2);

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/test.txt"),
            HRN_LIBSSH2_READ(2, .readBuffer = STRDEF("TE")),
            HRN_LIBSSH2_READ(2, .readBuffer = STRDEF("ST")),
            HRN_LIBSSH2_READ(2, .readBuffer = STRDEF("FI")),
            HRN_LIBSSH2_READ(2, .readBuffer = STRDEF("LE")),
            HRN_LIBSSH2_READ(2, .readBuffer = STRDEF("\n")),
            HRN_LIBSSH2_READ(1),
            HRN_LIBSSH2_CLOSE());

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"))), "get text");
        TEST_RESULT_UINT(bufSize(buffer), 9, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "TESTFILE\n", bufSize(buffer)) == 0, true, "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid read offset bytes");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/test.txt"),
            HRN_LIBSSH2_SEEK(18446744073709551615),
            HRN_LIBSSH2_READ(2, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_BAD_MESSAGE));

        TEST_ERROR(
            storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"), .offset = UINT64_MAX)), FileOpenError,
            "unable to seek to 18446744073709551615 in file '" TEST_PATH "/test.txt'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid read");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/test.txt"),
            HRN_LIBSSH2_READ(2, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_BAD_MESSAGE));

        TEST_ERROR(
            storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"))), FileReadError,
            "unable to read '" TEST_PATH "/test.txt': sftp errno [5]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read limited bytes");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/test.txt"),
            HRN_LIBSSH2_READ(2, .readBuffer = STRDEF("TE")),
            HRN_LIBSSH2_READ(2, .readBuffer = STRDEF("ST")),
            HRN_LIBSSH2_READ(2, .readBuffer = STRDEF("FI")),
            HRN_LIBSSH2_READ(1, .readBuffer = STRDEF("LE"), .resultInt = 1),
            HRN_LIBSSH2_CLOSE());

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"), .limit = VARUINT64(7))), "get");
        TEST_RESULT_UINT(bufSize(buffer), 7, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "TESTFIL", bufSize(buffer)) == 0, true, "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read offset bytes");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/test.txt"),
            HRN_LIBSSH2_SEEK(4),
            HRN_LIBSSH2_READ(2, .readBuffer = STRDEF("FI")),
            HRN_LIBSSH2_READ(2, .readBuffer = STRDEF("LE")),
            HRN_LIBSSH2_READ(2, .readBuffer = STRDEF("\n")),
            HRN_LIBSSH2_READ(1),
            HRN_LIBSSH2_CLOSE());

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"), .offset = 4)), "get");
        TEST_RESULT_UINT(bufSize(buffer), 5, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "FILE\n", bufSize(buffer)) == 0, true, "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - read timeout EAGAIN");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_OPEN_READ(TEST_PATH "/test.txt"),
            HRN_LIBSSH2_READ(2, .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_READ(2, .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        TEST_ERROR(
            storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"))), FileReadError,
            "timeout reading '" TEST_PATH "/test.txt'");

        storageHelperFree();
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("storageRemove()"))
    {
#ifdef HAVE_LIBSSH2
        HRN_LIBSSH2_SCRIPT_SET(HRNLIBSSH2_MACRO_STARTUP());

        StringList *argList = strLstDup(argCommonList);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, strZ(FSLASH_STR));
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        const Storage *storageTest = storageRepoWrite();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file exists");

        HRN_LIBSSH2_SCRIPT_SET(HRN_LIBSSH2_UNLINK("/exists"));

        TEST_RESULT_VOID(storageRemoveP(storageTest, STRDEF("exists")), "remove file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file missing");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_UNLINK("/missing", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_UNLINK("/missing", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE));

        TEST_RESULT_VOID(storageRemoveP(storageTest, STRDEF("missing")), "remove missing file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("timeout");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_UNLINK("/missing", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_UNLINK("/missing", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING));

        TEST_ERROR(storageRemoveP(storageTest, STRDEF("missing")), FileRemoveError, "timeout removing '/missing'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error");

        HRN_LIBSSH2_SCRIPT_SET(
            HRN_LIBSSH2_UNLINK("/missing", .resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_UNLINK("/missing", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_CONNECTION_LOST),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_CONNECTION_LOST),
            HRNLIBSSH2_MACRO_SHUTDOWN());

        TEST_ERROR(
            storageRemoveP(storageTest, STRDEF("missing")), FileRemoveError,
            "unable to remove file '/missing': libssh2 error [-31]: sftp error [7] connection lost");

        storageHelperFree();
#else
        TEST_LOG(PROJECT_NAME " not built with sftp support");
#endif // HAVE_LIBSSH2
    }

    // *****************************************************************************************************************************
    if (testBegin("storageSftpLibSsh2SessionFreeResource()"))
    {
#ifdef HAVE_LIBSSH2
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpLibSsh2SessionFreeResource() sftpHandle, sftpSession, session not NULL, branch tests, EAGAIN");

        HRN_LIBSSH2_SCRIPT_SET(
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "\",1,1]"},
            HRN_LIBSSH2_CLOSE(.resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_CLOSE(),
            HRN_LIBSSH2_SHUTDOWN(.resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),

            HRN_LIBSSH2_SHUTDOWN(),
            HRN_LIBSSH2_DISCONNECT(.resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_DISCONNECT(),
            HRN_LIBSSH2_SESSION_FREE(.resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(),
            HRN_LIBSSH2_SESSION_FREE(),
            HRN_LIBSSH2_CLOSE(),
            HRNLIBSSH2_MACRO_SHUTDOWN(),);

        Storage *storageTest = NULL;
        StorageSftp *storageSftp = NULL;

        StringList *argList = strLstDup(argCommonList);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ASSIGN(storageTest, storageSftpHelper(0, true, storageRepoPathExpression), "new storage");
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "storage driver");

        // Populate sftpHandle, NULL sftpSession and session
        storageSftp->sftpHandle = libssh2_sftp_open_ex(storageSftp->sftpSession, TEST_PATH, 25, 1, 0, 1);

        TEST_RESULT_VOID(storageSftpLibSsh2SessionFreeResource(storageSftp), "freeResource not NULL sftpHandle");

        objFree(storageTest);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpLibSsh2SessionFreeResource() sftpHandle, sftpSession, session all NULL");

        HRN_LIBSSH2_SCRIPT_SET(HRNLIBSSH2_MACRO_STARTUP());

        TEST_ASSIGN(storageTest, storageSftpHelper(0, true, NULL), "new storage");
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "storage driver");

        // NULL out sftpSession and session
        storageSftp->sftpSession = NULL;
        storageSftp->session = NULL;

        TEST_RESULT_VOID(objFree(storageTest), "free resource all NULL");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpLibSsh2SessionFreeResource() sftp close handle failure, sftpHandle not NULL, libssh2 error");

        HRN_LIBSSH2_SCRIPT_SET(
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "\",1,1]"},
            HRN_LIBSSH2_CLOSE(.resultInt = LIBSSH2_ERROR_SOCKET_SEND));

        TEST_ASSIGN(storageTest, storageSftpHelper(0, true, NULL), "new storage");
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "storage driver");

        // Populate sftpHandle, NULL out sftpSession and session
        storageSftp->sftpHandle = libssh2_sftp_open_ex(storageSftp->sftpSession, TEST_PATH, 25, 1, 0, 1);
        storageSftp->sftpSession = NULL;
        storageSftp->session = NULL;

        TEST_ERROR_FMT(objFree(storageTest), ServiceError, "failed to close sftpHandle: libssh2 errno [-7]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpLibSsh2SessionFreeResource() sftp close handle failure, sftpHandle not NULL, EAGAIN");

        HRN_LIBSSH2_SCRIPT_SET(
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "\",1,1]"},
            HRN_LIBSSH2_CLOSE(.resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING));

        TEST_ASSIGN(storageTest, storageSftpHelper(0, true, NULL), "new storage");
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "storage driver");

        // Populate sftpHandle, NULL out sftpSession and session
        storageSftp->sftpHandle = libssh2_sftp_open_ex(storageSftp->sftpSession, TEST_PATH, 25, 1, 0, 1);
        storageSftp->sftpSession = NULL;
        storageSftp->session = NULL;

        TEST_ERROR_FMT(objFree(storageTest), ServiceError, "timeout closing sftpHandle: libssh2 errno [-37]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpLibSsh2SessionFreeResource() sftp close handle failure, sftpHandle not NULL, libssh2 sftp error");

        HRN_LIBSSH2_SCRIPT_SET(
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"" TEST_PATH "\",1,1]"},
            HRN_LIBSSH2_CLOSE(.resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_CONNECTION_LOST));

        TEST_ASSIGN(storageTest, storageSftpHelper(0, true, NULL), "new storage");
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "storage driver");

        // Populate sftpHandle, NULL out sftpSession and session
        storageSftp->sftpHandle = libssh2_sftp_open_ex(storageSftp->sftpSession, TEST_PATH, 25, 1, 0, 1);
        storageSftp->sftpSession = NULL;
        storageSftp->session = NULL;

        TEST_ERROR_FMT(objFree(storageTest), ServiceError, "failed to close sftpHandle: libssh2 errno [-31]: sftp errno [7]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpLibSsh2SessionFreeResource() sftp shutdown failure libssh2 error");

        HRN_LIBSSH2_SCRIPT_SET(
            HRNLIBSSH2_MACRO_STARTUP(),
            HRN_LIBSSH2_SHUTDOWN(.resultInt = LIBSSH2_ERROR_SOCKET_SEND));

        TEST_ASSIGN(storageTest, storageSftpHelper(0, true, NULL), "new storage");
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "storage driver");
        TEST_ERROR_FMT(objFree(storageTest), ServiceError, "failed to shutdown sftpSession: libssh2 errno [-7]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpLibSsh2SessionFreeResource() sftp shutdown failure libssh2 sftp error");

        HRN_LIBSSH2_SCRIPT_SET(
            HRNLIBSSH2_MACRO_STARTUP(),
            HRN_LIBSSH2_SHUTDOWN(.resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_CONNECTION_LOST),
            HRN_LIBSSH2_SFTP_ERROR(LIBSSH2_FX_CONNECTION_LOST));

        TEST_ASSIGN(storageTest, storageSftpHelper(0, true, NULL), "new storage");
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "storage driver");
        TEST_ERROR_FMT(
            objFree(storageTest), ServiceError,
            "failed to shutdown sftpSession: libssh2 errno [-31]: sftp errno [7] connection lost");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpLibSsh2SessionFreeResource() sftp shutdown failure EAGAIN");

        HRN_LIBSSH2_SCRIPT_SET(
            HRNLIBSSH2_MACRO_STARTUP(),
            HRN_LIBSSH2_SHUTDOWN(.resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING));

        TEST_ASSIGN(storageTest, storageSftpHelper(0, true, NULL), "new storage");
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "storage driver");
        TEST_ERROR_FMT(objFree(storageTest), ServiceError, "timeout shutting down sftpSession: libssh2 errno [-37]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpLibSsh2SessionFreeResource() session disconnect failure");

        HRN_LIBSSH2_SCRIPT_SET(
            HRNLIBSSH2_MACRO_STARTUP(),
            HRN_LIBSSH2_SHUTDOWN(),
            HRN_LIBSSH2_DISCONNECT(.resultInt = LIBSSH2_ERROR_SOCKET_DISCONNECT));

        TEST_ASSIGN(storageTest, storageSftpHelper(0, true, NULL), "new storage");
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "storage driver");
        TEST_ERROR_FMT(objFree(storageTest), ServiceError, "failed to disconnect libssh2 session: libssh2 errno [-13]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpLibSsh2SessionFreeResource() session disconnect failure EAGAIN");

        HRN_LIBSSH2_SCRIPT_SET(
            HRNLIBSSH2_MACRO_STARTUP(),
            HRN_LIBSSH2_SHUTDOWN(),
            HRN_LIBSSH2_DISCONNECT(.resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING));

        TEST_ASSIGN(storageTest, storageSftpHelper(0, true, NULL), "new storage");
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "storage driver");
        TEST_ERROR_FMT(objFree(storageTest), ServiceError, "timeout disconnecting libssh2 session: libssh2 errno [-37]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpLibSsh2SessionFreeResource() session free failure");

        HRN_LIBSSH2_SCRIPT_SET(
            HRNLIBSSH2_MACRO_STARTUP(),
            HRN_LIBSSH2_SHUTDOWN(),
            HRN_LIBSSH2_DISCONNECT(),
            HRN_LIBSSH2_SESSION_FREE(.resultInt = LIBSSH2_ERROR_SOCKET_DISCONNECT));

        TEST_ASSIGN(storageTest, storageSftpHelper(0, true, NULL), "new storage");
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "storage driver");
        TEST_ERROR_FMT(objFree(storageTest), ServiceError, "failed to free libssh2 session: libssh2 errno [-13]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpLibSsh2SessionFreeResource() session free failure EAGAIN");

        HRN_LIBSSH2_SCRIPT_SET(
            HRNLIBSSH2_MACRO_STARTUP(),
            HRN_LIBSSH2_SHUTDOWN(),
            HRN_LIBSSH2_DISCONNECT(),
            HRN_LIBSSH2_SESSION_FREE(.resultInt = LIBSSH2_ERROR_EAGAIN),
            HRN_LIBSSH2_BLOCK(.resultInt = SSH2_BLOCK_READING_WRITING));

        TEST_ASSIGN(storageTest, storageSftpHelper(0, true, NULL), "new storage");
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "storage driver");
        TEST_ERROR_FMT(objFree(storageTest), ServiceError, "timeout freeing libssh2 session: libssh2 errno [-37]");
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
