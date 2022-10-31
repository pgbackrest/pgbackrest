/***********************************************************************************************************************************
Test SFTP Storage
***********************************************************************************************************************************/
#include "common/io/io.h"
#include "common/io/session.h"
#include "common/io/socket/client.h"
#include "common/time.h"
#include "storage/read.h"
#include "storage/cifs/storage.h"
#include "storage/sftp/storage.h"
#include "storage/write.h"

#include "common/harnessConfig.h"
#include "common/harnessFork.h"
#include "common/harnessLibssh2.h"
#include "common/harnessSftp.h"
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Test function for path expression
***********************************************************************************************************************************/
static String *
storageTestPathExpression(const String *expression, const String *path)
{
    String *result = NULL;

    if (strcmp(strZ(expression), "<TEST>") == 0)
        result = strNewFmt("test%s", path == NULL ? "" : zNewFmt("/%s", strZ(path)));
    else if (strcmp(strZ(expression), "<NULL>") != 0)
        THROW_FMT(AssertError, "invalid expression '%s'", strZ(expression));

    return result;
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    hrnSftpIoSessionFdShimInstall();

    ioBufferSizeSet(2);

    // Directory and file that cannot be accessed to test permissions errors
    const String *fileNoPerm = STRDEF(TEST_PATH "/noperm/noperm");
    String *pathNoPerm = strPath(fileNoPerm);

    // This test should always be first so the storage helper is uninitialized
    // *****************************************************************************************************************************
    if (testBegin("storageHelperDryRunInit()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("writable storage fails when dry-run is not initialized");

        TEST_ERROR(storagePgIdxWrite(0), AssertError, WRITABLE_WHILE_DRYRUN);
        TEST_ERROR(storageRepoIdxWrite(0), AssertError, WRITABLE_WHILE_DRYRUN);
        TEST_ERROR(storageSpoolWrite(), AssertError, WRITABLE_WHILE_DRYRUN);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("writable storage fails when dry-run is true");

        storageHelperDryRunInit(true);

        TEST_ERROR(storagePgIdxWrite(0), AssertError, WRITABLE_WHILE_DRYRUN);
        TEST_ERROR(storageRepoIdxWrite(0), AssertError, WRITABLE_WHILE_DRYRUN);
        TEST_ERROR(storageSpoolWrite(), AssertError, WRITABLE_WHILE_DRYRUN);
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNew()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("ssh2 init failure");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = -1},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(TEST_PATH_STR, STRDEF("localhost"), 22, 5, 5, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
            ServiceError, "unable to init libssh2");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("driver session failure");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = 0},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]", .resultNull = true},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(TEST_PATH_STR, STRDEF("localhost"), 22, 5, 5, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
            ServiceError, "unable to init libssh2 session");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("handshake failure");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = 0},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = "[63581]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = "[63581]", .resultInt = -1},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(TEST_PATH_STR, STRDEF("localhost"), 22, 1000, 1000, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
            ServiceError, "libssh2 handshake failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("handshake failure");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = 0},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = "[63581]", .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 1000},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = "[63581]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(TEST_PATH_STR, STRDEF("localhost"), 22, 1000, 1000, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
            ServiceError, "libssh2 handshake failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("hostkey hash fail");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = 0},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = "[63581]", .resultInt = 0},
            {.function = HRNLIBSSH2_HOSTKEY_HASH, .param = "[2]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_PROTO},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(TEST_PATH_STR, STRDEF("localhost"), 22, 1000, 1000, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
            ServiceError, "libssh2 hostkey hash failed: libssh2 errno [-14]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("public key from file auth failure");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = 0},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = "[63581]", .resultInt = 0},
            {.function = HRNLIBSSH2_HOSTKEY_HASH, .param = "[2]", .resultZ = "adummyhash12345"},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
                .param = "[\"vagrant\",7,\"/home/vagrant/.ssh/id_rsa.pub\",\"/home/vagrant/.ssh/id_rsa\",null]",
                .resultInt = -16},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = 0},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = -16},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(TEST_PATH_STR, STRDEF("localhost"), 22, 1000, 1000, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
            ServiceError,
            "public key authentication failed: libssh2 error [-16]\n"
            "HINT: libssh2 compiled against non-openssl libraries requires --repo-sftp-private-keyfile and"
                " --repo-sftp-public-keyfile to be provided\n"
            "HINT: libssh2 versions before 1.9.0 expect a PEM format keypair, try ssh-keygen -m PEM -t rsa -P \"\" to generate the"
                " keypair");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("public key from file auth failure, no public key");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = 0},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = "[63581]", .resultInt = 0},
            {.function = HRNLIBSSH2_HOSTKEY_HASH, .param = "[2]", .resultZ = "adummyhash12345"},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
                .param = "[\"vagrant\",7,null,\"/home/vagrant/.ssh/id_rsa\",null]", .resultInt = -16},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = 0},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = -16},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(TEST_PATH_STR, STRDEF("localhost"), 22, 1000, 1000, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa")),
            ServiceError,
            "public key authentication failed: libssh2 error [-16]\n"
            "HINT: libssh2 compiled against non-openssl libraries requires --repo-sftp-private-keyfile and"
                " --repo-sftp-public-keyfile to be provided\n"
            "HINT: libssh2 versions before 1.9.0 expect a PEM format keypair, try ssh-keygen -m PEM -t rsa -P \"\" to generate the"
                " keypair");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("public key from file auth failure, key passphrase");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = 0},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = "[63581]", .resultInt = 0},
            {.function = HRNLIBSSH2_HOSTKEY_HASH, .param = "[2]", .resultZ = "adummyhash12345"},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
                .param = "[\"vagrant\",7,\"/home/vagrant/.ssh/id_rsa.pub\",\"/home/vagrant/.ssh/id_rsa\",\"keyPassphrase\"]",
                .resultInt = -16},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = 0},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = -16},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(TEST_PATH_STR, STRDEF("localhost"), 22, 1000, 1000, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"),
                .keyPassphrase = STRDEF("keyPassphrase")),
            ServiceError,
            "public key authentication failed: libssh2 error [-16]\n"
            "HINT: libssh2 compiled against non-openssl libraries requires --repo-sftp-private-keyfile and"
                " --repo-sftp-public-keyfile to be provided\n"
            "HINT: libssh2 versions before 1.9.0 expect a PEM format keypair, try ssh-keygen -m PEM -t rsa -P \"\" to generate the"
                " keypair");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("public key from file auth no private key failure");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = 0},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = "[63581]", .resultInt = 0},
            {.function = HRNLIBSSH2_HOSTKEY_HASH, .param = "[2]", .resultZ = "adummyhash12345"},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(TEST_PATH_STR, STRDEF("localhost"), 22, 1000, 1000, .user = TEST_USER_STR,
                .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
            ParamRequiredError, "user and private key required");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("public key from file auth no user, no private key failure");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = 0},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = "[63581]", .resultInt = 0},
            {.function = HRNLIBSSH2_HOSTKEY_HASH, .param = "[2]", .resultZ = "adummyhash12345"},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(TEST_PATH_STR, STRDEF("localhost"), 22, 1000, 1000,
                .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
            ParamRequiredError, "user and private key required");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("public key from file LIBSSH2_ERROR_EAGAIN, failure");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = 0},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = "[63581]", .resultInt = 0},
            {.function = HRNLIBSSH2_HOSTKEY_HASH, .param = "[2]", .resultZ = "12345678910123456789"},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
                .param = "[\"vagrant\",7,\"/home/vagrant/.ssh/id_rsa.pub\",\"/home/vagrant/.ssh/id_rsa\",null]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 1000},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
                .param = "[\"vagrant\",7,\"/home/vagrant/.ssh/id_rsa.pub\",\"/home/vagrant/.ssh/id_rsa\",null]",
                .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = 0},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = 0},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(TEST_PATH_STR, STRDEF("localhost"), 22, 1000, 1000, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
            ServiceError,
            "public key authentication failed: libssh2 error [0]\n"
            "HINT: libssh2 compiled against non-openssl libraries requires --repo-sftp-private-keyfile and"
                " --repo-sftp-public-keyfile to be provided\n"
            "HINT: libssh2 versions before 1.9.0 expect a PEM format keypair, try ssh-keygen -m PEM -t rsa -P \"\" to generate the"
                " keypair");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init failure");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = 0},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = "[63581]", .resultInt = 0},
            {.function = HRNLIBSSH2_HOSTKEY_HASH, .param = "[2]", .resultZ = "12345678910123456789"},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
                .param = "[\"vagrant\",7,\"/home/vagrant/.ssh/id_rsa.pub\",\"/home/vagrant/.ssh/id_rsa\",null]",
                .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_INIT, .resultNull = true},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(TEST_PATH_STR, STRDEF("localhost"), 22, 1, 1, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
            ServiceError, "unable to init libssh2_sftp session");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init fail && timeout");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = 0},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = "[63581]", .resultInt = 0},
            {.function = HRNLIBSSH2_HOSTKEY_HASH, .param = "[2]", .resultZ = "12345678910123456789"},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
                .param = "[\"vagrant\",7,\"/home/vagrant/.ssh/id_rsa.pub\",\"/home/vagrant/.ssh/id_rsa\",null]",
                .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_INIT, .resultNull = true, .sleep = 1000},
            {.function = HRNLIBSSH2_SFTP_INIT, .resultNull = true, .sleep = 200},
            {.function = NULL}
        });

        TEST_ERROR(
            storageSftpNewP(TEST_PATH_STR, STRDEF("localhost"), 22, 1000, 1000, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
            ServiceError, "unable to init libssh2_sftp session");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init success");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        Storage *storageTest = NULL;
        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                STRDEF("/tmp"), STRDEF("localhost"), 22, 5, 5, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
             "new storage (defaults)");
        TEST_RESULT_BOOL(storageTest->write, false, "check write");

        // Free context, otherwise callbacks to libssh2SessionFreeResource() accumulate
        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp session init success timeout");

        // shim sets FD for tests.
        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            {.function = HRNLIBSSH2_INIT, .param = "[0]", .resultInt = 0},
            {.function = HRNLIBSSH2_SESSION_INIT_EX, .param = "[null,null,null,null]"},
            {.function = HRNLIBSSH2_SESSION_HANDSHAKE, .param = "[63581]", .resultInt = 0},
            {.function = HRNLIBSSH2_HOSTKEY_HASH, .param = "[2]", .resultZ = "12345678910123456789"},
            {.function = HRNLIBSSH2_USERAUTH_PUBLICKEY_FROMFILE_EX,
                .param = "[\"vagrant\",7,\"/home/vagrant/.ssh/id_rsa.pub\",\"/home/vagrant/.ssh/id_rsa\",null]",
                .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_INIT, .resultInt = 0, .sleep = 1000},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = NULL;
        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                STRDEF("/tmp"), STRDEF("localhost"), 22, 1000, 1000, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
             "new storage (defaults)");
        TEST_RESULT_BOOL(storageTest->write, false, "check write");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create new storage with defaults");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = NULL;
        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                STRDEF("/tmp"), STRDEF("localhost"), 22, 1000, 1000, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
             "new storage (defaults)");

        TEST_RESULT_STR_Z(storageTest->path, "/tmp", "check path");
        TEST_RESULT_INT(storageTest->modeFile, 0640, "check file mode");
        TEST_RESULT_INT(storageTest->modePath, 0750, "check path mode");
        TEST_RESULT_BOOL(storageTest->write, false, "check write");
        TEST_RESULT_BOOL(storageTest->pathExpressionFunction == NULL, true, "check expression function is not set");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create new storage - override defaults");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                STRDEF("/path/to"), STRDEF("localhost"), 22, 1000, 1000, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"),
                .modeFile = 0600, .modePath = 0700, .write = true),
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
        TEST_TITLE("storageSftpNewInternal create new storage - override pathSync, incorrect storage type to cover branches");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewInternal(
                STORAGE_POSIX_TYPE, STRDEF("/path/to"), STRDEF("localhost"), 22, 1000, 1000, TEST_USER_STR, NULL,
                STRDEF("/home/vagrant/.ssh/id_rsa.pub"), STRDEF("/home/vagrant/.ssh/id_rsa"), NULL, 0600, 0700, true, NULL,
                true),
            "new storage override pathSync)");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

    }

    // *****************************************************************************************************************************
    if (testBegin("storageExists() and storagePathExists()"))
    {
        hrnSftpIoSessionFdShimInstall();

        // imic existance of /noperm/noperm for permission denied
        // drwx------ 2 root root 3 Dec  5 15:30 noperm
        // noperm/:
        // total 1
        // -rw------- 1 root root 0 Dec  5 15:30 noperm

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // file missing
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/missing\",33,0]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/missing\",33,0]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE, .sleep = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/missing\",33,0]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            // path missing
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/missing\",33,0]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0\",25,0]",
                .attrPerms = LIBSSH2_SFTP_S_IFDIR, .resultInt = LIBSSH2_ERROR_NONE},
            // permission denied
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/noperm/noperm\",39,0]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/noperm/noperm\",39,0]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            // file and path
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/exists\",32,0]",
                .attrPerms = LIBSSH2_SFTP_S_IFREG, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pathExists\",36,0]",
                .attrPerms = LIBSSH2_SFTP_S_IFDIR, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/exists\",32,0]",
                .attrPerms = LIBSSH2_SFTP_S_IFREG, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pathExists\",36,0]",
                .attrPerms = LIBSSH2_SFTP_S_IFDIR, .resultInt = LIBSSH2_ERROR_NONE},
            // file exists after wait
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/exists\",32,0]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL, .sleep = 25},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/exists\",32,0]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL, .sleep = 25},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/exists\",32,0]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL, .sleep = 25},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/exists\",32,0]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL, .sleep = 25},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/exists\",32,0]",
                .attrPerms = LIBSSH2_SFTP_S_IFREG, .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        Storage *storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 2000, 2000, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file");

        TEST_RESULT_BOOL(storageExistsP(storageTest, STRDEF("missing")), false, "file does not exist");
        TEST_RESULT_BOOL(storageExistsP(storageTest, STRDEF("missing"), .timeout = 1000), false, "file does not exist");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path");

        TEST_RESULT_BOOL(storagePathExistsP(storageTest, STRDEF("missing")), false, "path does not exist");
        TEST_RESULT_BOOL(storagePathExistsP(storageTest, NULL), true, "test path exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("permission denied");

        TEST_ERROR_FMT(storageExistsP(storageTest, fileNoPerm), FileOpenError, STORAGE_ERROR_INFO, strZ(fileNoPerm));
        TEST_ERROR_FMT(storagePathExistsP(storageTest, fileNoPerm), FileOpenError, STORAGE_ERROR_INFO, strZ(fileNoPerm));

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
                HRN_SYSTEM_FMT("touch %s", strZ(fileExists));
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                TEST_RESULT_BOOL(storageExistsP(storageTest, fileExists, .timeout = 10000), true, "file exists after wait");
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

        HRN_SYSTEM_FMT("rm %s", strZ(fileExists));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
    }

    // *****************************************************************************************************************************
    if (testBegin("storageInfo()"))
    {
        // File open error via permission denied
        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/noperm/noperm\",39,1]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Create storage object for testing
        Storage *storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 4000, 4000, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ERROR_FMT(storageInfoP(storageTest, fileNoPerm), FileOpenError, STORAGE_ERROR_INFO, strZ(fileNoPerm));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info for / exists");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/\",1,1]", .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Create storage object for testing
        storageTest = storageSftpNewP(
           FSLASH_STR , STRDEF("localhost"), 22, 1000, 1000, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"));

        TEST_RESULT_BOOL(storageInfoP( storageTest, NULL).exists, true, "info for /");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info for / does not exist with no path feature");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        Storage *storageRootNoPath = storageSftpNewP(
            FSLASH_STR, STRDEF("localhost"), 22, 1000, 1000, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"));
        storageRootNoPath->pub.interface.feature ^= 1 << storageFeaturePath;

        TEST_RESULT_BOOL(storageInfoP(storageRootNoPath, NULL, .ignoreMissing = true).exists, false, "no info for /");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageRootNoPath)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("directory does not exists");

        const String *fileName = STRDEF(TEST_PATH "/fileinfo");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // file does not exist
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/fileinfo\",34,1]",
                 .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/fileinfo\",34,1]",
                 .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            // info out of base path - first test libssh2_sftp_stat_ex throws error before reaching libssh2 code
            // second test libssh2_sftp_stat_stat_ex reaches libssh2 code
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/etc\",4,1]", .resultInt = LIBSSH2_ERROR_NONE},
            // info - path
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0\",25,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            // info - path exists only
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0\",25,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            // info basic - path
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0\",25,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            // info - file
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/fileinfo\",34,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                    LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1555155555, .uid = 99999, .gid = 99999, .filesize = 8},
            // info - link
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/testlink\",34,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG | LIBSSH2_SFTP_S_IRWXO,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"/home/vagrant/test/test-0/testlink\",34,\"\",4096,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .symlinkExTarget = STRDEF("/tmp")},
            // info - link .followLink = true
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/testlink\",34,0]",
                .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/testlink\",34,0]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG | LIBSSH2_SFTP_S_IRWXO,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .uid = 0, .gid = 0},
            // info - link .followLink = true, timeout EAGAIN in followLink
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/testlink\",34,0]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 23},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/testlink\",34,0]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 1},
            // info - link .followLink = false, timeout EAGAIN
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/testlink\",34,1]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 23},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/testlink\",34,1]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 1},
            // info - link .followLink = false, libssh2_sftp_symlink_ex link destination size timeout
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/testlink\",34,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG | LIBSSH2_SFTP_S_IRWXO,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"/home/vagrant/test/test-0/testlink\",34,\"\",4096,1]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 18},
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"/home/vagrant/test/test-0/testlink\",34,\"\",4096,1]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 5},
            // info - pipe
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/testpipe\",34,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IWOTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .uid = 1000, .gid = 1000},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Create storage object for testing
        storageTest = storageSftpNewP(
           TEST_PATH_STR , STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ERROR_FMT(
            storageInfoP(
                storageTest, fileName),
            FileOpenError, STORAGE_ERROR_INFO_MISSING ": [115] Operation now in progress", strZ(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file does not exists");

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
        TEST_RESULT_STR(info.user, TEST_USER_STR, "check user");
        TEST_RESULT_UINT(info.groupId, TEST_GROUP_ID, "check group id");
        TEST_RESULT_STR(info.group, TEST_GROUP_STR, "check group");

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

        // mimic file chown'd 99999:99999, timestamp 1555155555, containing "TESTFILE"

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
        // mimic link testlink to /tmp

        TEST_ASSIGN(info, storageInfoP(storageTest, linkName), "get link info");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypeLink, "check type");
        TEST_RESULT_UINT(info.size, 0, "check size");
        TEST_RESULT_INT(info.mode, 0777, "check mode");
        TEST_RESULT_STR_Z(info.linkDestination, "/tmp", "check link destination");
        TEST_RESULT_STR(info.user, TEST_USER_STR, "check user");
        TEST_RESULT_STR(info.group, TEST_GROUP_STR, "check group");

        TEST_ASSIGN(info, storageInfoP(storageTest, linkName, .followLink = true), "get info from path pointed to by link");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypePath, "check type");
        TEST_RESULT_UINT(info.size, 0, "check size");
        TEST_RESULT_INT(info.mode, 0777, "check mode");
        TEST_RESULT_STR(info.linkDestination, NULL, "check link destination");
        TEST_RESULT_STR_Z(info.user, "root", "check user");
        TEST_RESULT_STR_Z(info.group, strEqZ(info.group, "wheel") ? "wheel" : "root", "check group");

        // libssh2_sftp_stat_ex timeout EAGAIN followLink true
        TEST_ERROR_FMT(
                storageInfoP(storageTest, linkName, .followLink = true), FileOpenError,
                "unable to get info for path/file '" TEST_PATH "/testlink'");

        // libssh2_sftp_stat_ex timeout EAGAIN followLink false
        TEST_ERROR_FMT(
                storageInfoP(storageTest, linkName, .followLink = false), FileOpenError,
                "unable to get info for path/file '" TEST_PATH "/testlink'");

        // libssh2_sftp_symlink_ex link destination timeout EAGAIN followLink false
        TEST_ERROR_FMT(
                storageInfoP(storageTest, linkName, .followLink = false), FileReadError,
                "unable to get destination for link '" TEST_PATH "/testlink'");

       // --------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info - pipe");

        const String *pipeName = STRDEF(TEST_PATH "/testpipe");
        // mimic pipe "/testpipe" with mode 0666

        TEST_ASSIGN(info, storageInfoP(storageTest, pipeName), "get info from pipe (special file)");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypeSpecial, "check type");
        TEST_RESULT_UINT(info.size, 0, "check size");
        TEST_RESULT_INT(info.mode, 0666, "check mode");
        TEST_RESULT_STR(info.linkDestination, NULL, "check link destination");
        TEST_RESULT_STR(info.user, TEST_USER_STR, "check user");
        TEST_RESULT_STR(info.group, TEST_GROUP_STR, "check group");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNewItrP()"))
    {
        // mimic existance of /noperm/noperm

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
              HRNLIBSSH2_MACRO_STARTUP(),
            // path missing errorOnMissing true
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/BOGUS\",31,0,0,1]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/BOGUS\",31,0,0,1]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = 0},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            // ignore missing dir
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/BOGUS\",31,0,0,1]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = 0},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            // path no perm
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/noperm\",32,0,0,1]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            // path no perm ignore missing
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/noperm\",32,0,0,1]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            // helper function storageSftpListEntry()
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"pg/missing\",10,1]", .resultNull = true},
            // path with only dot
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg\",28,0]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/pg\",28,0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("."),
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg\",28,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            // path with file, link, pipe, dot dotdot
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg\",28,0]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/pg\",28,0,0,1]"},
            // readdir returns .
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("."),
                .resultInt = 3, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            // readdir returns ..
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\".\",4096,null,0]", .fileName = STRDEF(".."),
                .resultInt = 8, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            // readdir returns .include
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"..\",4096,null,0]", .fileName = STRDEF(".include"),
                .resultInt = 8, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 77777, .gid = 77777},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/.include\",37,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP |
                    LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 77777, .gid = 77777},
            // readdir returns file
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\".include\",4096,null,0]", .fileName = STRDEF("file"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/file\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID
                    | LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1656433838, .uid = 1000, .gid = 1000, .filesize = 8},
            // readdir returns link
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"file\",4096,null,0]", .fileName = STRDEF("link"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/link\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000, .symlinkExTarget = STRDEF("../file")},
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"/home/vagrant/test/test-0/pg/link\",33,\"\",4096,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .symlinkExTarget = STRDEF("../file")},
            // readdir returns pipe
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"link\",4096,null,0]", .fileName = STRDEF("pipe"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/pipe\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"pipe\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/pg/.include\",37,0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0,
                .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP |
                    LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 77777, .gid = 77777},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg\",28,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/link\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000, .symlinkExTarget = STRDEF("../file")},
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"/home/vagrant/test/test-0/pg/link\",33,\"\",4096,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .symlinkExTarget = STRDEF("../file")},
            // helper function - storageSftpListEntry() info doesn't exist
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"pg/missing\",10,1]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"pg/missing\",10,1]", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Create storage object for testing
        Storage *storageTest = storageSftpNewP(
            TEST_PATH_STR , STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path missing");

        TEST_ERROR_FMT(
            storageNewItrP(storageTest, STRDEF(BOGUS_STR), .errorOnMissing = true), PathMissingError,
            STORAGE_ERROR_LIST_INFO_MISSING, TEST_PATH "/BOGUS");

        TEST_RESULT_PTR(storageNewItrP(storageTest, STRDEF(BOGUS_STR), .nullOnMissing = true), NULL, "ignore missing dir");

        TEST_ERROR_FMT(
            storageNewItrP(storageTest, pathNoPerm, .errorOnMissing = true), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": libssh2 error [-31]: libssh2sftp error [3]", strZ(pathNoPerm));

        // Should still error even when ignore missing
        TEST_ERROR_FMT(
            storageNewItrP(storageTest, pathNoPerm, .errorOnMissing = false  ), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": libssh2 error [-31]: libssh2sftp error [3]", strZ(pathNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("helper function - storageSftpListEntry()");

        TEST_RESULT_VOID(
            storageSftpListEntry(
                (StorageSftp *)storageDriver(storageTest), storageLstNew(storageInfoLevelBasic), STRDEF("pg"), "missing",
                storageInfoLevelBasic),
            "missing path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path with only dot");

//        storagePathCreateP(storageTest, STRDEF("pg"), .mode = 0766);

        // jrt !!!
        // NOTE: in my tests with --vm=none the resultant file would have the incorrect permissions. The request was coming through
        // properly but the umask resulted in altered permissions.  Do we want to alter the test result check, or the sshd_config
        // file as noted below? Do we need to note this in the documentation somehow?
        //
        // vagrant@pgbackrest-test:~$ umask
        // 0002
        //
        // Jul 13 16:38:33 localhost sftp-server[340225]: mkdir name "/home/vagrant/test/test-0/pg" mode 0766
        // But the directory would be created with 0764
        // test/test-0:
        // total 0
        // drwxrw-r-- 2 vagrant vagrant 40 Jul 14 15:37 pg
        //
        // Updating sshd_config to
        // Subsystem	sftp	/usr/lib/openssh/sftp-server -u 000
        // would result in the file being created with 0766 permissions
        TEST_STORAGE_LIST(
            storageTest, "pg",
            "./ {u=" TEST_USER ", g=" TEST_GROUP ", m=0764}\n",
            .level = storageInfoLevelDetail, .includeDot = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path with file, link, pipe");

        // mimic existance of TEST_PATH "/pg/.include" mode 0755 chown'd 77777:77777
        // mimic existance of TEST_PATH "pg/file" mode 0660 timemodified 1656433838 containing "TESTDATA"
        // mimic existance of TEST_PATH "/pg/link" linked to "../file"
        // mimic existance of TEST_PATH "/pg/pipe" mode 777

        // jrt !!! see note above about permissions and sftp server mask setting
        TEST_STORAGE_LIST(
            storageTest, "pg",
            "./ {u=" TEST_USER ", g=" TEST_GROUP ", m=0764}\n"
            ".include/ {u=77777, g=77777, m=0755}\n"
            "file {s=8, t=1656433838, u=" TEST_USER ", g=" TEST_GROUP ", m=0660}\n"
            "link> {d=../file, u=" TEST_USER ", g=" TEST_GROUP "}\n"
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

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/pg\",28,0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("."),
                .resultInt = 3, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\".\",4096,null,0]", .fileName = STRDEF(".include"),
                .resultInt = 8, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 77777, .gid = 77777},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/.include\",37,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP |
                    LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 77777, .gid = 77777},
            // readdir returns file
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\".include\",4096,null,0]", .fileName = STRDEF("file"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/file\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID
                    | LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1656433838, .uid = 1000, .gid = 1000, .filesize = 8},
            // readdir returns link
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"file\",4096,null,0]", .fileName = STRDEF("link"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/link\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000, .symlinkExTarget = STRDEF("../file")},
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"/home/vagrant/test/test-0/pg/link\",33,\"\",4096,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .symlinkExTarget = STRDEF("../file")},
            // readdir returns pipe
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"link\",4096,null,0]", .fileName = STRDEF("pipe"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/pipe\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"pipe\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Create storage object for testing
        storageTest = storageSftpNewP(
            TEST_PATH_STR , STRDEF("localhost"), 22, 4000, 4000, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"));

        TEST_ASSIGN(storageItr, storageNewItrP(storageTest, STRDEF("pg")), "new iterator");
        TEST_RESULT_BOOL(storageItrMore(storageItr), true, "check more");
        TEST_RESULT_BOOL(storageItrMore(storageItr), true, "check more again");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path - recurse desc");

        // mimic existance of "pg/path" mode 0700
        // mimic existance of "pg/path/file" mode 0600 timemodified 1656434296 containing "TESTDATA"

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg\",28,0]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/pg\",28,0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("."),
                .resultInt = 3, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\".\",4096,null,0]", .fileName = STRDEF("path"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 77777, .gid = 77777},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/path\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP |
                    LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 77777, .gid = 77777},
            // readdir returns file
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"path\",4096,null,0]", .fileName = STRDEF("file"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/file\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID
                    | LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1656433838, .uid = 1000, .gid = 1000, .filesize = 8},
            // readdir returns link
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"file\",4096,null,0]", .fileName = STRDEF("link"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/link\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000, .symlinkExTarget = STRDEF("../file")},
            // readdir returns pipe
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"link\",4096,null,0]", .fileName = STRDEF("pipe"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/pipe\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"pipe\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            // recurse readdir path
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/pg/path\",33,0,0,1]"},
            // readdir returns file
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("file"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/path/file\",38,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID
                    | LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1656434296, .uid = 1000, .gid = 1000, .filesize = 8},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"file\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg\",28,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/link\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000, .symlinkExTarget = STRDEF("../file")},
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"/home/vagrant/test/test-0/pg/link\",33,\"\",4096,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .symlinkExTarget = STRDEF("../file")},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Create storage object for testing
        storageTest = storageSftpNewP(
            TEST_PATH_STR , STRDEF("localhost"), 22, 4000, 4000, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"));

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
        // mimic existance of "pg/zzz/yyy" mode 0700

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg\",28,0]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/pg\",28,0,0,1]"},
            // readdir returns dot
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("."),
                .resultInt = 3, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            // readdir returns path
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\".\",4096,null,0]", .fileName = STRDEF("path"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 77777, .gid = 77777},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/path\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP |
                    LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 77777, .gid = 77777},
            // readdir returns file
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"path\",4096,null,0]", .fileName = STRDEF("file"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/file\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID
                    | LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1656433838, .uid = 1000, .gid = 1000, .filesize = 8},
            // readdir returns link
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"file\",4096,null,0]", .fileName = STRDEF("link"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/link\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000, .symlinkExTarget = STRDEF("../file")},
            // readdir returns pipe
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"link\",4096,null,0]", .fileName = STRDEF("pipe"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/pipe\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            // readdir returns zzz
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"pipe\",4096,null,0]", .fileName = STRDEF("zzz"),
                .resultInt = 3, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/zzz\",32,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            // readdir return empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"zzz\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            // recurse readdir path
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/pg/path\",33,0,0,1]"},
            // readdir returns file
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("file"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/path/file\",38,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID
                    | LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1656434296, .uid = 1000, .gid = 1000, .filesize = 8},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"file\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            // recurse zzz
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/pg/zzz\",32,0,0,1]"},
            // readdir returns yyy
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("yyy"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/zzz/yyy\",36,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"yyy\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            // recurse yyy
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/pg/zzz/yyy\",36,0,0,1]"},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg\",28,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/link\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000, .symlinkExTarget = STRDEF("../file")},
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"/home/vagrant/test/test-0/pg/link\",33,\"\",4096,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .symlinkExTarget = STRDEF("../file")},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Create storage object for testing
        storageTest = storageSftpNewP(
            TEST_PATH_STR , STRDEF("localhost"), 22, 4000, 4000, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"));

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

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg\",28,0]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/pg\",28,0,0,1]"},
            // readdir returns dot
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("."),
                .resultInt = 3, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            // readdir returns path
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\".\",4096,null,0]", .fileName = STRDEF("path"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 77777, .gid = 77777},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/path\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP |
                    LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 77777, .gid = 77777},
            // readdir returns file
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"path\",4096,null,0]", .fileName = STRDEF("file"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/file\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID
                    | LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1656433838, .uid = 1000, .gid = 1000, .filesize = 8},
            // readdir returns link
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"file\",4096,null,0]", .fileName = STRDEF("link"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/link\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000, .symlinkExTarget = STRDEF("../file")},
            // readdir returns pipe
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"link\",4096,null,0]", .fileName = STRDEF("pipe"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/pipe\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            // readdir returns zzz
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"pipe\",4096,null,0]", .fileName = STRDEF("zzz"),
                .resultInt = 3, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/zzz\",32,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            // readdir return empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"zzz\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            // recurse zzz
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/pg/zzz\",32,0,0,1]"},
            // readdir returns yyy
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("yyy"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/zzz/yyy\",36,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"yyy\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            // recurse yyy
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/pg/zzz/yyy\",36,0,0,1]"},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            // recurse path
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/pg/path\",33,0,0,1]"},
            // readdir returns file
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("file"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/path/file\",38,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID
                    | LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1656434296, .uid = 1000, .gid = 1000, .filesize = 8},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"file\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg\",28,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/link\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000, .symlinkExTarget = STRDEF("../file")},
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"/home/vagrant/test/test-0/pg/link\",33,\"\",4096,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .symlinkExTarget = STRDEF("../file")},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Create storage object for testing
        storageTest = storageSftpNewP(
            TEST_PATH_STR , STRDEF("localhost"), 22, 4000, 4000, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"));

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

        // mimic "pg/empty" mode 0700

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg\",28,0]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/pg\",28,0,0,1]"},
            // readdir returns dot
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("."),
                .resultInt = 3, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\".\",4096,null,0]", .fileName = STRDEF("empty"),
                .resultInt = 5, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 77777, .gid = 77777},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/empty\",34,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP |
                    LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 77777, .gid = 77777},
            // readdir returns path
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"empty\",4096,null,0]", .fileName = STRDEF("path"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 77777, .gid = 77777},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/path\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP |
                    LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 77777, .gid = 77777},
            // readdir returns file
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"path\",4096,null,0]", .fileName = STRDEF("file"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/file\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID
                    | LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1656433838, .uid = 1000, .gid = 1000, .filesize = 8},
            // readdir returns link
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"file\",4096,null,0]", .fileName = STRDEF("link"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/link\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000, .symlinkExTarget = STRDEF("../file")},
            // readdir returns pipe
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"link\",4096,null,0]", .fileName = STRDEF("pipe"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/pipe\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            // readdir returns zzz
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"pipe\",4096,null,0]", .fileName = STRDEF("zzz"),
                .resultInt = 3, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/zzz\",32,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            // readdir return empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"zzz\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            // recurse empty
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/pg/empty\",34,0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            // recurse path
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/pg/path\",33,0,0,1]"},
            // readdir returns file
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("file"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/path/file\",38,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID
                    | LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1656434296, .uid = 1000, .gid = 1000, .filesize = 8},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"file\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            // recurse zzz
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/pg/zzz\",32,0,0,1]"},
            // readdir returns yyy
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("yyy"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/zzz/yyy\",36,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"yyy\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            // recurse yyy
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/pg/zzz/yyy\",36,0,0,1]"},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Create storage object for testing
        storageTest = storageSftpNewP(
            TEST_PATH_STR , STRDEF("localhost"), 22, 4000, 4000, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"));

        TEST_STORAGE_LIST( storageTest, "pg",
            "empty/\n",
            .level = storageInfoLevelType, .expression = "^empty");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("filter in subpath during recursion");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg\",28,0]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/pg\",28,0,0,1]"},
            // readdir returns dot
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("."),
                .resultInt = 3, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            // readdir returns path
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\".\",4096,null,0]", .fileName = STRDEF("path"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 77777, .gid = 77777},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/path\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP |
                    LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 77777, .gid = 77777},
            // readdir returns file
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"path\",4096,null,0]", .fileName = STRDEF("file"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/file\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID
                    | LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1656433838, .uid = 1000, .gid = 1000, .filesize = 8},
            // readdir returns link
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"file\",4096,null,0]", .fileName = STRDEF("link"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/link\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000, .symlinkExTarget = STRDEF("../file")},
            // readdir returns pipe
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"link\",4096,null,0]", .fileName = STRDEF("pipe"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/pipe\",33,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFIFO | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            // readdir returns zzz
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"pipe\",4096,null,0]", .fileName = STRDEF("zzz"),
                .resultInt = 3, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/zzz\",32,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            // readdir return empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"zzz\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            // recurse path
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/pg/path\",33,0,0,1]"},
            // readdir returns file
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("file"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/path/file\",38,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID
                    | LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1656434296, .uid = 1000, .gid = 1000, .filesize = 8},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"file\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            // recurse zzz
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/pg/zzz\",32,0,0,1]"},
            // readdir returns yyy
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("yyy"),
                .resultInt = 4, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/pg/zzz/yyy\",36,1]",
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IWGRP | LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"yyy\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656433838, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            // recurse yyy
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/pg/zzz/yyy\",36,0,0,1]"},
            // readdir returns empty
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Create storage object for testing
        storageTest = storageSftpNewP(
            TEST_PATH_STR , STRDEF("localhost"), 22, 4000, 4000, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"));

        TEST_STORAGE_LIST(
            storageTest, "pg",
            "path/file {s=8, t=1656434296}\n",
            .level = storageInfoLevelBasic, .expression = "\\/file$");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
    }

    // *****************************************************************************************************************************
    if (testBegin("storageList()"))
    {
        // mimic existance of /noperm/noperm

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path missing");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/BOGUS\",31,0,0,1]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = 0},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/BOGUS\",31,0,0,1]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = 0},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            // empty list
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/BOGUS\",31,0,0,1]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = 0},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            // timeout on sftp_open_ex
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/BOGUS\",31,0,0,1]", .resultNull = true,
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 23},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/BOGUS\",31,0,0,1]", .resultNull = true,
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 4},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 4},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 4},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_OK},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            // error on missing, regardless of errorOnMissing setting
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/noperm\",32,0,0,1]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            // ignore missing
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/noperm\",32,0,0,1]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Create storage object for testing
        Storage *storageTest = storageSftpNewP(
            TEST_PATH_STR , STRDEF("localhost"), 22, 25, 25, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"));

        TEST_ERROR_FMT(
            storageListP(storageTest, STRDEF(BOGUS_STR), .errorOnMissing = true), PathMissingError, STORAGE_ERROR_LIST_INFO_MISSING,
            TEST_PATH "/BOGUS");

        TEST_RESULT_PTR(storageListP(storageTest, STRDEF(BOGUS_STR), .nullOnMissing = true), NULL, "null for missing dir");
        TEST_RESULT_UINT(strLstSize(storageListP(storageTest, STRDEF(BOGUS_STR))), 0, "empty list for missing dir");

        // timeout on sftp_open_ex
        TEST_ERROR_FMT(
            storageListP(storageTest, STRDEF(BOGUS_STR), .errorOnMissing = true), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": libssh2 error [-37]",
            TEST_PATH "/BOGUS");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on missing, regardless of errorOnMissing setting");

        TEST_ERROR_FMT(
            storageListP(storageTest, pathNoPerm, .errorOnMissing = true), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": libssh2 error [-31]: libssh2sftp error [3]", strZ(pathNoPerm));

        // Should still error even when ignore missing
        TEST_ERROR_FMT(
            storageListP(storageTest, pathNoPerm), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": libssh2 error [-31]: libssh2sftp error [3]", strZ(pathNoPerm));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("list - path with files");

        ioBufferSizeSet(14);
        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // storagePutP(storageNewWriteP())
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/.aaa.txt.pgbackrest.tmp\",49,26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[14]", .resultInt = 14},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
                .param = "[\"/home/vagrant/test/test-0/.aaa.txt.pgbackrest.tmp\",49,\"/home/vagrant/test/test-0/.aaa.txt\",34,7]",
                .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
                .param = "[\"/home/vagrant/test/test-0/.aaa.txt.pgbackrest.tmp\",49,\"/home/vagrant/test/test-0/.aaa.txt\",34,7]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/home/vagrant/test/test-0/.aaa.txt\",34]", .resultUInt = 0},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
                .param = "[\"/home/vagrant/test/test-0/.aaa.txt.pgbackrest.tmp\",49,\"/home/vagrant/test/test-0/.aaa.txt\",34,7]",
                .resultInt = 0},
            // strListSort(storageListP())
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0\",25,0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF(".aaa.txt"),
                .resultInt = 8, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/.aaa.txt\",34,1]",
                .fileName = STRDEF(".aaa.txt"), .resultInt = 0,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\".aaa.txt\",4096,null,0]", .fileName = STRDEF("noperm"),
                .resultInt = 6, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/noperm\",32,1]",
                .fileName = STRDEF("noperm"), .resultInt = 0, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"noperm\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            // bbb.txt
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/bbb.txt.pgbackrest.tmp\",48,26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[7]", .resultInt = 7},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
                .param = "[\"/home/vagrant/test/test-0/bbb.txt.pgbackrest.tmp\",48,\"/home/vagrant/test/test-0/bbb.txt\",33,7]",
                .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0\",25,0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("bbb.txt"),
                .resultInt = 7, .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/bbb.txt\",33,1]",
                .fileName = STRDEF("bbb.txt"), .resultInt = 0,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"bbb.txt\",4096,null,0]", .fileName = STRDEF("noperm"),
                .resultInt = 6, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/noperm\",32,1]",
                .fileName = STRDEF("noperm"), .resultInt = 0, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"noperm\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            // path with only dot, readdir timeout EAGAIN
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0\",25,0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .resultInt = LIBSSH2_ERROR_EAGAIN, .param = "[\"\",4096,null,0]", .sleep = 18},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .resultInt = LIBSSH2_ERROR_EAGAIN, .param = "[\"\",4096,null,0]", .sleep = 6},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            //  fail to close path after listing
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0\",25,0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("."),
                .resultInt = LIBSSH2_ERROR_NONE, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1555160000, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 18},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 5},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Create storage object for testing
        storageTest = storageSftpNewP(
            TEST_PATH_STR , STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, STRDEF(".aaa.txt")), BUFSTRDEF("aaaaaaaaaaaaaa")), "write aaa.text");
        TEST_RESULT_STRLST_Z(strLstSort(storageListP(storageTest, NULL), sortOrderAsc), ".aaa.txt\n" "noperm\n", "dir list");

        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, STRDEF("bbb.txt")), BUFSTRDEF("bbb.txt")), "write bbb.text");
        TEST_RESULT_STRLST_Z(storageListP(storageTest, NULL, .expression = STRDEF("^bbb")), "bbb.txt\n", "dir list");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpList() readdir EAGAIN timeout");

        TEST_RESULT_VOID(storageListP(storageTest, NULL, .errorOnMissing = true),"storageSftpList readdir EAGAIN timeout");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpList() fail to close path after listing");

        TEST_ERROR_FMT(
            storageListP(storageTest, NULL, .errorOnMissing = true), PathCloseError,
            "unable to close path '" TEST_PATH "' after listing");

        ioBufferSizeSet(2);

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePath()"))
    {
        Storage *storageTest = NULL;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path - root path");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                FSLASH_STR, STRDEF("localhost"), 22, 1000, 1000, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
            "new storage /");
        TEST_RESULT_STR_Z(storagePathP(storageTest, NULL), "/", "root dir");
        TEST_RESULT_STR_Z(storagePathP(storageTest, STRDEF("/")), "/", "same as root dir");
        TEST_RESULT_STR_Z(storagePathP(storageTest, STRDEF("subdir")), "/subdir", "simple subdir");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path - expressions");

        TEST_ERROR(
            storagePathP(storageTest, STRDEF("<TEST>")), AssertError, "expression '<TEST>' not valid without callback function");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                STRDEF("/path/to"), STRDEF("localhost"), 22, 1000, 1000, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"),
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
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathCreate()"))
    {
        Storage *storageTest = NULL;

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // create /sub1
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub1\",30,488]",
                .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub1\",30,488]", .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/sub1\",30,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID
                    | LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            // create again
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub1\",30,488]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/sub1\",30,0]",
                .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/sub1\",30,0]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID
                    | LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            // errorOnExists
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub1\",30,488]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/sub1\",30,0]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID
                    | LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            // sub2 custom mode
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub2\",30,511]",
                .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/sub2\",30,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG | LIBSSH2_SFTP_S_IROTH |
                    LIBSSH2_SFTP_S_IXOTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID
                    | LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            // sub3/sub4 .noParentCreate fail
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub3/sub4\",35,488]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            // sub3/sub4 success
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub3/sub4\",35,488]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub3\",30,488]",
                .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub3/sub4\",35,488]",
                .resultInt = 0},
            // subfail EAGAIN timeout
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/subfail\",33,488]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 50},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/subfail\",33,488]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 50},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                TEST_PATH_STR, STRDEF("localhost"), 22, 25, 25, .user = TEST_USER_STR, .write = true,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
            "new storage /");

        TEST_RESULT_VOID(storagePathCreateP(storageTest, STRDEF("sub1")), "create sub1");
        TEST_RESULT_INT(storageInfoP(storageTest, STRDEF("sub1")).mode, 0750, "check sub1 dir mode");
        TEST_RESULT_VOID(storagePathCreateP(storageTest, STRDEF("sub1")), "create sub1 again");
        TEST_ERROR(
            storagePathCreateP(storageTest, STRDEF("sub1"), .errorOnExists = true), PathCreateError,
            "unable to create path '" TEST_PATH "/sub1': path already exists");

        // jrt !!!
        // NOTE: in my tests with --vm=none the resultant file would have the incorrect permissions. The request was coming through
        // properly but the umask resulted in altered permissions.  Do we want to alter the test result check, or the sshd_config
        // file as noted below? Do we need to note in documentation?
        //
        // vagrant@pgbackrest-test:~$ umask
        // 0022
        //
        // Sep 27 03:38:33 localhost sftp-server[340225]: mkdir name "/home/vagrant/test/test-0/sub2" mode 0777
        // But the directory would be created with 0775
        // test/test-0:
        // total 0
        // drwxrwxr-x 2 vagrant vagrant 40 Sep 27 03:37 sub2
        //
        // Updating sshd_config to
        // Subsystem	sftp	/usr/lib/openssh/sftp-server -u 000
        // would result in the file being created with 0777 permissions
        TEST_RESULT_VOID(storagePathCreateP(storageTest, STRDEF("sub2"), .mode = 0777), "create sub2 with custom mode");
        TEST_RESULT_INT(storageInfoP(storageTest, STRDEF("sub2")).mode, 0775, "check sub2 dir mode");

        TEST_ERROR(
            storagePathCreateP(storageTest, STRDEF("sub3/sub4"), .noParentCreate = true), PathCreateError,
            "unable to create path '" TEST_PATH "/sub3/sub4'");
        TEST_RESULT_VOID(storagePathCreateP(storageTest, STRDEF("sub3/sub4")), "create sub3/sub4");

        // LIBSSH2_ERROR_EAGAIN timeout fail
        TEST_ERROR(
            storagePathCreateP(storageTest, STRDEF("subfail")), PathCreateError, "unable to create path '" TEST_PATH "/subfail'");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path create, timeout success on stat");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // timeout success
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/subfail\",33,488]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/subfail\",33,0]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 30},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/subfail\",33,0]",
                .resultInt = LIBSSH2_ERROR_EAGAIN},
            // !=no such file && parent create LIBSSH2_FX_PERMISSION_DENIED}
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/subfail\",33,488]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            // no error on already exists
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/subfail\",33,488]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FILE_ALREADY_EXISTS},
            // error on already exists
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/subfail\",33,488]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FILE_ALREADY_EXISTS},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                TEST_PATH_STR, STRDEF("localhost"), 22, 25, 25, .user = TEST_USER_STR, .write = true,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
            "new storage /");

        TEST_RESULT_VOID(storagePathCreateP(storageTest, STRDEF("subfail")), "timeout success");
        TEST_ERROR(
            storagePathCreateP(storageTest, STRDEF("subfail"), .noParentCreate = true), PathCreateError,
            "unable to create path '" TEST_PATH "/subfail'");
        TEST_RESULT_VOID(storagePathCreateP(storageTest, STRDEF("subfail")), "do not throw error on already exists");
        TEST_ERROR(
            storagePathCreateP(storageTest, STRDEF("subfail"), .errorOnExists = true), PathCreateError,
            "unable to create path '" TEST_PATH "/subfail'");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathRemove()"))
    {
        Storage *storageTest = NULL;

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // path remove missing errorOnMissing
            {.function = HRNLIBSSH2_SFTP_RMDIR_EX, .param = "[\"/home/vagrant/test/test-0/remove1\",33]",
                .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_RMDIR_EX, .param = "[\"/home/vagrant/test/test-0/remove1\",33]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            // recurse - ignore missing path
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/remove1\",33,0,0,1]", .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            {.function = HRNLIBSSH2_SFTP_RMDIR_EX, .param = "[\"/home/vagrant/test/test-0/remove1\",33]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            // recurse parent/subpath permission denied
            {.function = HRNLIBSSH2_SFTP_RMDIR_EX, .param = "[\"/home/vagrant/test/test-0/remove1/remove2\",41]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            // recurse subpath permission denied
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/remove1/remove2\",41,0,0,1]",
                .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/remove1/remove2\",41,0,0,1]",
                .resultNull = true},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            // path remove - file in subpath, permission denied
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/remove1\",33,0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("remove2"),
                .resultInt = 7, .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"remove2\",4096,null,0]", .fileName = STRDEF("remove.txt"),
                .resultInt = 10,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"remove.txt\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/home/vagrant/test/test-0/remove1/remove2\",41]",
                .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/home/vagrant/test/test-0/remove1/remove2\",41]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/remove1/remove2\",41,0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("remove.txt"),
                .resultInt = 10,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG | LIBSSH2_SFTP_S_IRWXO,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"remove.txt\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/home/vagrant/test/test-0/remove1/remove2/remove.txt\",52]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            // path remove - path with subpath and file removed
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/remove1\",33,0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("remove2"),
                .resultInt = 7,
                .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG| LIBSSH2_SFTP_S_IRWXO,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"remove2\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/home/vagrant/test/test-0/remove1/remove2\",41]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/remove1/remove2\",41,0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("remove.txt"),
                .resultInt = 10,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG | LIBSSH2_SFTP_S_IRWXO,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"remove.txt\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/home/vagrant/test/test-0/remove1/remove2/remove.txt\",52]",
                .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_RMDIR_EX, .param = "[\"/home/vagrant/test/test-0/remove1/remove2\",41]",
                .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_RMDIR_EX, .param = "[\"/home/vagrant/test/test-0/remove1/remove2\",41]",
                .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_RMDIR_EX, .param = "[\"/home/vagrant/test/test-0/remove1\",33]",
                .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_RMDIR_EX, .param = "[\"/home/vagrant/test/test-0/remove1\",33]",
                .resultInt = 0},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                TEST_PATH_STR, STRDEF("localhost"), 22, 250, 250, .user = TEST_USER_STR, .write = true,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
            "new storage /");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - missing");

        const String *pathRemove1 = STRDEF(TEST_PATH "/remove1");

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .errorOnMissing = true), PathRemoveError,
            STORAGE_ERROR_PATH_REMOVE_MISSING, strZ(pathRemove1));
        TEST_RESULT_VOID(storagePathRemoveP(storageTest, pathRemove1, .recurse = true), "ignore missing path");

        // -------------------------------------------------------------------------------------------------------------------------
        String *pathRemove2 = strNewFmt("%s/remove2", strZ(pathRemove1));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - parent/subpath permission denied");

        // mimic existance of pathRemove2 mode 700

        TEST_ERROR_FMT(storagePathRemoveP(storageTest, pathRemove2), PathRemoveError, STORAGE_ERROR_PATH_REMOVE, strZ(pathRemove2));
        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove2, .recurse = true), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": libssh2 error [-31]: libssh2sftp error [3]", strZ(pathRemove2));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - subpath permission denied");

        // mimic chmod 777 pathRemove1

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove2, .recurse = true), PathOpenError,
            STORAGE_ERROR_LIST_INFO ": libssh2 error [-31]: libssh2sftp error [3]", strZ(pathRemove2));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - file in subpath, permission denied");

        String *fileRemove = strNewFmt("%s/remove.txt", strZ(pathRemove2));

        // mimic "sudo chmod 755 %s && sudo touch %s && sudo chmod 777 %s", strZ(pathRemove2), strZ(fileRemove), strZ(fileRemove));

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), PathRemoveError, STORAGE_ERROR_PATH_REMOVE_FILE,
            strZ(fileRemove));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - path with subpath and file removed");

        // mimic chmod 777 pathRemove2

        TEST_RESULT_VOID(storagePathRemoveP(storageTest, pathRemove1, .recurse = true), "remove path");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - unlink LIBSSH2_ERROR_EAGAIN timeout");
        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/remove1\",33,0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("remove2"),
                .resultInt = 7,
                .attrPerms = LIBSSH2_SFTP_S_IFDIR | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG| LIBSSH2_SFTP_S_IRWXO,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 1000, .gid = 1000},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"remove2\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/home/vagrant/test/test-0/remove1/remove2\",41]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/remove1/remove2\",41,0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF("remove.txt"),
                .resultInt = 10,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG | LIBSSH2_SFTP_S_IRWXO,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"remove.txt\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP |
                    LIBSSH2_SFTP_S_IROTH,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID,
                .mtime = 1656434296, .uid = 0, .gid = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/home/vagrant/test/test-0/remove1/remove2/remove.txt\",52]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 30},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/home/vagrant/test/test-0/remove1/remove2/remove.txt\",52]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 30},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                TEST_PATH_STR, STRDEF("localhost"), 22, 25, 25, .user = TEST_USER_STR, .write = true,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
            "new storage /");

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), PathRemoveError, STORAGE_ERROR_PATH_REMOVE_FILE,
            strZ(fileRemove));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - rmdir LIBSSH2_ERROR_EAGAIN timeout");
        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/remove1\",33,0,0,1]"},
            {.function = HRNLIBSSH2_SFTP_READDIR_EX, .param = "[\"\",4096,null,0]", .fileName = STRDEF(""),
                .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_RMDIR_EX, .param = "[\"/home/vagrant/test/test-0/remove1\",33]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 30},
            {.function = HRNLIBSSH2_SFTP_RMDIR_EX, .param = "[\"/home/vagrant/test/test-0/remove1\",33]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 30},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                TEST_PATH_STR, STRDEF("localhost"), 22, 25, 25, .user = TEST_USER_STR, .write = true,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
            "new storage /");

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), PathRemoveError, STORAGE_ERROR_PATH_REMOVE,
            strZ(pathRemove1));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
    }

    // *****************************************************************************************************************************
    if (testBegin("storageLinkCreate()"))
    {
        Storage *storageTest = NULL;
        StorageInfo info = {0};
        const String *backupLabel = STRDEF("20181119-152138F");
        const String *latestLabel = strNewFmt("%s%s",TEST_PATH, "/latest");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // create path to link to
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/20181119-152138F\",42,488]"},
            // create symlink
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"20181119-152138F\",16,\"/home/vagrant/test/test-0/latest\",32,0]",
                .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"20181119-152138F\",16,\"/home/vagrant/test/test-0/latest\",32,0]",
                .symlinkExTarget = STRDEF("/home/vagrant/test/test-0/latest")},
            // get link info
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/latest\",32,1]",
                .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_STAT_EX, .param = "[\"/home/vagrant/test/test-0/latest\",32,1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFLNK | LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID
                    | LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1656434296, .uid = 1000, .gid = 1000, .symlinkExTarget = STRDEF("20181119-152138F")},
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX,
                .param = "[\"/home/vagrant/test/test-0/latest\",32,\"\",4096,1]",
                .symlinkExTarget = STRDEF("20181119-152138F")},
            // assert failure - asserts before libssh2 call
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                TEST_PATH_STR, STRDEF("localhost"), 22, 25, 25, .user = TEST_USER_STR, .write = true,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
            "new storage /");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create symbolic link to BACKUPLABEL");

        TEST_RESULT_VOID(storagePathCreateP(storageTest, backupLabel), "create path to link to");
        TEST_RESULT_VOID(storageLinkCreateP(storageTest, backupLabel, latestLabel), "create symlink");
        TEST_ASSIGN(info, storageInfoP(storageTest, latestLabel, .ignoreMissing = false), "get link info");
        TEST_RESULT_STR(info.linkDestination, backupLabel, "match link destination");
        TEST_ERROR_FMT(
            storageLinkCreateP(storageTest, backupLabel, latestLabel, .linkType = storageLinkHard), AssertError,
            "assertion '(param.linkType == storageLinkSym && storageFeature(this, storageFeatureSymLink)) ||"
                " (param.linkType == storageLinkHard && storageFeature(this, storageFeatureHardLink))' failed");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create symbolic link timeout");

        backupLabel = STRDEF("20181119-152138F");
        latestLabel = strNewFmt("%s%s",TEST_PATH, "/latest");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"20181119-152138F\",16,\"/home/vagrant/test/test-0/latest\",32,0]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 24},
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"20181119-152138F\",16,\"/home/vagrant/test/test-0/latest\",32,0]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 3},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                TEST_PATH_STR, STRDEF("localhost"), 22, 25, 25, .user = TEST_USER_STR, .write = true,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
            "new storage /");

        TEST_ERROR_FMT(
            storageLinkCreateP(storageTest, backupLabel, latestLabel), FileOpenError,
            "unable to create symlink '" TEST_PATH "/latest' to '20181119-152138F'");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove symbolic link");

        backupLabel = STRDEF("20181119-152138F");
        latestLabel = strNewFmt("%s%s",TEST_PATH, "/latest");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // create symlink
            {.function = HRNLIBSSH2_SFTP_SYMLINK_EX, .param = "[\"20181119-152138F\",16,\"/home/vagrant/test/test-0/latest\",32,0]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            // unlink symlink
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/home/vagrant/test/test-0/latest\",32]",
                .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/home/vagrant/test/test-0/latest\",32]",
                .resultInt = LIBSSH2_ERROR_NONE},
            // unlink symlink no error on missing
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/home/vagrant/test/test-0/latest\",32]",
                .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/home/vagrant/test/test-0/latest\",32]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            // unlink symlink error on missing
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/home/vagrant/test/test-0/latest\",32]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            // unlink symlink no error on missing, not no such file
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/home/vagrant/test/test-0/latest\",32]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            // unlink symlink not an sftp error, error on missing
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/home/vagrant/test/test-0/latest\",32]",
                .resultInt = LIBSSH2_ERROR_METHOD_NOT_SUPPORTED},
            // unlink symlink not an sftp error, no error on missing
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/home/vagrant/test/test-0/latest\",32]",
                .resultInt = LIBSSH2_ERROR_METHOD_NOT_SUPPORTED},
            // unlink symlink error on missing, timeout
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/home/vagrant/test/test-0/latest\",32]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 30},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/home/vagrant/test/test-0/latest\",32]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 1},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                TEST_PATH_STR, STRDEF("localhost"), 22, 25, 25, .user = TEST_USER_STR, .write = true,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
            "new storage /");

        TEST_ERROR(
            storageLinkCreateP(storageTest, backupLabel, latestLabel),
            FileOpenError,
            "unable to create symlink '" TEST_PATH "/latest' to '20181119-152138F'");
        TEST_RESULT_VOID(storageRemoveP(storageTest, latestLabel), "remove symlink");
        TEST_RESULT_VOID(storageRemoveP(storageTest, latestLabel), "remove symlink - no error on missing");
        // error on missing
        TEST_ERROR_FMT(
            storageRemoveP(storageTest, latestLabel, .errorOnMissing = true), FileRemoveError,
            "unable to remove '" TEST_PATH "/latest': libssh2 error [-31]: libssh2sftp error [2]");
        // no error on missing, not no such file
        TEST_ERROR_FMT(
            storageRemoveP(storageTest, latestLabel, .errorOnMissing = false), FileRemoveError,
            "unable to remove '" TEST_PATH "/latest': libssh2 error [-31]: libssh2sftp error [3]");
        // error on missing, not an sftp error
        TEST_ERROR_FMT(
            storageRemoveP(storageTest, latestLabel, .errorOnMissing = true), FileRemoveError,
            "unable to remove '" TEST_PATH "/latest'");
        // no error on missing, not an sftp error
        TEST_RESULT_VOID(
            storageRemoveP(storageTest, latestLabel), "remove symlink, no sftp error, no error on missing");
        // error on missing, timeout
        TEST_ERROR_FMT(
            storageRemoveP(storageTest, latestLabel, .errorOnMissing = true), FileRemoveError,
            "unable to remove '" TEST_PATH "/latest'");

        // cover what should be an unreachable branch - linkType = storageLinkHard
        StorageSftp *storageSftp = (StorageSftp *)storageDriver(storageTest);
        StorageInterfaceLinkCreateParam param = {.linkType = storageLinkHard};

        TEST_RESULT_VOID(storageSftpLinkCreate(
            storageSftp, STRDEF("/home/vagrant/test/test-0/path/to/file"), STRDEF("/home/vagrant/test/test-0/path/to/link"),
            param), "noop - force param.linkType = storageLinkHard");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNewRead()"))
    {
        Storage *storageTest = NULL;
        StorageRead *file = NULL;
        const String *fileName = STRDEF(TEST_PATH "/readtest.txt");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // missing sftp error no such file
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/readtest.txt\",38,1,0,0]",
                .resultNull = true, .sleep = 23},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/readtest.txt\",38,1,0,0]",
                .resultNull = true, .sleep = 5},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            // timeout EAGAIN
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/readtest.txt\",38,1,0,0]",
                .resultNull = true, .sleep = 23},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/readtest.txt\",38,1,0,0]",
                .resultNull = true, .sleep = 5},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_OK},
            // error not sftp, not EAGAIN
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/readtest.txt\",38,1,0,0]",
                .resultNull = true, .sleep = 23},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/readtest.txt\",38,1,0,0]",
                .resultNull = true, .sleep = 5},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_METHOD_NOT_SUPPORTED},
            // read success
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/readtest.txt\",38,1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                TEST_PATH_STR, STRDEF("localhost"), 22, 25, 25, .user = TEST_USER_STR, .write = true,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
            "new storage /");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read missing");

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");

        // missing no such file
        TEST_ERROR_FMT(ioReadOpen(storageReadIo(file)), FileMissingError, STORAGE_ERROR_READ_MISSING, strZ(fileName));

        // missing EAGAIN timeout
        TEST_ERROR_FMT(ioReadOpen(storageReadIo(file)), FileOpenError, STORAGE_ERROR_READ_OPEN, strZ(fileName));

        // missing not sftp, not EAGAIN
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), false, "not sftp, not EAGAIN");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read success");

        // mimic creation of TEST_PATH "/readtest.txt"

        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_RESULT_INT(ioReadFd(storageReadIo(file)), -1, "check read fd");
        TEST_RESULT_VOID(ioReadClose(storageReadIo(file)), "close file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
    }

    // *****************************************************************************************************************************
    if (testBegin("storageReadClose()"))
    {
        // ------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("close success via storageReadSftpClose()");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // storageReadSftpClose
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/readtest.txt\",38,1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = 0},
            // close(ioSessionFd()...)
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        Storage *storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 25, 25, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        StorageRead *file = NULL;
        const String *fileName = STRDEF(TEST_PATH "/readtest.txt");
        // mimic creation of fileName

        Buffer *outBuffer = bufNew(2);

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_RESULT_VOID(storageReadSftpClose((StorageReadSftp *)file->driver), "close file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // ------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("close success via close()");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // close(ioSessionFd()...)
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/readtest.txt\",38,1,0,0]"},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 25, 25, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        close(ioSessionFd(((StorageReadSftp *)file->driver)->ioSession));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // ------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("close, null sftpHandle");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // storageReadSftpClose null sftpHandle
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/readtest.txt\",38,1,0,0]"},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 25, 25, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        ((StorageReadSftp *)file->driver)->sftpHandle = NULL;
        TEST_RESULT_VOID(storageReadSftpClose((StorageReadSftp *)file->driver), "close file null sftpHandle");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // ------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("close fail via storageReadSftpClose() EAGAIN");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // storageReadSftpClose
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/readtest.txt\",38,1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 23},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 5},
            // close(ioSessionFd()...)
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 25, 25, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_ERROR(
            storageReadSftpClose((StorageReadSftp *)file->driver), FileCloseError,
            "unable to close file '" TEST_PATH "/readtest.txt' after read: libssh2 errno [-37]");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // ------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("close fail via storageReadSftpClose() sftp error");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // storageReadSftpClose
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/readtest.txt\",38,1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 23},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL, .sleep = 5},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 25, 25, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_ERROR(
            storageReadSftpClose((StorageReadSftp *)file->driver), FileCloseError,
            "unable to close file '" TEST_PATH "/readtest.txt' after read: libssh2 errno [-31] sftp errno [4]");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // ------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageReadSftp()");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // storageReadSftpClose
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/readtest.txt\",38,1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_ERROR(
            storageReadSftp(
                ((StorageReadSftp *)file->driver), outBuffer, false), FileReadError,
            "unable to read '" TEST_PATH "/readtest.txt': sftp errno [4]");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNewWrite()"))
    {
        const String *fileName = STRDEF(TEST_PATH "/sub1/testfile");
        StorageWrite *file = NULL;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("permission denied");

        // mimic existance of /noperm/noperm

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // permission denied
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/noperm/noperm\",39,26,416,0]",
                .resultNull = true, .sleep = 18},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/noperm/noperm\",39,26,416,0]",
                .resultNull = true, .sleep = 18, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        Storage *storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ASSIGN(
            file, storageNewWriteP(storageTest, fileNoPerm, .noAtomic = true, .noCreatePath = true), "new write file (defaults)");
        TEST_ERROR_FMT(ioWriteOpen(storageWriteIo(file)), FileOpenError, STORAGE_ERROR_WRITE_OPEN, strZ(fileNoPerm));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("timeout");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // timeout
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/noperm/noperm\",39,26,416,0]",
                .resultNull = true, .sleep = 18},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/noperm/noperm\",39,26,416,0]",
                .resultNull = true, .sleep = 18, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ASSIGN(
            file,
            storageNewWriteP(storageTest, fileNoPerm, .noAtomic = true, .noCreatePath = true),
            "new write file (defaults)");
        TEST_ERROR_FMT(ioWriteOpen(storageWriteIo(file)), FileOpenError, STORAGE_ERROR_WRITE_OPEN, strZ(fileNoPerm));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("missing");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // missing
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/missing\",33,26,416,0]",
                .resultNull = true, .sleep = 18},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ASSIGN(
            file,
            storageNewWriteP(storageTest, STRDEF("missing"), .noAtomic = true, .noCreatePath = true),
            "new write file (defaults)");
        TEST_ERROR_FMT(ioWriteOpen(storageWriteIo(file)), FileMissingError, STORAGE_ERROR_WRITE_MISSING, TEST_PATH "/missing");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write file - defaults");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true, .sleep = 18,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub1\",30,488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                    LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1, .uid = 1000, .gid = 1000, .filesize = 8},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                    LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1, .uid = 1000, .gid = 1000, .filesize = 8},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
                .param =
                    "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,\"/home/vagrant/test/test-0/sub1/testfile\","
                        "39,7]",
                .resultInt = 0},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ASSIGN(
            file,
            storageNewWriteP(storageTest, fileName, .user = TEST_USER_STR, .group = TEST_GROUP_STR, .timeModified = 1),
            "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("timeout on fsync");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true, .sleep = 18,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub1\",30,488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                    LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1, .uid = 1000, .gid = 1000, .filesize = 8},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 20},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 7},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        const String *fileNameTmp = STRDEF(TEST_PATH "/sub1/testfile.pgbackrest.tmp");

        TEST_ASSIGN(
            file,
            storageNewWriteP(storageTest, fileName, .user = TEST_USER_STR, .group = TEST_GROUP_STR, .timeModified = 1),
            "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_ERROR_FMT(ioWriteClose(storageWriteIo(file)), FileSyncError, STORAGE_ERROR_WRITE_SYNC, strZ(fileNameTmp));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("timeout on rename");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true, .sleep = 18,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub1\",30,488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                    LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1, .uid = 1000, .gid = 1000, .filesize = 8},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                    LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1, .uid = 1000, .gid = 1000, .filesize = 8},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
                .param =
                    "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,\"/home/vagrant/test/test-0/sub1/testfile\","
                        "39,7]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 18},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
                .param =
                    "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,\"/home/vagrant/test/test-0/sub1/testfile\","
                        "39,7]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 5},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_OK},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ASSIGN(
            file,
            storageNewWriteP(storageTest, fileName, .user = TEST_USER_STR, .group = TEST_GROUP_STR, .timeModified = 1),
            "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_ERROR_FMT(
                ioWriteClose( storageWriteIo(file)),
                FileCloseError, "unable to move '%s' to '%s': libssh2 error [-37]", strZ(fileNameTmp), strZ(fileName));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftp error on rename, other than LIBSSH2_FX_FAILURE");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true, .sleep = 18,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub1\",30,488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                    LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1, .uid = 1000, .gid = 1000, .filesize = 8},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                    LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1, .uid = 1000, .gid = 1000, .filesize = 8},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
                .param =
                    "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,\"/home/vagrant/test/test-0/sub1/testfile\","
                        "39,7]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_CONNECTION_LOST},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_CONNECTION_LOST},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ASSIGN(
            file,
            storageNewWriteP(storageTest, fileName, .user = TEST_USER_STR, .group = TEST_GROUP_STR, .timeModified = 1),
            "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_ERROR_FMT(
                ioWriteClose( storageWriteIo(file)),
                FileCloseError, "unable to move '%s' to '%s': libssh2 error [-31]: libssh2sftp error [7]", strZ(fileNameTmp),
                strZ(fileName));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("unable to set ownership storageWriteSftpOpen");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true, .sleep = 18,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub1\",30,488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 23},
            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]", .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 4},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        fileNameTmp = STRDEF(TEST_PATH "/sub1/testfile.pgbackrest.tmp");

        TEST_ASSIGN(
            file,
            storageNewWriteP(
                storageTest, fileName, .user = TEST_USER_STR, .group = TEST_GROUP_STR, .timeModified = 1, .noSyncFile = true),
            "new write file (defaults)");
        TEST_ERROR_FMT(ioWriteOpen(storageWriteIo(file)), FileOwnerError, "unable to set ownership for '%s'", strZ(fileNameTmp));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageWriteSftpOpen group NULL");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true, .sleep = 18,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub1\",30,488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]", .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        fileNameTmp = STRDEF(TEST_PATH "/sub1/testfile.pgbackrest.tmp");

        TEST_ASSIGN(
            file,
            storageNewWriteP(
                storageTest, fileName, .user = TEST_USER_STR, .group = NULL, .timeModified = 1, .noSyncFile = true),
            "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageWriteSftpOpen user NULL");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true, .sleep = 18,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub1\",30,488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]", .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        fileNameTmp = STRDEF(TEST_PATH "/sub1/testfile.pgbackrest.tmp");

        TEST_ASSIGN(
            file,
            storageNewWriteP(storageTest, fileName, .user = NULL, .group = TEST_GROUP_STR, .timeModified = 1, .noSyncFile = true),
            "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageWriteSftpClose timeout EAGAIN libssh2_sftp_fsetstat");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true, .sleep = 18,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub1\",30,488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                    LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1, .uid = 1000, .gid = 1000, .filesize = 8},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},

            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 18},
            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 5},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ASSIGN(
            file,
            storageNewWriteP(storageTest, fileName, .user = TEST_USER_STR, .group = TEST_GROUP_STR, .timeModified = 1),
            "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_ERROR_FMT(
            ioWriteClose(storageWriteIo(file)), FileInfoError, "unable to set time for '%s': libssh2 error [0] ",
            strZ(fileNameTmp));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageWriteSftpClose() timeout EAGAIN on libssh2_sftp_close");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true, .sleep = 18,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub1\",30,488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                    LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1, .uid = 1000, .gid = 1000, .filesize = 8},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                    LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1, .uid = 1000, .gid = 1000, .filesize = 8},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 18},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 5},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ASSIGN(
            file,
            storageNewWriteP(storageTest, fileName, .user = TEST_USER_STR, .group = TEST_GROUP_STR, .timeModified = 1),
            "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_ERROR_FMT(
            ioWriteClose(storageWriteIo(file)), FileCloseError, STORAGE_ERROR_WRITE_CLOSE ": libssh2 error [0] ",
            strZ(fileNameTmp));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write file - noAtomic = true");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile\",39,26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub1\",30,488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile\",39,26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                    LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1, .uid = 1000, .gid = 1000, .filesize = 8},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                    LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1, .uid = 1000, .gid = 1000, .filesize = 8},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
       });

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ASSIGN(
            file,
            storageNewWriteP(storageTest, fileName, .user = TEST_USER_STR, .noAtomic = true, .group = TEST_GROUP_STR,
                .timeModified = 1),
            "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write file - syncPath not NULL, .noSyncPath = true");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile\",39,26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub1\",30,488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile\",39,26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                    LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1, .uid = 1000, .gid = 1000, .filesize = 8},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                    LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1, .uid = 1000, .gid = 1000, .filesize = 8},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
       });

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        // make interface.pathSync != NULL
        ((StorageSftp *)storageDriver(storageTest))->interface.pathSync = malloc(1);

        TEST_ASSIGN(
            file,
            storageNewWriteP(storageTest, fileName, .user = TEST_USER_STR, .noAtomic = true, .group = TEST_GROUP_STR,
                .timeModified = 1, .noSyncPath = true),
            "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");

        free(((StorageSftp *)storageDriver(storageTest))->interface.pathSync);

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write file - syncPath not NULL, .noSyncPath = false");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile\",39,26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub1\",30,488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile\",39,26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                    LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1, .uid = 1000, .gid = 1000, .filesize = 8},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                    LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1, .uid = 1000, .gid = 1000, .filesize = 8},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
       });

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        // make interface.pathSync != NULL
        ((StorageSftp *)storageDriver(storageTest))->interface.pathSync = malloc(1);

        TEST_ASSIGN(
            file,
            storageNewWriteP(storageTest, fileName, .user = TEST_USER_STR, .noAtomic = true, .group = TEST_GROUP_STR,
                .timeModified = 1, .noSyncPath = false),
            "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");

        free(((StorageSftp *)storageDriver(storageTest))->interface.pathSync);

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write file - syncFile false");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile\",39,26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub1\",30,488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile\",39,26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                    LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1, .uid = 1000, .gid = 1000, .filesize = 8},
            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                    LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1, .uid = 1000, .gid = 1000, .filesize = 8},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
       });

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        ((StorageSftp *)storageDriver(storageTest))->interface.pathSync = malloc(1);

        TEST_ASSIGN(
            file,
            storageNewWriteP(storageTest, fileName, .user = TEST_USER_STR, .noAtomic = true, .group = TEST_GROUP_STR,
                .timeModified = 1, .noSyncPath = false),
            "new write file (defaults)");

        ((StorageWriteSftp *)file->driver)->interface.syncFile = false;

        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");

        free(((StorageSftp *)storageDriver(storageTest))->interface.pathSync);

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageWriteSftpClose() - null sftpHandle");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile\",39,26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub1\",30,488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile\",39,26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSTAT_EX, .param = "[1]",
                .resultInt = LIBSSH2_ERROR_NONE,
                .attrPerms = LIBSSH2_SFTP_S_IFREG | LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP,
                .flags = LIBSSH2_SFTP_ATTR_PERMISSIONS | LIBSSH2_SFTP_ATTR_ACMODTIME | LIBSSH2_SFTP_ATTR_UIDGID |
                    LIBSSH2_SFTP_ATTR_SIZE,
                .mtime = 1, .uid = 1000, .gid = 1000, .filesize = 8},
            HRNLIBSSH2_MACRO_SHUTDOWN()
       });

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ASSIGN(
            file,
            storageNewWriteP(storageTest, fileName, .user = TEST_USER_STR, .noAtomic = true, .group = TEST_GROUP_STR,
                .timeModified = 1, .noSyncPath = false),
            "new write file (defaults)");

        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");

        // make sftpHandle NULL
        ((StorageWriteSftp *)file->driver)->sftpHandle = NULL;

        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePut() and storageGet()"))
    {
        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0\",25,1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 18},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL, .sleep = 5},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        Storage *storageTest = storageSftpNewP(
            FSLASH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get error - attempt to get directory");

        TEST_ERROR(
            storageGetP(storageNewReadP(storageTest, TEST_PATH_STR, .ignoreMissing = false)), FileReadError,
            "unable to read '" TEST_PATH "': sftp errno [4]");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - empty file");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/test.empty.pgbackrest.tmp\",51,26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
                .param =
                    "[\"/home/vagrant/test/test-0/test.empty.pgbackrest.tmp\",51,\"/home/vagrant/test/test-0/test.empty\",36,7]",
                .resultInt = 0},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            FSLASH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        const String *emptyFile = STRDEF(TEST_PATH "/test.empty");
        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, emptyFile), NULL), "put empty file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - empty file, already exists");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/test.empty.pgbackrest.tmp\",51,26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
                .param =
                    "[\"/home/vagrant/test/test-0/test.empty.pgbackrest.tmp\",51,\"/home/vagrant/test/test-0/test.empty\",36,7]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL, .sleep = 18 },
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            // storageWriteSftpUnlinkExisting()
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX,
                .param = "[\"/home/vagrant/test/test-0/test.empty\",36]", .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 18},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX,
                .param = "[\"/home/vagrant/test/test-0/test.empty\",36]", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL, .sleep = 5},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_OK},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            FSLASH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ERROR(
            storagePutP(storageNewWriteP(storageTest, emptyFile), NULL), FileRemoveError,
            "unable to remove existing '" TEST_PATH "/test.empty': libssh2 error [-31]: libssh2sftp error [0]");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - empty file, remove already existing file, storageWriteSftpRename timeout EAGAIN");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/test.empty.pgbackrest.tmp\",51,26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
                .param =
                    "[\"/home/vagrant/test/test-0/test.empty.pgbackrest.tmp\",51,\"/home/vagrant/test/test-0/test.empty\",36,7]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL, .sleep = 18 },
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX,
                .param = "[\"/home/vagrant/test/test-0/test.empty\",36]", .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 18},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX,
                .param = "[\"/home/vagrant/test/test-0/test.empty\",36]", .resultInt = LIBSSH2_ERROR_NONE, .sleep = 5},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
                .param =
                    "[\"/home/vagrant/test/test-0/test.empty.pgbackrest.tmp\",51,\"/home/vagrant/test/test-0/test.empty\",36,7]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 18 },
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
                .param =
                    "[\"/home/vagrant/test/test-0/test.empty.pgbackrest.tmp\",51,\"/home/vagrant/test/test-0/test.empty\",36,7]",
                .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 5 },
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_OK},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            FSLASH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ERROR(
            storagePutP(storageNewWriteP(storageTest, emptyFile), NULL), FileRemoveError,
            "unable to move '" TEST_PATH "/test.empty.pgbackrest.tmp' to '"
                TEST_PATH "/test.empty': libssh2 error [-37]");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - empty file, EAGAIN fail on remove already existing file");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/test.empty.pgbackrest.tmp\",51,26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
                .param =
                    "[\"/home/vagrant/test/test-0/test.empty.pgbackrest.tmp\",51,\"/home/vagrant/test/test-0/test.empty\",36,7]",
                .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL, .sleep = 18 },
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_FAILURE},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX,
                .param = "[\"/home/vagrant/test/test-0/test.empty\",36]", .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 18},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX,
                .param = "[\"/home/vagrant/test/test-0/test.empty\",36]", .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 5},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_OK},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            FSLASH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ERROR(
            storagePutP(storageNewWriteP(storageTest, emptyFile), NULL), FileRemoveError,
            "unable to remove existing '" TEST_PATH "/test.empty': libssh2 error [-37]");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - file with contents - timeout EAGAIN libssh2_sftp_write");

        const Buffer *failBuffer = BUFSTRDEF("FAIL\n");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/test.txt.pgbackrest.tmp\",49,26,416,0]"},
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[2]", .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 18},
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[2]", .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 5},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            FSLASH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ERROR(
            storagePutP(storageNewWriteP(storageTest, STRDEF(TEST_PATH "/test.txt")), failBuffer), FileWriteError,
            "unable to write '" TEST_PATH "/test.txt.pgbackrest.tmp'");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - file with contents");

        const Buffer *buffer = BUFSTRDEF("TESTFILE\n");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/test.txt.pgbackrest.tmp\",49,26,416,0]"},
            // not passing buffer param, see shim function, initial passed buffer contains random data
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[2]", .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 18},
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[2]", .resultInt = 2, .sleep = 5},
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[2]", .resultInt = 2},
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[2]", .resultInt = 2},
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[2]", .resultInt = 1},
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[1]", .resultInt = 1},
            {.function = HRNLIBSSH2_SFTP_WRITE, .param = "[1]", .resultInt = 1},
            {.function = HRNLIBSSH2_SFTP_FSYNC, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE, .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_RENAME_EX,
                .param =
                    "[\"/home/vagrant/test/test-0/test.txt.pgbackrest.tmp\",49,\"/home/vagrant/test/test-0/test.txt\",34,7]",
                .resultInt = 0},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            FSLASH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, STRDEF(TEST_PATH "/test.txt")), buffer), "put test file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - ignore missing");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/BOGUS\",31,1,0,0]", .resultNull = true,
                .sleep = 18},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/BOGUS\",31,1,0,0]", .resultNull = true,
                .sleep = 5},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            FSLASH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_RESULT_PTR(
            storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/" BOGUS_STR), .ignoreMissing = true)), NULL,
            "get missing file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - empty file");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/test.empty\",36,1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = LIBSSH2_ERROR_NONE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            FSLASH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.empty"))), "get empty");
        TEST_RESULT_UINT(bufSize(buffer), 0, "size is 0");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - file with contents");

        const Buffer *outBuffer = bufNew(2);

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/test.txt\",34,1,0,0]"},
            // not passing buffer param, see shim function, initial passed buffer contains random data
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
            FSLASH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ASSIGN(outBuffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"))), "get text");
        TEST_RESULT_UINT(bufSize(outBuffer), 9, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(outBuffer), "TESTFILE\n", bufSize(outBuffer)) == 0, true, "check content");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - exact size smaller");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/test.txt\",34,1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = 2, .readBuffer = STRDEF("TE")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = 2, .readBuffer = STRDEF("ST")},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            FSLASH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt")), .exactSize = 4), "get exact");
        TEST_RESULT_UINT(bufSize(buffer), 4, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "TEST", bufSize(buffer)) == 0, true, "check content");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - exact size larger");

        ioBufferSizeSet(4);
        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/test.txt\",34,1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[4]", .resultInt = 4, .readBuffer = STRDEF("TEST")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[4]", .resultInt = 4, .readBuffer = STRDEF("FILE")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[4]", .resultInt = 1, .readBuffer = STRDEF("\n")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[3]", .resultInt = 0},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            FSLASH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ERROR(
            storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt")), .exactSize = 64), FileReadError,
            "unable to read 64 byte(s) from '" TEST_PATH "/test.txt'");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - smaller buffer size");

        ioBufferSizeSet(2);

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/test.txt\",34,1,0,0]"},
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
            FSLASH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"))), "get text");
        TEST_RESULT_UINT(bufSize(buffer), 9, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "TESTFILE\n", bufSize(buffer)) == 0, true, "check content");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid read offset bytes");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/test.txt\",34,1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_SEEK64, .param = "[18446744073709551615]"},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_BAD_MESSAGE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            FSLASH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ERROR(
            storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"), .offset = UINT64_MAX)), FileOpenError,
            "unable to seek to 18446744073709551615 in file '" TEST_PATH "/test.txt'");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid read");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/test.txt\",34,1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_BAD_MESSAGE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            FSLASH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ERROR(
            storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"))), FileReadError,
            "unable to read '" TEST_PATH "/test.txt': sftp errno [5]");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read limited bytes");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/test.txt\",34,1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = 2, .readBuffer = STRDEF("TE")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = 2, .readBuffer = STRDEF("ST")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = 2, .readBuffer = STRDEF("FI")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[1]", .resultInt = 1, .readBuffer = STRDEF("LE")},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            FSLASH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"), .limit = VARUINT64(7))), "get");
        TEST_RESULT_UINT(bufSize(buffer), 7, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "TESTFIL", bufSize(buffer)) == 0, true, "check content");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read offset bytes");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/test.txt\",34,1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_SEEK64, .param = "[4]"},
            // simulate seeking offset 4
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = 2, .readBuffer = STRDEF("FI")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = 2, .readBuffer = STRDEF("LE")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = 1, .readBuffer = STRDEF("\n")},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[1]", .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            FSLASH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);


        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"), .offset = 4)), "get");
        TEST_RESULT_UINT(bufSize(buffer), 5, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "FILE\n", bufSize(buffer)) == 0, true, "check content");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - read timeout EAGAIN");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/test.txt\",34,1,0,0]"},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 18},
            {.function = HRNLIBSSH2_SFTP_READ, .param = "[2]", .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 5},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            FSLASH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ERROR(
            storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"))), FileReadError,
            "unable to read '" TEST_PATH "/test.txt'");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
    }

    // *****************************************************************************************************************************
    if (testBegin("storageRemove()"))
    {
        // mimic existance of /noperm/noperm

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove - file missing");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/missing\",8]", .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_UNLINK_EX, .param = "[\"/missing\",8]", .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        Storage *storageTest = storageSftpNewP(
            FSLASH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_RESULT_VOID(storageRemoveP(storageTest, STRDEF("missing")), "remove missing file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageWriteSftpOpen libssh2_sftp_open_ex timeout EAGAIN");

        StorageWrite *file = NULL;
        const String *fileName = STRDEF(TEST_PATH "/sub1/testfile");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true, .sleep = 18,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub1\",30,488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true, .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 18,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true, .resultInt = LIBSSH2_ERROR_EAGAIN, .sleep = 5,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_EAGAIN},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ASSIGN(
            file,
            storageNewWriteP(
                storageTest, fileName, .user = TEST_USER_STR, .group = TEST_GROUP_STR, .timeModified = 1, .noSyncFile = true),
            "storageWriteSftpOpen timeout EAGAIN");
        TEST_ERROR_FMT(ioWriteOpen(storageWriteIo(file)), FileOpenError, STORAGE_ERROR_WRITE_OPEN, strZ(fileName));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageWriteSftpOpen libssh2_sftp_open_ex sftp error");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true, .sleep = 18,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_MKDIR_EX, .param = "[\"/home/vagrant/test/test-0/sub1\",30,488]"},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .resultNull = true, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL, .sleep = 18,
                .param = "[\"/home/vagrant/test/test-0/sub1/testfile.pgbackrest.tmp\",54,26,416,0]"},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        TEST_ASSIGN(
            file,
            storageNewWriteP(
                storageTest, fileName, .user = TEST_USER_STR, .group = TEST_GROUP_STR, .timeModified = 1, .noSyncFile = true),
            "storageWriteSftpOpen sftp error");
        TEST_ERROR_FMT(ioWriteOpen(storageWriteIo(file)), FileOpenError, STORAGE_ERROR_WRITE_OPEN, strZ(fileName));

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
    }

    // *****************************************************************************************************************************
    if (testBegin("StorageRead"))
    {
        Storage *storageTest = NULL;
        StorageRead *file = NULL;

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // ignore missing - file with no permission to read
               // new read file, check ignore missing, check name require no libssh2 calls
            // permission denied
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/noperm/noperm\",39,1,0,0]",
                .resultNull = true, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL, .sleep = 18},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/noperm/noperm\",39,1,0,0]",
                .resultNull = true, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL, .sleep = 8},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_PERMISSION_DENIED},
            // file missing
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/test.file\",35,1,0,0]",
                .resultNull = true, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL, .sleep = 18},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/test.file\",35,1,0,0]",
                .resultNull = true, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL, .sleep = 8},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            // ignore missing
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/test.file\",35,1,0,0]",
                .resultNull = true, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL, .sleep = 18},
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/test.file\",35,1,0,0]",
                .resultNull = true, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL, .sleep = 8},
            {.function = HRNLIBSSH2_SESSION_LAST_ERRNO, .resultInt = LIBSSH2_ERROR_SFTP_PROTOCOL},
            {.function = HRNLIBSSH2_SFTP_LAST_ERROR, .resultUInt = LIBSSH2_FX_NO_SUCH_FILE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("ignore missing - file with no permission to read");

        // mimic existance of /noperm/noperm

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileNoPerm, .ignoreMissing = true), "new read file");
        TEST_RESULT_BOOL(storageReadIgnoreMissing(file), true, "check ignore missing");
        TEST_RESULT_STR(storageReadName(file), fileNoPerm, "check name");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("permission denied");

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileNoPerm), "new no perm read file");
        TEST_ERROR_FMT(ioReadOpen(storageReadIo(file)), FileOpenError, STORAGE_ERROR_READ_OPEN, strZ(fileNoPerm));

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

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            // open file
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0/test.file\",35,1,0,0]",
                .resultInt = LIBSSH2_ERROR_NONE},
            // check file name, check file type, check offset, check limit require no libssh2 calls
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
            FSLASH_STR, STRDEF("localhost"), 22, 5000, 5000, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"), .write = true);

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
        TEST_RESULT_BOOL(storageReadSftpEof((StorageReadSftp *)file->driver), false, "storageReadSftpEof eof false");

        TEST_RESULT_VOID(ioRead(storageReadIo(file), outBuffer), "load data");
        bufCat(buffer, outBuffer);
        bufUsedZero(outBuffer);

        TEST_RESULT_VOID(ioRead(storageReadIo(file), outBuffer), "no data to load");
        TEST_RESULT_UINT(bufUsed(outBuffer), 0, "buffer is empty");

        TEST_RESULT_VOID(storageReadSftp(file->driver, outBuffer, true), "no data to load from driver either");
        TEST_RESULT_UINT(bufUsed(outBuffer), 0, "buffer is empty");

        TEST_RESULT_BOOL(bufEq(buffer, expectedBuffer), true, "check file contents (all loaded)");

        TEST_RESULT_BOOL(ioReadEof(storageReadIo(file)), true, "eof");
        TEST_RESULT_BOOL(ioReadEof(storageReadIo(file)), true, "still eof");
        TEST_RESULT_BOOL(storageReadSftpEof((StorageReadSftp *)file->driver), true, "storageReadSftpEof eof true");

        TEST_RESULT_VOID(ioReadClose(storageReadIo(file)), "close file");

        TEST_RESULT_VOID(storageReadFree(storageNewReadP(storageTest, fileName)), "free file");

        TEST_RESULT_VOID(storageReadMove(NULL, memContextTop()), "move null file");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
    }

    // *****************************************************************************************************************************
    if (testBegin("storageRepo*()"))
    {
        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        // Load configuration to set repo-path and stanza
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpAccount, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyfile, "/home/vagrant/.ssh/id_rsa");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyfile, "/home/vagrant/.ssh/id_rsa.pub");
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

        memContextFree(objMemContext((StorageSftp *)storageDriver(storage)));
    }

    // *****************************************************************************************************************************
    if (testBegin("libssh2SessionFreeResource()"))
    {
        Storage *storageTest = NULL;
        StorageSftp *storageSftp = NULL;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("sftpHandle not NULL, sftpSession && session NULL, force branch tests");

        harnessLibssh2ScriptSet((HarnessLibssh2 [])
        {
            HRNLIBSSH2_MACRO_STARTUP(),
            {.function = HRNLIBSSH2_SFTP_OPEN_EX, .param = "[\"/home/vagrant/test/test-0\",25,1,0,1]"},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE},
            {.function = HRNLIBSSH2_SESSION_DISCONNECT_EX, .param ="[11,\"pgbackrest instance shutdown\",\"\"]", .resultInt = 0},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE},
            {.function = HRNLIBSSH2_SFTP_CLOSE_HANDLE},
            HRNLIBSSH2_MACRO_SHUTDOWN()
        });

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                STRDEF("/tmp"), STRDEF("localhost"), 22, 20, 20, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub")),
             "new storage (defaults)");

        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "assign storage");

        // Populate sftpHandle, NULL sftpSession and session
        storageSftp->sftpHandle = libssh2_sftp_open_ex(storageSftp->sftpSession, "/home/vagrant/test/test-0",25,1,0,1);
        storageSftp->sftpSession = NULL;
        storageSftp->session = NULL;

        TEST_RESULT_VOID(libssh2SessionFreeResource(storageSftp), "freeResource not NULL sftpHandle");

        memContextFree(objMemContext((StorageSftp *)storageDriver(storageTest)));
    }

    // *****************************************************************************************************************************
    if (testBegin("storageSftpEvalLibssh2Error()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSftpEvalLibssh2Error()");

        TEST_ERROR_FMT(
            storageSftpEvalLibssh2Error(
                -11, 16, &FileRemoveError, strNewFmt("unable to move '%s' to '%s'", "BOGUS", "NOT BOGUS"), STRDEF("HINT")),
            FileRemoveError,
            "unable to move 'BOGUS' to 'NOT BOGUS': libssh2 error [-11]\n"
            "HINT");
        TEST_ERROR_FMT(
            storageSftpEvalLibssh2Error(
                LIBSSH2_ERROR_SFTP_PROTOCOL, 16, &FileRemoveError,
                strNewFmt("unable to move '%s' to '%s'", "BOGUS", "NOT BOGUS"), STRDEF("HINT")),
            FileRemoveError,
            "unable to move 'BOGUS' to 'NOT BOGUS': libssh2 error [-31]: libssh2sftp error [16]\n"
            "HINT");
        TEST_ERROR_FMT(
            storageSftpEvalLibssh2Error(
                LIBSSH2_ERROR_SFTP_PROTOCOL, 16, &FileRemoveError,
                strNewFmt("unable to move '%s' to '%s'", "BOGUS", "NOT BOGUS"), NULL),
            FileRemoveError,
            "unable to move 'BOGUS' to 'NOT BOGUS': libssh2 error [-31]: libssh2sftp error [16]");
        TEST_ERROR_FMT(
            storageSftpEvalLibssh2Error(
                LIBSSH2_ERROR_SFTP_PROTOCOL, 16, &FileRemoveError, NULL, NULL),
            FileRemoveError,
            "libssh2 error [-31]: libssh2sftp error [16]");
    }

    hrnSftpIoSessionFdShimUninstall();
    FUNCTION_HARNESS_RETURN_VOID();
}
