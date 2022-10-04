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
Macros to create/remove a path and file that cannot be accessed
***********************************************************************************************************************************/
#define TEST_CREATE_NOPERM()                                                                                                       \
    HRN_SYSTEM_FMT(                                                                                                                \
        "sudo mkdir -m 700 %s && sudo touch %s && sudo chmod 600 %s", strZ(pathNoPerm), strZ(fileNoPerm), strZ(fileNoPerm))

#define TEST_REMOVE_NOPERM()                                                                                                       \
    HRN_SYSTEM_FMT("sudo rm -rf %s", strZ(pathNoPerm))

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

//    HRN_SYSTEM_FMT("sudo perl -pi.bak -e 's#Subsystem.*sftp.*/usr/lib/openssh/sftp-server#Subsystem    sftp    /usr/lib/openssh/sftp-server -l VERBOSE#' /etc/ssh/sshd_config && sudo service ssh restart > /dev/null 2>&1");
    // Create default storage object for testing
    // jrt !!!  -- write test with small timout to error on EAGAIN and cover EAGAIN paths
    // jrt !!! need to implement user/pwd or keypair auth for the vm's
    /*
    Storage *storageTest = storageSftpNewP(
        TEST_PATH_STR, STRDEF("localhost"), 22, 5000, 5000, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.pub"),
        .write = true);
        */
    /*
    Storage *storageTest = storageSftpNewP(
        TEST_PATH_STR, STRDEF("localhost"), 22, 5000, 5000, .user = TEST_USER_STR, .password = TEST_USER_STR,
        .write = true);
        */

    Storage *storageTest = storageSftpNewP(
        TEST_PATH_STR, STRDEF("localhost"), 22, 10000, 10000, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
        .keyPub = STRDEF("/home/vagrant/.ssh/authorized_keys"), .write = true);

    ioBufferSizeSet(2);

    // Directory and file that cannot be accessed to test permissions errors
#ifdef TEST_CONTAINER_REQUIRED
    const String *fileNoPerm = STRDEF(TEST_PATH "/noperm/noperm");
    String *pathNoPerm = strPath(fileNoPerm);
#endif // TEST_CONTAINER_REQUIRED

    // Write file for testing if storage is read-only
    const String *writeFile = STRDEF(TEST_PATH "/writefile");

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
        // Set timeouts to 1 to force a handshake failure
        TEST_ERROR(
            storageSftpNewP(TEST_PATH_STR, STRDEF("localhost"), 22, 1, 1, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/authorized_keys")),
            ServiceError, "libssh2 handshake failed: [11] Resource temporarily unavailable");

            /*
        TEST_ERROR(
            storageSftpNewP(STRDEF("/tmp"), STRDEF("localhost"), 22, 1, 1, .user = TEST_USER_STR, .password = TEST_USER_STR),
            ServiceError, "libssh2 handshake failed: [11] Resource temporarily unavailable");

        TEST_ERROR(
            storageSftpNewP(TEST_PATH_STR, STRDEF("localhost"), 22, 5000, 5000, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/authorized_keys")),
            ServiceError, "error in password authenticate libssh2 error [-18]: [11] Resource temporarily unavailable");
            */

        TEST_ERROR(
            storageSftpNewP(TEST_PATH_STR, STRDEF("localhost"), 22, 3000, 3000, .write = true), ServiceError,
            "unable to init libssh2_sftp session: [11] Resource temporarily unavailable");

        TEST_ERROR(
            storageSftpNewP(STRDEF("/tmp"), STRDEF("localhost"), 22, 5000, 5000, .user = TEST_USER_STR, .keyPriv = TEST_USER_STR,
            .keyPub = TEST_USER_STR), ServiceError, "public key from file authentication failed libssh2 error [-16]");
        TEST_ERROR(
            storageSftpNewP(STRDEF("/tmp"), STRDEF("localhost"), 22, 5000, 5000, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/id_rsa.doesnotexist")),
            ServiceError, "public key from file authentication failed libssh2 error [-16]");

        /* Non PEM format keypair failure
        // Put the non pem private key
        storagePutP(storageNewWriteP(storageTest, STRDEF("id_rsa_non_pem")), BUFSTRZ(nonPemRsaPrivateKey));

        // Put the non pem public key
        storagePutP(storageNewWriteP(storageTest, STRDEF("id_rsa_pub_non_pem")), BUFSTRZ(nonPemRsaPublicKey));

        TEST_ERROR(
            storageSftpNewP(STRDEF("/tmp"), STRDEF("localhost"), 22, 5000, 5000, .user = TEST_USER_STR,
                .keyPriv = STRDEF(TEST_PATH "/id_rsa_non_pem"),
                .keyPub = STRDEF(TEST_PATH "/id_rsa_pub_non_pem")),
            ServiceError, "public key from file authentication failed libssh2 error [-16]");
            */

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create new storage with defaults");

        Storage *storageTest = NULL;
        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                STRDEF("/tmp"), STRDEF("localhost"), 22, 5000, 5000, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/authorized_keys")),
             "new storage (defaults)");
        TEST_RESULT_STR_Z(storageTest->path, "/tmp", "check path");
        TEST_RESULT_INT(storageTest->modeFile, 0640, "check file mode");
        TEST_RESULT_INT(storageTest->modePath, 0750, "check path mode");
        TEST_RESULT_BOOL(storageTest->write, false, "check write");
        TEST_RESULT_BOOL(storageTest->pathExpressionFunction == NULL, true, "check expression function is not set");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create new storage - override defaults");

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                STRDEF("/path/to"), STRDEF("localhost"), 22, 5000, 5000, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/authorized_keys"),
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
    }

    // *****************************************************************************************************************************
    if (testBegin("storageExists() and storagePathExists()"))
    {
#ifdef TEST_CONTAINER_REQUIRED
        TEST_CREATE_NOPERM();
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file");

        TEST_RESULT_BOOL(storageExistsP(storageTest, STRDEF("missing")), false, "file does not exist");
        TEST_RESULT_BOOL(storageExistsP(storageTest, STRDEF("missing"), .timeout = 1000), false, "file does not exist");
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path");

        TEST_RESULT_BOOL(storagePathExistsP(storageTest, STRDEF("missing")), false, "path does not exist");
        TEST_RESULT_BOOL(storagePathExistsP(storageTest, NULL), true, "test path exists");

#ifdef TEST_CONTAINER_REQUIRED
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("permission denied");

        TEST_ERROR_FMT(storageExistsP(storageTest, fileNoPerm), FileOpenError, STORAGE_ERROR_INFO, strZ(fileNoPerm));
        TEST_ERROR_FMT(storagePathExistsP(storageTest, fileNoPerm), FileOpenError, STORAGE_ERROR_INFO, strZ(fileNoPerm));
//        TEST_REMOVE_NOPERM();
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file and path");

        const String *fileExists = STRDEF(TEST_PATH "/exists");
        const String *pathExists = STRDEF(TEST_PATH "/pathExists");
        HRN_SYSTEM_FMT("touch %s", strZ(fileExists));
        HRN_SYSTEM_FMT("mkdir %s", strZ(pathExists));

        TEST_RESULT_BOOL(storageExistsP(storageTest, fileExists), true, "file exists");
        TEST_RESULT_BOOL(storageExistsP(storageTest, pathExists), false, "not a file");
        TEST_RESULT_BOOL(storagePathExistsP(storageTest, fileExists), false, "not a path");
        HRN_SYSTEM_FMT("rm %s", strZ(fileExists));

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
    }

    // *****************************************************************************************************************************
    if (testBegin("storageInfo()"))
    {
#ifdef TEST_CONTAINER_REQUIRED
        TEST_CREATE_NOPERM();

        TEST_ERROR_FMT(storageInfoP(storageTest, fileNoPerm), FileOpenError, STORAGE_ERROR_INFO, strZ(fileNoPerm));
//        TEST_REMOVE_NOPERM();
#endif // TEST_CONTAINER_REQUIRED

        // -----------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info for / exists");

        TEST_RESULT_BOOL(
            storageInfoP(
                storageSftpNewP(
                FSLASH_STR, STRDEF("localhost"), 22, 5000, 5000, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/authorized_keys")),
                NULL).exists,
            true, "info for /");

        // -----------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info for / does not exist with no path feature");

        Storage *storageRootNoPath = storageSftpNewP(
            FSLASH_STR, STRDEF("localhost"), 22, 5000, 5000, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/authorized_keys"));
        storageRootNoPath->pub.interface.feature ^= 1 << storageFeaturePath;

        TEST_RESULT_BOOL(storageInfoP(storageRootNoPath, NULL, .ignoreMissing = true).exists, false, "no info for /");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("directory does not exists");

        const String *fileName = STRDEF(TEST_PATH "/fileinfo");

        //!!! validate/verify this change -- sftp returns 2 file not found, should we overwrite errno with 2
        TEST_ERROR_FMT(
            storageInfoP(
                storageTest, fileName), FileOpenError, STORAGE_ERROR_INFO_MISSING ": [11] Resource temporarily unavailable",
            strZ(fileName));

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

        const Buffer *buffer = BUFSTRDEF("TESTFILE");
        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, fileName), buffer), "put test file");

        HRN_STORAGE_TIME(storageTest, strZ(fileName), 1555155555);

#ifdef TEST_CONTAINER_REQUIRED
        HRN_SYSTEM_FMT("sudo chown 99999:99999 %s", strZ(fileName));
#endif // TEST_CONTAINER_REQUIRED

        TEST_ASSIGN(info, storageInfoP(storageTest, fileName), "get file info");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypeFile, "check type");
        TEST_RESULT_UINT(info.size, 8, "check size");
        TEST_RESULT_INT(info.mode, 0640, "check mode");
        TEST_RESULT_INT(info.timeModified, 1555155555, "check mod time");
        TEST_RESULT_STR(info.linkDestination, NULL, "no link destination");
#ifdef TEST_CONTAINER_REQUIRED
        TEST_RESULT_STR(info.user, NULL, "check user");
        TEST_RESULT_STR(info.group, NULL, "check group");
#endif // TEST_CONTAINER_REQUIRED

        storageRemoveP(storageTest, fileName, .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info - link");

        const String *linkName = STRDEF(TEST_PATH "/testlink");
        HRN_SYSTEM_FMT("ln -s /tmp %s", strZ(linkName));

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

        storageRemoveP(storageTest, linkName, .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info - pipe");

        const String *pipeName = STRDEF(TEST_PATH "/testpipe");
        HRN_SYSTEM_FMT("mkfifo -m 666 %s", strZ(pipeName));

        TEST_ASSIGN(info, storageInfoP(storageTest, pipeName), "get info from pipe (special file)");
        TEST_RESULT_STR(info.name, NULL, "name is not set");
        TEST_RESULT_BOOL(info.exists, true, "check exists");
        TEST_RESULT_INT(info.type, storageTypeSpecial, "check type");
        TEST_RESULT_UINT(info.size, 0, "check size");
        TEST_RESULT_INT(info.mode, 0666, "check mode");
        TEST_RESULT_STR(info.linkDestination, NULL, "check link destination");
        TEST_RESULT_STR(info.user, TEST_USER_STR, "check user");
        TEST_RESULT_STR(info.group, TEST_GROUP_STR, "check group");

        storageRemoveP(storageTest, pipeName, .errorOnMissing = true);
    }

    // *****************************************************************************************************************************
    if (testBegin("storageInfoList()"))
    {
#ifdef TEST_CONTAINER_REQUIRED
        TEST_CREATE_NOPERM();
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path missing");

        TEST_ERROR_FMT(
            storageInfoListP(storageTest, STRDEF(BOGUS_STR), (StorageInfoListCallback)1, NULL, .errorOnMissing = true),
            PathMissingError, STORAGE_ERROR_LIST_INFO_MISSING, TEST_PATH "/BOGUS");

        TEST_RESULT_BOOL(
            storageInfoListP(storageTest, STRDEF(BOGUS_STR), (StorageInfoListCallback)1, NULL), false, "ignore missing dir");

#ifdef TEST_CONTAINER_REQUIRED
        TEST_ERROR_FMT(
            storageInfoListP(storageTest, pathNoPerm, (StorageInfoListCallback)1, NULL), PathOpenError, STORAGE_ERROR_LIST_INFO,
            strZ(pathNoPerm));

        // Should still error even when ignore missing
        TEST_ERROR_FMT(
            storageInfoListP(storageTest, pathNoPerm, (StorageInfoListCallback)1, NULL), PathOpenError, STORAGE_ERROR_LIST_INFO,
            strZ(pathNoPerm));
//        TEST_REMOVE_NOPERM();
#endif // TEST_CONTAINER_REQUIRED
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("helper function - storageSftpInfoListEntry()");

        HarnessStorageInfoListCallbackData callbackData =
        {
            .content = strNew(),
        };

        TEST_RESULT_VOID(
            storageSftpInfoListEntry(
                (StorageSftp *)storageDriver(storageTest), STRDEF("pg"), STRDEF("missing"), storageInfoLevelBasic,
                hrnStorageInfoListCallback, &callbackData),
            "missing path");
        TEST_RESULT_STR_Z(callbackData.content, "", "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path with only dot");

        // jrt !!!
        // NOTE: in my tests with --vm=none the resultant file would have the incorrect permissions. The request was coming through
        // properly but the umask resulted in altered permissions.  Do we want to alter the test result check, or the sshd_config
        // file as noted below?
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
        storagePathCreateP(storageTest, STRDEF("pg"), .mode = 0766);

        callbackData.content = strNew();

        TEST_RESULT_VOID(
            storageInfoListP(storageTest, STRDEF("pg"), hrnStorageInfoListCallback, &callbackData),
            "directory with one dot file sorted");
        TEST_RESULT_STR_Z(callbackData.content, ". {path, m=0764, u=" TEST_USER ", g=" TEST_GROUP "}\n", "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path with file, link, pipe");

#ifdef TEST_CONTAINER_REQUIRED
        storagePathCreateP(storageTest, STRDEF("pg/.include"), .mode = 0755);
        HRN_SYSTEM("sudo chown 77777:77777 " TEST_PATH "/pg/.include");
#endif // TEST_CONTAINER_REQUIRED

        storagePutP(storageNewWriteP(storageTest, STRDEF("pg/file"), .modeFile = 0660), BUFSTRDEF("TESTDATA"));

        HRN_SYSTEM("ln -s ../file " TEST_PATH "/pg/link");
        HRN_SYSTEM("mkfifo -m 777 " TEST_PATH "/pg/pipe");

        callbackData = (HarnessStorageInfoListCallbackData)
        {
            .content = strNew(),
            .timestampOmit = true,
            .modeOmit = true,
            .modePath = 0766,
            .modeFile = 0600,
            .userOmit = true,
            .groupOmit = true,
        };

        TEST_RESULT_VOID(
            storageInfoListP(storageTest, STRDEF("pg"), hrnStorageInfoListCallback, &callbackData, .sortOrder = sortOrderAsc),
            "directory with one dot file sorted");
        TEST_RESULT_STR_Z(
            callbackData.content,
            ". {path, m=0764}\n"
#ifdef TEST_CONTAINER_REQUIRED
            ".include {path, m=0755, u=77777, g=77777}\n"
#endif // TEST_CONTAINER_REQUIRED
            "file {file, s=8, m=0660}\n"
            "link {link, d=../file}\n"
            "pipe {special}\n",
            "check content");

#ifdef TEST_CONTAINER_REQUIRED
        HRN_SYSTEM("sudo rmdir " TEST_PATH "/pg/.include");
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path - recurse");

        storagePathCreateP(storageTest, STRDEF("pg/path"), .mode = 0700);
        storagePutP(storageNewWriteP(storageTest, STRDEF("pg/path/file"), .modeFile = 0600), BUFSTRDEF("TESTDATA"));

        callbackData.content = strNew();

        TEST_RESULT_VOID(
            storageInfoListP(
                storageTest, STRDEF("pg"), hrnStorageInfoListCallback, &callbackData, .sortOrder = sortOrderDesc, .recurse = true),
            "recurse descending");
        TEST_RESULT_STR_Z(
            callbackData.content,
            "pipe {special}\n"
            "path/file {file, s=8}\n"
            "path {path, m=0700}\n"
            "link {link, d=../file}\n"
            "file {file, s=8, m=0660}\n"
            ". {path, m=0764}\n",
            "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path basic info - recurse");

        // jrt !!! sftp will not overwrite file -- do we need to add additional param to force removal and write in these cases
        storageRemoveP(storageTest, STRDEF("pg/path/file"), .errorOnMissing = true);

        storagePathCreateP(storageTest, STRDEF("pg/path"), .mode = 0700);
        storagePutP(storageNewWriteP(storageTest, STRDEF("pg/path/file"), .modeFile = 0600), BUFSTRDEF("TESTDATA"));

        callbackData.content = strNew();

        storageTest->pub.interface.feature ^= 1 << storageFeatureInfoDetail;

        TEST_RESULT_VOID(
            storageInfoListP(
                storageTest, STRDEF("pg"), hrnStorageInfoListCallback, &callbackData, .sortOrder = sortOrderDesc, .recurse = true),
            "recurse descending");
        TEST_RESULT_STR_Z(
            callbackData.content,
            "pipe {special}\n"
            "path/file {file, s=8}\n"
            "path {path}\n"
            "link {link}\n"
            "file {file, s=8}\n"
            ". {path}\n",
            "check content");

        storageTest->pub.interface.feature ^= 1 << storageFeatureInfoDetail;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path - filter");

        callbackData.content = strNew();

        TEST_RESULT_VOID(
            storageInfoListP(
                storageTest, STRDEF("pg"), hrnStorageInfoListCallback, &callbackData, .sortOrder = sortOrderAsc,
                .expression = STRDEF("^path")),
            "filter");
        TEST_RESULT_STR_Z(
            callbackData.content,
            "path {path, m=0700}\n",
            "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("filter in subpath during recursion");

        callbackData.content = strNew();

        TEST_RESULT_VOID(
            storageInfoListP(
                storageTest, STRDEF("pg"), hrnStorageInfoListCallback, &callbackData, .sortOrder = sortOrderAsc, .recurse = true,
                .expression = STRDEF("\\/file$")),
            "filter");
        TEST_RESULT_STR_Z(callbackData.content, "path/file {file, s=8}\n", "check content");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageList()"))
    {
#ifdef TEST_CONTAINER_REQUIRED
        TEST_CREATE_NOPERM();
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path missing");

        TEST_ERROR_FMT(
            storageListP(storageTest, STRDEF(BOGUS_STR), .errorOnMissing = true), PathMissingError, STORAGE_ERROR_LIST_INFO_MISSING,
            TEST_PATH "/BOGUS");

        TEST_RESULT_PTR(storageListP(storageTest, STRDEF(BOGUS_STR), .nullOnMissing = true), NULL, "null for missing dir");
        TEST_RESULT_UINT(strLstSize(storageListP(storageTest, STRDEF(BOGUS_STR))), 0, "empty list for missing dir");

#ifdef TEST_CONTAINER_REQUIRED
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on missing, regardless of errorOnMissing setting");

        TEST_ERROR_FMT(
            storageListP(storageTest, pathNoPerm, .errorOnMissing = true), PathOpenError, STORAGE_ERROR_LIST_INFO,
            strZ(pathNoPerm));

        // Should still error even when ignore missing
        TEST_ERROR_FMT(storageListP(storageTest, pathNoPerm), PathOpenError, STORAGE_ERROR_LIST_INFO, strZ(pathNoPerm));
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("list - path with files");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, STRDEF(".aaa.txt")), BUFSTRDEF("aaa")), "write aaa.text");
        TEST_RESULT_STRLST_Z(strLstSort(storageListP(storageTest, NULL), sortOrderAsc), ".aaa.txt\n"
#ifdef TEST_CONTAINER_REQUIRED
            "noperm\n"
#endif // TEST_CONTAINER_REQUIRED
            , "dir list");

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, STRDEF("bbb.txt")), BUFSTRDEF("bbb")), "write bbb.text");
        TEST_RESULT_STRLST_Z(storageListP(storageTest, NULL, .expression = STRDEF("^bbb")), "bbb.txt\n", "dir list");

#ifdef TEST_CONTAINER_REQUIRED
//        TEST_REMOVE_NOPERM();
#endif // TEST_CONTAINER_REQUIRED
    }

/*  Optional -- can look at adding later
    // *****************************************************************************************************************************
    if (testBegin("storageCopy()"))
    {
        const String *sourceFile = STRDEF(TEST_PATH "/source.txt");
        const String *const destinationFile = STRDEF(TEST_PATH "/destination.txt");

        StorageRead *source = storageNewReadP(storageTest, sourceFile);
        StorageWrite *destination = storageNewWriteP(storageTest, destinationFile);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("copy - missing source");

        TEST_ERROR_FMT(storageCopyP(source, destination), FileMissingError, STORAGE_ERROR_READ_MISSING, strZ(sourceFile));
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("copy - ignore missing source");

        source = storageNewReadP(storageTest, sourceFile, .ignoreMissing = true);

        TEST_RESULT_BOOL(storageCopyP(source, destination), false, "copy and ignore missing file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("copy - success");

        const Buffer *expectedBuffer = BUFSTRDEF("TESTFILE\n");
        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, sourceFile), expectedBuffer), "write source file");

        source = storageNewReadP(storageTest, sourceFile);

        TEST_RESULT_BOOL(storageCopyP(source, destination), true, "copy file");
        TEST_RESULT_BOOL(bufEq(expectedBuffer, storageGetP(storageNewReadP(storageTest, destinationFile))), true, "check file");

        storageRemoveP(storageTest, sourceFile, .errorOnMissing = true);
        storageRemoveP(storageTest, destinationFile, .errorOnMissing = true);
    }

    // *****************************************************************************************************************************
    if (testBegin("storageMove()"))
    {
#ifdef TEST_CONTAINER_REQUIRED
        TEST_CREATE_NOPERM();
#endif // TEST_CONTAINER_REQUIRED

        const String *sourceFile = STRDEF(TEST_PATH "/source.txt");
        const String *destinationFile = STRDEF(TEST_PATH "/sub/destination.txt");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("move - missing source");

        StorageRead *source = storageNewReadP(storageTest, sourceFile);
        StorageWrite *destination = storageNewWriteP(storageTest, destinationFile);

        TEST_ERROR_FMT(
            storageMoveP(storageTest, source, destination), FileMissingError,
            "unable to move missing source '%s': [2] No such file or directory", strZ(sourceFile));
#ifdef TEST_CONTAINER_REQUIRED
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("move - source file permission denied");

        source = storageNewReadP(storageTest, fileNoPerm);

        TEST_ERROR_FMT(
            storageMoveP(storageTest, source, destination), FileMoveError,
            "unable to move '%s' to '%s': [13] Permission denied", strZ(fileNoPerm), strZ(destinationFile));
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("move - missing destination");

        const Buffer *buffer = BUFSTRDEF("TESTFILE");
        storagePutP(storageNewWriteP(storageTest, sourceFile), buffer);

        source = storageNewReadP(storageTest, sourceFile);
        destination = storageNewWriteP(storageTest, destinationFile, .noCreatePath = true);

        TEST_ERROR_FMT(
            storageMoveP(storageTest, source, destination), PathMissingError,
            "unable to move '%s' to missing path '%s': [2] No such file or directory", strZ(sourceFile),
            strZ(strPath(destinationFile)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("move - success");

        destination = storageNewWriteP(storageTest, destinationFile);

        TEST_RESULT_VOID(storageMoveP(storageTest, source, destination), "move file to subpath");
        TEST_RESULT_BOOL(storageExistsP(storageTest, sourceFile), false, "check source file not exists");
        TEST_RESULT_BOOL(storageExistsP(storageTest, destinationFile), true, "check destination file exists");
        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storageTest, destinationFile))), "TESTFILE", "check destination contents");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("move - same path");

        sourceFile = destinationFile;
        source = storageNewReadP(storageTest, sourceFile);
        destinationFile = STRDEF(TEST_PATH "/sub/destination2.txt");
        destination = storageNewWriteP(storageTest, destinationFile);

        TEST_RESULT_VOID(storageMoveP(storageTest, source, destination), "move file to same path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("move - parent path");

        sourceFile = destinationFile;
        source = storageNewReadP(storageTest, sourceFile);
        destinationFile = STRDEF(TEST_PATH "/source.txt");
        destination = storageNewWriteP(storageTest, destinationFile, .noSyncPath = true);

        TEST_RESULT_VOID(storageMoveP(storageTest, source, destination), "move file to parent path (no sync)");

    }
*/

    // *****************************************************************************************************************************
    if (testBegin("storagePath()"))
    {
        Storage *storageTest = NULL;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path - root path");

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                FSLASH_STR, STRDEF("localhost"), 22, 5000, 5000, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/authorized_keys")),
            "new storage /");
        TEST_RESULT_STR_Z(storagePathP(storageTest, NULL), "/", "root dir");
        TEST_RESULT_STR_Z(storagePathP(storageTest, STRDEF("/")), "/", "same as root dir");
        TEST_RESULT_STR_Z(storagePathP(storageTest, STRDEF("subdir")), "/subdir", "simple subdir");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path - expressions");

        TEST_ERROR(
            storagePathP(storageTest, STRDEF("<TEST>")), AssertError, "expression '<TEST>' not valid without callback function");

        TEST_ASSIGN(
            storageTest,
            storageSftpNewP(
                STRDEF("/path/to"), STRDEF("localhost"), 22, 5000, 5000, .user = TEST_USER_STR,
                .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/authorized_keys"),
                .pathExpressionFunction = storageTestPathExpression),
            "new storage /");
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
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathCreate()"))
    {
        TEST_RESULT_VOID(storagePathCreateP(storageTest, STRDEF("sub1")), "create sub1");
        TEST_RESULT_INT(storageInfoP(storageTest, STRDEF("sub1")).mode, 0750, "check sub1 dir mode");
        TEST_RESULT_VOID(storagePathCreateP(storageTest, STRDEF("sub1")), "create sub1 again");
        TEST_ERROR(
            storagePathCreateP(storageTest, STRDEF("sub1"), .errorOnExists = true), PathCreateError,
            "unable to create path '" TEST_PATH "/sub1': path already exists");

        // jrt !!!
        // NOTE: in my tests with --vm=none the resultant file would have the incorrect permissions. The request was coming through
        // properly but the umask resulted in altered permissions.  Do we want to alter the test result check, or the sshd_config
        // file as noted below?
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

        // EAGAIN fail
        StorageSftp *storageSftp = NULL;
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "assign storage");
        storageSftp->timeoutConnect = 1;
        storageSftp->timeoutSession = 1;
        TEST_ERROR(
            storagePathCreateP(storageTest, STRDEF("subfail")), PathCreateError, "unable to create path '" TEST_PATH "/subfail'");

        HRN_SYSTEM("rm -rf " TEST_PATH "/sub*");

        storageSftp->timeoutConnect = 10000;
        storageSftp->timeoutSession = 10000;
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePathRemove()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - missing");

        const String *pathRemove1 = STRDEF(TEST_PATH "/remove1");

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .errorOnMissing = true), PathRemoveError,
            STORAGE_ERROR_PATH_REMOVE_MISSING, strZ(pathRemove1));
        TEST_RESULT_VOID(storagePathRemoveP(storageTest, pathRemove1, .recurse = true), "ignore missing path");

        // -------------------------------------------------------------------------------------------------------------------------
        String *pathRemove2 = strNewFmt("%s/remove2", strZ(pathRemove1));

#ifdef TEST_CONTAINER_REQUIRED
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - parent/subpath permission denied");

        HRN_SYSTEM_FMT("sudo mkdir -p -m 700 %s", strZ(pathRemove2));

        TEST_ERROR_FMT(storagePathRemoveP(storageTest, pathRemove2), PathRemoveError, STORAGE_ERROR_PATH_REMOVE, strZ(pathRemove2));
        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove2, .recurse = true), PathOpenError, STORAGE_ERROR_LIST_INFO,
            strZ(pathRemove2));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - subpath permission denied");

        HRN_SYSTEM_FMT("sudo chmod 777 %s", strZ(pathRemove1));

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove2, .recurse = true), PathOpenError, STORAGE_ERROR_LIST_INFO,
            strZ(pathRemove2));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - file in subpath, permission denied");

        String *fileRemove = strNewFmt("%s/remove.txt", strZ(pathRemove2));

        HRN_SYSTEM_FMT(
            "sudo chmod 755 %s && sudo touch %s && sudo chmod 777 %s", strZ(pathRemove2), strZ(fileRemove), strZ(fileRemove));

        TEST_ERROR_FMT(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), PathRemoveError, STORAGE_ERROR_PATH_REMOVE_FILE,
            strZ(fileRemove));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - path with subpath and file removed");

        HRN_SYSTEM_FMT("sudo chmod 777 %s", strZ(pathRemove2));

        TEST_RESULT_VOID(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), "remove path");
        TEST_RESULT_BOOL(
            storageExistsP(storageTest, pathRemove1), false, "path is removed");
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path remove - path with subpath removed");

        HRN_SYSTEM_FMT("mkdir -p %s", strZ(pathRemove2));

        TEST_RESULT_VOID(
            storagePathRemoveP(storageTest, pathRemove1, .recurse = true), "remove path");
        TEST_RESULT_BOOL(
            storageExistsP(storageTest, pathRemove1), false, "path is removed");

/*
#ifdef TEST_CONTAINER_REQUIRED
        HRN_SYSTEM_FMT("mkdir -p %s", strZ(pathRemove2));
        HRN_SYSTEM_FMT("touch %s", strZ(strNewFmt("%s%s", strZ(pathRemove2), "/afile.txt")));
        HRN_SYSTEM_FMT("touch %s", strZ(strNewFmt("%s%s", strZ(pathRemove2), "/afile1.txt")));
        HRN_SYSTEM_FMT("touch %s", strZ(strNewFmt("%s%s", strZ(pathRemove2), "/afile2.txt")));
        HRN_SYSTEM_FMT("touch %s", strZ(strNewFmt("%s%s", strZ(pathRemove2), "/afile3.txt")));
        HRN_SYSTEM_FMT("sudo chmod 600  %s", strZ(strNewFmt("%s%s", strZ(pathRemove2), "/afile3.txt")));

        StorageSftp *storageSftp = NULL;
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "assign storage");

        StorageSftpPathRemoveData data =
        {
            .driver = storageTest->pub.driver,
            .path = strNewFmt("%s%s", strZ(pathRemove2), "/afile3.txt"),
            .session = storageSftp->session,
            .sftpSession = storageSftp->sftpSession,
            .timeoutConnect = storageSftp->timeoutConnect,
            .wait = storageSftp->wait,
        };


        //storageSftp->timeoutConnect = 1;
        //storageSftp->timeoutSession = 1;
        //TEST_ERROR_FMT(
        //    storagePathRemoveP(storageTest, pathRemove1, .recurse = true), PathRemoveError, STORAGE_ERROR_PATH_REMOVE,
        //    strZ(pathRemove1));
        //StorageInfo *info;
        TEST_ERROR_FMT(
        storageInterfaceInfoListP(storageTest, strNewFmt("%s%s", strZ(pathRemove2), "/afile3.txt"), storageInfoLevelExists, storageSftpPathRemoveCallback, &data), PathRemoveError, "jrt");

        HRN_SYSTEM_FMT("sudo rm %s", strZ(strNewFmt("%s%s", strZ(pathRemove2), "/afile3.txt")));

        storageSftp->timeoutConnect = 10000;
        storageSftp->timeoutSession = 10000;
#endif // TEST_CONTAINER_REQUIRED
        */
    }

    // ****************************************************************************************************************************
    if (testBegin("storageNewRead()"))
    {
        StorageRead *file = NULL;
        const String *fileName = STRDEF(TEST_PATH "/readtest.txt");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read missing");

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_ERROR_FMT(ioReadOpen(storageReadIo(file)), FileMissingError, STORAGE_ERROR_READ_MISSING, strZ(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read success");

        HRN_SYSTEM_FMT("touch %s", strZ(fileName));

        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_RESULT_INT(ioReadFd(storageReadIo(file)), -1, "check read fd");
        TEST_RESULT_VOID(ioReadClose(storageReadIo(file)), "close file");
    }

    // ****************************************************************************************************************************
    if (testBegin("storageReadClose()"))
    {
        Storage *storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 10000, 10000, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/authorized_keys"), .write = true);

        StorageRead *file = NULL;
        const String *fileName = STRDEF(TEST_PATH "/readtest.txt");
        HRN_SYSTEM_FMT("touch %s", strZ(fileName));
        Buffer *outBuffer = bufNew(2);

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_RESULT_VOID(storageReadSftpClose((StorageReadSftp *)file->driver), "close file");

        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 10000, 10000, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/authorized_keys"), .write = true);

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        close(ioSessionFd(((StorageReadSftp *)file->driver)->ioSession));
        /*
        TEST_ERROR(
            storageReadSftpClose((StorageReadSftp *)file->driver), FileCloseError,
            "unable to close file '" TEST_PATH "/readtest.txt' after read: [9] Bad file descriptor");
            */

        TEST_ERROR(
            storageReadSftpClose((StorageReadSftp *)file->driver), FileCloseError,
            "unable to close file '" TEST_PATH "/readtest.txt' after read: libssh2 errno [-7] Unable to send FXP_CLOSE command");
        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 10000, 10000, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/authorized_keys"), .write = true);

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        ((StorageReadSftp *)file->driver)->timeoutConnect = 0;
        ((StorageReadSftp *)file->driver)->timeoutSession = 0;
        TEST_ERROR(
            storageReadSftp(
                ((StorageReadSftp *)file->driver), outBuffer, false), FileReadError, "unable to read '" TEST_PATH "/readtest.txt'");
/*
        storageTest = storageSftpNewP(
            TEST_PATH_STR, STRDEF("localhost"), 22, 10000, 10000, .user = TEST_USER_STR,
            .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"), .keyPub = STRDEF("/home/vagrant/.ssh/authorized_keys"), .write = true);

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        ((StorageReadSftp *)file->driver)->timeoutConnect = 0;
        ((StorageReadSftp *)file->driver)->timeoutSession = 0;
        libssh2_session_set_last_error(((StorageReadSftp *)file->driver)->session, -2, "jrt");
        TEST_ERROR(
            storageReadSftpClose((StorageReadSftp *)file->driver), FileCloseError,
            "unable to close file '%s' after read: [9] Bad file descriptor");
            */

//        TEST_RESULT_VOID(storageReadSftpClose((StorageReadSftp *)file->driver), "close file");
//        TEST_RESULT_UINT(storageReadSftp(((StorageReadSftp *)file->driver), outBuffer, false), 0, "read file");


//        TEST_RESULT_PTR(storageInterface(storageTest).info, storageTest->pub.interface.info, "check interface");

//        libssh2_session_set_last_error(((StorageReadSftp *)file->driver)->session, -2, "jrt");
//        TEST_RESULT_INT(libssh2_session_last_errno(((StorageReadSftp *)file->driver)->session), -2, "check ssh2 errno");
//
//        TEST_RESULT_VOID(storageReadSftpClose((StorageReadSftp *)file->driver), "close file");
//        TEST_RESULT_PTR(((StorageReadSftp *)file->driver)->sftpHandle, storageTest->pub.driver.sftpHandle, "null handle?");
//        ((StorageReadSftp *)file->driver)->wait = waitNew(0);
//                (StorageSftp *)storageDriver(storageTest)
//        TEST_RESULT_PTR(storageDriver(storageTest), storageTest->pub.driver, "check driver");
//        TEST_RESULT_UINT(storageType(storageTest), storageTest->pub.type, "check type");

//        StorageRead *file = NULL;
//        const String *fileName = STRDEF(TEST_PATH "/readtest.txt");
//        HRN_SYSTEM_FMT("touch %s", strZ(fileName));


//        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
//        TEST_RESULT_PTR(((StorageReadSftp *)file->driver)->sftpHandle, NULL, "null handle?");


/*
        ((StorageReadSftp *)file->driver)->wait = 0;
        StorageReadSftp *storageReadSftp = (StorageReadSftp *)file->driver;
        TEST_RESULT_VOID(storageReadSftpClose(storageReadSftp), "close file");
        */
/*
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read missing");

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file (defaults)");
        TEST_ERROR_FMT(ioReadOpen(storageReadIo(file)), FileMissingError, STORAGE_ERROR_READ_MISSING, strZ(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read success");

        HRN_SYSTEM_FMT("touch %s", strZ(fileName));

        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");
        TEST_RESULT_INT(ioReadFd(storageReadIo(file)), -1, "check read fd");
        TEST_RESULT_VOID(ioReadClose(storageReadIo(file)), "close file");
        */
    }

    // *****************************************************************************************************************************
    if (testBegin("storageNewWrite()"))
    {
        const String *fileName = STRDEF(TEST_PATH "/sub1/testfile");
        StorageWrite *file = NULL;

#ifdef TEST_CONTAINER_REQUIRED
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("permission denied");

        TEST_CREATE_NOPERM();

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileNoPerm, .noAtomic = true), "new write file (defaults)");
        TEST_ERROR_FMT(ioWriteOpen(storageWriteIo(file)), FileOpenError, STORAGE_ERROR_WRITE_OPEN, strZ(fileNoPerm));
//        TEST_REMOVE_NOPERM();
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write file - defaults");

        TEST_ASSIGN(
            file,
            storageNewWriteP(storageTest, fileName, .user = TEST_USER_STR, .group = TEST_GROUP_STR, .timeModified = 1),
            "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_INT(ioWriteFd(storageWriteIo(file)), -1, "check write fd");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");
        TEST_RESULT_INT(storageInfoP(storageTest, strPath(fileName)).mode, 0750, "check path mode");
        TEST_RESULT_INT(storageInfoP(storageTest, fileName).mode, 0640, "check file mode");
        TEST_RESULT_INT(storageInfoP(storageTest, fileName).timeModified, 1, "check file modified times");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("test that a premature free (from error or otherwise) does not rename the file");

        fileName = STRDEF(TEST_PATH "/sub1/testfile-abort");
        String *fileNameTmp = strNewFmt("%s." STORAGE_FILE_TEMP_EXT, strZ(fileName));

        TEST_ASSIGN(
            file, storageNewWriteP(storageTest, fileName, .user = TEST_USER_STR), "new write file (defaults)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_VOID(ioWrite(storageWriteIo(file), BUFSTRDEF("TESTDATA")), "write data");
        TEST_RESULT_VOID(ioWriteFlush(storageWriteIo(file)), "flush data");
        TEST_RESULT_VOID(ioWriteFree(storageWriteIo(file)), "free file");

        TEST_RESULT_BOOL(storageExistsP(storageTest, fileName), false, "destination file does not exist");
        TEST_RESULT_BOOL(storageExistsP(storageTest, fileNameTmp), true, "destination tmp file exists");
        TEST_RESULT_UINT(storageInfoP(storageTest, fileNameTmp).size, 8, "check temp file size");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write file - set mode");

        fileName = STRDEF(TEST_PATH "/sub2/testfile");

        TEST_ASSIGN(
            file, storageNewWriteP(storageTest, fileName, .modePath = 0700, .modeFile = 0600, .group = TEST_GROUP_STR),
            "new write file (set mode)");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");
        TEST_RESULT_VOID(ioWriteClose(storageWriteIo(file)), "close file");
        TEST_RESULT_INT(storageInfoP(storageTest, strPath(fileName)).mode, 0700, "check path mode");
        TEST_RESULT_INT(storageInfoP(storageTest, fileName).mode, 0600, "check file mode");
    }

    // *****************************************************************************************************************************
    if (testBegin("storagePut() and storageGet()"))
    {
        Storage *storageTest = storageSftpNewP(
            FSLASH_STR, STRDEF("localhost"), 22, 5000, 5000, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/authorized_keys"), .write = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get error - attempt to get directory");

        TEST_ERROR(
            storageGetP(storageNewReadP(storageTest, TEST_PATH_STR)), FileReadError,
            "unable to read '" TEST_PATH "': sftp errno [4]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - empty file");

        const String *emptyFile = STRDEF(TEST_PATH "/test.empty");
        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, emptyFile), NULL), "put empty file");
        TEST_RESULT_BOOL(storageExistsP(storageTest, emptyFile), true, "check empty file exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("put - file with contents");

        const Buffer *buffer = BUFSTRDEF("TESTFILE\n");

        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, STRDEF(TEST_PATH "/test.txt")), buffer), "put test file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - ignore missing");

        TEST_RESULT_PTR(
            storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/" BOGUS_STR), .ignoreMissing = true)), NULL,
            "get missing file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - empty file");

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.empty"))), "get empty");
        TEST_RESULT_UINT(bufSize(buffer), 0, "size is 0");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - file with contents");

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"))), "get text");
        TEST_RESULT_UINT(bufSize(buffer), 9, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "TESTFILE\n", bufSize(buffer)) == 0, true, "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - exact size smaller");

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt")), .exactSize = 4), "get exact");
        TEST_RESULT_UINT(bufSize(buffer), 4, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "TEST", bufSize(buffer)) == 0, true, "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - exact size larger");

        TEST_ERROR(
            storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt")), .exactSize = 64), FileReadError,
            "unable to read 64 byte(s) from '" TEST_PATH "/test.txt'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get - smaller buffer size");

        ioBufferSizeSet(2);

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"))), "get text");
        TEST_RESULT_UINT(bufSize(buffer), 9, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "TESTFILE\n", bufSize(buffer)) == 0, true, "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid read offset bytes");

        TEST_ERROR(
            storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"), .offset = UINT64_MAX)), FileOpenError,
            "unable to seek to 18446744073709551615 in file '" TEST_PATH "/test.txt'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read limited bytes");

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"), .limit = VARUINT64(7))), "get");
        TEST_RESULT_UINT(bufSize(buffer), 7, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "TESTFIL", bufSize(buffer)) == 0, true, "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read offset bytes");

        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"), .offset = 4)), "get");
        TEST_RESULT_UINT(bufSize(buffer), 5, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "FILE\n", bufSize(buffer)) == 0, true, "check content");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read offset/limited bytes");

        TEST_ASSIGN(
            buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test.txt"), .offset = 4,
            .limit = VARUINT64(4))), "get");
        TEST_RESULT_UINT(bufSize(buffer), 4, "check size");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(buffer), "FILE", bufSize(buffer)) == 0, true, "check content");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageRemove()"))
    {
#ifdef TEST_CONTAINER_REQUIRED
        TEST_CREATE_NOPERM();
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove - file missing");

        TEST_RESULT_VOID(storageRemoveP(storageTest, STRDEF("missing")), "remove missing file");
        TEST_ERROR(
            storageRemoveP(storageTest, STRDEF("missing"), .errorOnMissing = true), FileRemoveError,
            "unable to remove '" TEST_PATH "/missing'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove - file exists");

        const String *fileExists = STRDEF(TEST_PATH "/exists");
        HRN_SYSTEM_FMT("touch %s", strZ(fileExists));

        TEST_RESULT_VOID(storageRemoveP(storageTest, fileExists), "remove exists file");

#ifdef TEST_CONTAINER_REQUIRED
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remove - permission denied");

        TEST_ERROR_FMT(storageRemoveP(storageTest, fileNoPerm), FileRemoveError, "unable to remove '%s'", strZ(fileNoPerm));

        // EAGAIN fail
        StorageSftp *storageSftp = NULL;
        TEST_ASSIGN(storageSftp, storageDriver(storageTest), "assign storage");
        storageSftp->timeoutConnect = 1;
        storageSftp->timeoutSession = 1;

        TEST_ERROR_FMT(
            storageRemoveP(storageTest, fileNoPerm, .errorOnMissing = true),
            FileRemoveError, "unable to remove '%s'", strZ(fileNoPerm));
        storageSftp->timeoutConnect = 10000;
        storageSftp->timeoutSession = 10000;

        TEST_REMOVE_NOPERM();
#endif // TEST_CONTAINER_REQUIRED
    }

    // *****************************************************************************************************************************
    if (testBegin("StorageRead"))
    {
       StorageRead *file = NULL;

#ifdef TEST_CONTAINER_REQUIRED
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("ignore missing - file with no permission to read");

        TEST_CREATE_NOPERM();

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileNoPerm, .ignoreMissing = true), "new read file");
        TEST_RESULT_BOOL(storageReadIgnoreMissing(file), true, "check ignore missing");
        TEST_RESULT_STR(storageReadName(file), fileNoPerm, "check name");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("permission denied");

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileNoPerm), "new no perm read file");
        TEST_ERROR_FMT(ioReadOpen(storageReadIo(file)), FileOpenError, STORAGE_ERROR_READ_OPEN, strZ(fileNoPerm));

        TEST_REMOVE_NOPERM();
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file missing");

        const String *fileName = STRDEF(TEST_PATH "/test.file");

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new missing read file");
        TEST_ERROR_FMT(ioReadOpen(storageReadIo(file)), FileMissingError, STORAGE_ERROR_READ_MISSING, strZ(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("ignore missing");

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName, .ignoreMissing = true), "new missing read file");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), false, "missing file ignored");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("bad sftp handle");

        Buffer *outBuffer = bufNew(2);
        const Buffer *expectedBuffer = BUFSTRDEF("TESTFILE\n");
        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, fileName), expectedBuffer), "write test file");

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName), "new read file");
        TEST_RESULT_BOOL(ioReadOpen(storageReadIo(file)), true, "open file");

        libssh2_sftp_close(((StorageReadSftp *)file->driver)->sftpHandle);

        TEST_ERROR_FMT(
            ioRead(storageReadIo(file), outBuffer), FileReadError, "unable to read '%s': sftp errno [4]", strZ(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("incremental load, storageReadSftp(), storageReadSftpEof()");

        // Recreate storageTest as previous test closed socket fd
        storageTest = storageSftpNewP(
            FSLASH_STR, STRDEF("localhost"), 22, 5000, 5000, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/authorized_keys"), .write = true);

        Buffer *buffer = bufNew(0);

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
    }

    // *****************************************************************************************************************************
    if (testBegin("StorageWrite"))
    {
        StorageWrite *file = NULL;

#ifdef TEST_CONTAINER_REQUIRED
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check getters");

        TEST_CREATE_NOPERM();

        TEST_ASSIGN(
            file,
            storageNewWriteP(
                storageTest, fileNoPerm, .modeFile = 0444, .modePath = 0555, .noCreatePath = true, .noSyncFile = true,
                .noSyncPath = true, .noAtomic = true),
            "new write file");

        TEST_RESULT_BOOL(storageWriteAtomic(file), false, "check atomic");
        TEST_RESULT_BOOL(storageWriteCreatePath(file), false, "check create path");
        TEST_RESULT_INT(storageWriteModeFile(file), 0444, "check mode file");
        TEST_RESULT_INT(storageWriteModePath(file), 0555, "check mode path");
        TEST_RESULT_STR(storageWriteName(file), fileNoPerm, "check name");
        TEST_RESULT_BOOL(storageWriteSyncPath(file), false, "check sync path");
        TEST_RESULT_BOOL(storageWriteSyncFile(file), false, "check sync file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("permission denied");

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileNoPerm, .noAtomic = true), "new write file");
        TEST_ERROR_FMT(ioWriteOpen(storageWriteIo(file)), FileOpenError, STORAGE_ERROR_WRITE_OPEN, strZ(fileNoPerm));
        TEST_REMOVE_NOPERM();
#endif // TEST_CONTAINER_REQUIRED

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file missing");

        const String *fileName = STRDEF(TEST_PATH "/sub1/test.file");

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName, .noCreatePath = true, .noAtomic = true), "new write file");
        TEST_ERROR_FMT(ioWriteOpen(storageWriteIo(file)), FileMissingError, STORAGE_ERROR_WRITE_MISSING, strZ(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("bad sftp handle - error in sync");
// jrt also think may needs section of tests where the filesystem file descriptor is bad vs the sftpHandle file descriptor - next test
// jrt look at re-implement/add flag to signify that handle is closed to avoid double *sftp_close* free error
        String *fileTmp = strNewFmt("%s.pgbackrest.tmp", strZ(fileName));
        ioBufferSizeSet(10);
        const Buffer *buffer = BUFSTRDEF("TESTFILE\n");

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file");
        TEST_RESULT_STR(storageWriteName(file), fileName, "check file name");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");

        // Close the sftp handle so operations will fail
        libssh2_sftp_close(((StorageWriteSftp *)file->driver)->sftpHandle);
        storageRemoveP(storageTest, fileTmp, .errorOnMissing = true);

        // jrt ??? can we gen a better errmsg
        TEST_ERROR_FMT(
            storageWriteSftp(file->driver, buffer), FileWriteError, "unable to write '%s.pgbackrest.tmp'",
            strZ(fileName));
        TEST_ERROR_FMT(
            storageWriteSftpClose(file->driver), FileSyncError, STORAGE_ERROR_WRITE_SYNC, strZ(fileTmp));
/*
        // Disable file sync so close() can be reached
        ((StorageWriteSftp *)file->driver)->interface.syncFile = false;
        TEST_ERROR_FMT(
            storageWriteSftpClose(file->driver), FileMoveError, "unable to move '%s' to '%s': [2] No such file or directory",
            strZ(fileTmp), strZ(fileName));
*/

        // Set sftpHandle to NULL so the close on free with not fail
        //((StorageWriteSftp *)file->driver)->sftpHandle = NULL;
        // Set file descriptor to -1 so the close on free with not fail
        //((StorageWritePosix *)file->driver)->fd = -1;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("bad socket - error in file sync, update time modified");

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file");
        TEST_RESULT_STR(storageWriteName(file), fileName, "check file name");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");

        storageRemoveP(storageTest, fileTmp, .errorOnMissing = true);

        // Close the socket file descriptor
        close(ioSessionFd(((StorageWriteSftp *)file->driver)->ioSession));

        TEST_ERROR_FMT(
            storageWriteSftp(file->driver, buffer), FileWriteError, "unable to write '%s.pgbackrest.tmp'",
            strZ(fileName));

        // Disable file sync so timeModified can be reached
        ((StorageWriteSftp *)file->driver)->interface.syncFile = false;
        TEST_ERROR_FMT(
            storageWriteSftpClose(file->driver),
            FileCloseError,
            STORAGE_ERROR_WRITE_CLOSE ": libssh2 error [-7] Unable to send FXP_CLOSE command", strZ(fileTmp));

        // Fail on setting time modified
        ((StorageWriteSftp *)file->driver)->interface.timeModified = 44444;
        TEST_ERROR_FMT(
            storageWriteSftpClose( file->driver),
            FileInfoError,
            "unable to set time for '%s': libssh2 error [-7] Unable to send FXP_FSETSTAT", strZ(fileTmp));


        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("bad socket - error in close");

        // Recreate storageTest as previous test closed socket fd
        storageTest = storageSftpNewP(
            FSLASH_STR, STRDEF("localhost"), 22, 5000, 5000, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/authorized_keys"), .write = true);


        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file");
        TEST_RESULT_STR(storageWriteName(file), fileName, "check file name");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");

        // Close the sftp handle so operations will fail
//        libssh2_sftp_close(((StorageWriteSftp *)file->driver)->sftpHandle);
//        close(((StorageWritePosix *)file->driver)->fd);
        storageRemoveP(storageTest, fileTmp, .errorOnMissing = true);

        // Close the socket file descriptor
        close(ioSessionFd(((StorageWriteSftp *)file->driver)->ioSession));

        TEST_ERROR_FMT(
            storageWriteSftp(file->driver, buffer), FileWriteError, "unable to write '%s.pgbackrest.tmp'",
            strZ(fileName));
        /*
        TEST_ERROR_FMT(
            storageWriteSftpClose(file->driver), FileSyncError, STORAGE_ERROR_WRITE_SYNC ": [2] No such file or directory",
            strZ(fileTmp));
            */

        // Disable file sync so close can be reached
        ((StorageWriteSftp *)file->driver)->interface.syncFile = false;
        TEST_ERROR_FMT(
            storageWriteSftpClose(file->driver),
            FileCloseError,
            STORAGE_ERROR_WRITE_CLOSE ": libssh2 error [-7] Unable to send FXP_CLOSE command", strZ(fileTmp));

        /*
        TEST_ERROR_FMT(
            storageWriteSftpClose(file->driver), FileMoveError, "unable to move '%s' to '%s': [2] No such file or directory",
            strZ(fileTmp), strZ(fileName));
            */

        // Set sftpHandle to NULL so the close on free with not fail
        //((StorageWriteSftp *)file->driver)->sftpHandle = NULL;
        // Set file descriptor to -1 so the close on free with not fail
        //((StorageWritePosix *)file->driver)->fd = -1;

//        const String *fileName = STRDEF(TEST_PATH "/sub1/test.file");
//        HRN_SYSTEM_FMT("rmdir %s", strZ(strPath(fileName)));
        //storagePathRemoveP(storageTest, STRDEF("sub1"), .errorOnMissing = true, .recurse = true);


        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file already closed");

        ((StorageWriteSftp *)file->driver)->sftpHandle = NULL;
        TEST_RESULT_VOID(storageWriteSftpClose(file->driver), "close file");

        // -------------------------------------------------------------------------------------------------------------------------

        TEST_TITLE("fail rename in close");

        // Recreate storageTest as previous test closed socket fd
        storageTest = storageSftpNewP(
            FSLASH_STR, STRDEF("localhost"), 22, 5000, 5000, .user = TEST_USER_STR, .keyPriv = STRDEF("/home/vagrant/.ssh/id_rsa"),
            .keyPub = STRDEF("/home/vagrant/.ssh/authorized_keys"), .write = true);

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName), "new write file");
        TEST_RESULT_STR(storageWriteName(file), fileName, "check file name");
        TEST_RESULT_UINT(storageWriteType(file), STORAGE_SFTP_TYPE, "check file type");
        TEST_RESULT_VOID(ioWriteOpen(storageWriteIo(file)), "open file");

        // Rename the file back to original name from tmp -- this will cause the rename in close to fail
        TEST_RESULT_INT(rename(strZ(fileTmp), strZ(fileName)), 0, "rename tmp file");
        TEST_ERROR_FMT(
            ioWriteClose(storageWriteIo(file)),
            FileMoveError,
            "unable to move '%s' to '%s': libssh2 error [-31] SFTP Protocol Error", strZ(fileTmp), strZ(fileName));

        // Set file descriptor to -1 so the close on free with not fail
        //((StorageWriteSftp *)file->driver)->fd = -1;
        // Set sftpHandle to NULL so the close on free with not fail
        //((StorageWriteSftp *)file->driver)->sftpHandle = NULL;

        storageRemoveP(storageTest, fileName, .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write file success");

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

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write subpath and file success");

        fileName = STRDEF(TEST_PATH "/sub2/test.file");

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

        storageRemoveP(storageTest, fileName, .errorOnMissing = true);
    }

    // *****************************************************************************************************************************
    if (testBegin("storageLocal() and storageLocalWrite()"))
    {
        const Storage *storage = NULL;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageLocal()");

        TEST_RESULT_PTR(storageHelper.storageLocal, NULL, "local storage not cached");
        TEST_ASSIGN(storage, storageLocal(), "new storage");
        TEST_RESULT_PTR(storageHelper.storageLocal, storage, "local storage cached");
        TEST_RESULT_PTR(storageLocal(), storage, "get cached storage");

        TEST_RESULT_STR_Z(storagePathP(storage, NULL), "/", "check base path");

        TEST_ERROR(storageNewWriteP(storage, writeFile), AssertError, "assertion 'this->write' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageLocalWrite()");

        TEST_RESULT_PTR(storageHelper.storageLocalWrite, NULL, "local storage not cached");
        TEST_ASSIGN(storage, storageLocalWrite(), "new storage");
        TEST_RESULT_PTR(storageHelper.storageLocalWrite, storage, "local storage cached");
        TEST_RESULT_PTR(storageLocalWrite(), storage, "get cached storage");

        TEST_RESULT_STR_Z(storagePathP(storage, NULL), "/", "check base path");

        TEST_RESULT_VOID(storageNewWriteP(storage, writeFile), "writes are allowed");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageRepo*()"))
    {
        // Load configuration to set repo-path and stanza
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpAccount, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyfile, "/home/vagrant/.ssh/id_rsa");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyfile, "/home/vagrant/.ssh/authorized_keys");
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
            storagePathP(storage, STRDEF(STORAGE_REPO_ARCHIVE "/9.4-1/700000007000000070000000")),
            TEST_PATH "/archive/db/9.4-1/7000000070000000/700000007000000070000000", "check segment path");
        TEST_RESULT_STR_Z(
            storagePathP(storage, STRDEF(STORAGE_REPO_ARCHIVE "/9.4-1/00000008.history")),
            TEST_PATH "/archive/db/9.4-1/00000008.history", "check history path");
        TEST_RESULT_STR_Z(
            storagePathP(storage, STRDEF(STORAGE_REPO_ARCHIVE "/9.4-1/000000010000014C0000001A.00000028.backup")),
            TEST_PATH "/archive/db/9.4-1/000000010000014C/000000010000014C0000001A.00000028.backup",
            "check archive backup path");
        TEST_RESULT_STR_Z(storagePathP(storage, STORAGE_REPO_BACKUP_STR), TEST_PATH "/backup/db", "check backup path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageRepo() - helper does not fail when stanza option not set");

        // Change the stanza to NULL with the stanzaInit flag still true, make sure helper does not fail when stanza option not set
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpAccount, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyfile, "/home/vagrant/.ssh/id_rsa");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyfile, "/home/vagrant/.ssh/authorized_keys");
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

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageRepoWrite() - confirm write enabled");

        TEST_RESULT_PTR(storageHelper.storageRepoWrite, NULL, "repo write storage not cached");
        TEST_ASSIGN(storage, storageRepoWrite(), "new write storage");
        TEST_RESULT_PTR(storageHelper.storageRepoWrite[0], storage, "repo write storage cached");
        TEST_RESULT_PTR(storageRepoWrite(), storage, "get cached storage");

        TEST_RESULT_BOOL(storage->write, true, "get write enabled");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageSpool(), storageSpoolWrite() and storagePg*()"))
    {
        const Storage *storage = NULL;

        // Load configuration to set spool-path and stanza
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoSftpAccount, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyfile, "/home/vagrant/.ssh/id_rsa");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyfile, "/home/vagrant/.ssh/authorized_keys");
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawBool(argList, cfgOptArchiveAsync, true);
        hrnCfgArgRawZ(argList, cfgOptSpoolPath, TEST_PATH);
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, TEST_PATH "/db");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 2, TEST_PATH "/db2");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSpool() - cached/not cached");

        TEST_RESULT_PTR(storageHelper.storageSpool, NULL, "storage not cached");
        TEST_ASSIGN(storage, storageSpool(), "new storage");
        TEST_RESULT_PTR(storageHelper.storageSpool, storage, "storage cached");
        TEST_RESULT_PTR(storageSpool(), storage, "get cached storage");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("STORAGE_SPOOL_ARCHIVE expression");

        TEST_RESULT_STR_Z(storagePathP(storage, STRDEF(STORAGE_SPOOL_ARCHIVE)), TEST_PATH "/archive/db", "check spool path");
        TEST_RESULT_STR_Z(
            storagePathP(storage, strNewFmt("%s/%s", STORAGE_SPOOL_ARCHIVE, "file.ext")), TEST_PATH "/archive/db/file.ext",
            "check spool file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSpool() - confirm settings");

        TEST_RESULT_STR_Z(storagePathP(storage, NULL), TEST_PATH, "check base path");
        TEST_RESULT_STR_Z(
            storagePathP(storage, STORAGE_SPOOL_ARCHIVE_OUT_STR), TEST_PATH "/archive/db/out", "check spool out path");
        TEST_RESULT_STR_Z(
            storagePathP(storage, STRDEF(STORAGE_SPOOL_ARCHIVE_OUT "/file.ext")), TEST_PATH "/archive/db/out/file.ext",
            "check spool out file");

        TEST_RESULT_STR_Z(storagePathP(storage, STORAGE_SPOOL_ARCHIVE_IN_STR), TEST_PATH "/archive/db/in", "check spool in path");
        TEST_RESULT_STR_Z(
            storagePathP(storage, STRDEF(STORAGE_SPOOL_ARCHIVE_IN "/file.ext")), TEST_PATH "/archive/db/in/file.ext",
            "check spool in file");

        TEST_ERROR(storagePathP(storage, STRDEF("<" BOGUS_STR ">")), AssertError, "invalid expression '<BOGUS>'");

        TEST_ERROR(storageNewWriteP(storage, writeFile), AssertError, "assertion 'this->write' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSpoolWrite() - confirm write enabled");

        TEST_RESULT_PTR(storageHelper.storageSpoolWrite, NULL, "storage not cached");
        TEST_ASSIGN(storage, storageSpoolWrite(), "new storage");
        TEST_RESULT_PTR(storageHelper.storageSpoolWrite, storage, "storage cached");
        TEST_RESULT_PTR(storageSpoolWrite(), storage, "get cached storage");

        TEST_RESULT_VOID(storageNewWriteP(storage, writeFile), "writes are allowed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storagePg() - confirm settings for read, write and index functions");

        TEST_RESULT_PTR(storageHelper.storagePg, NULL, "pg storage not cached");
        TEST_ASSIGN(storage, storagePg(), "new pg storage");
        TEST_RESULT_PTR(storageHelper.storagePg[0], storage, "pg storage cached");
        TEST_RESULT_PTR(storagePg(), storage, "get cached pg storage");

        TEST_RESULT_STR_Z(storage->path, TEST_PATH "/db", "check pg storage path");
        TEST_RESULT_BOOL(storage->write, false, "check pg storage write");
        TEST_RESULT_STR_Z(storagePgIdx(1)->path, TEST_PATH "/db2", "check pg 2 storage path");

        TEST_RESULT_PTR(storageHelper.storagePgWrite, NULL, "pg write storage not cached");
        TEST_ASSIGN(storage, storagePgWrite(), "new pg write storage");
        TEST_RESULT_PTR(storageHelper.storagePgWrite[0], storage, "pg write storage cached");
        TEST_RESULT_PTR(storagePgWrite(), storage, "get cached pg write storage");
        TEST_RESULT_STR_Z(storagePgIdxWrite(1)->path, TEST_PATH "/db2", "check pg 2 write storage path");

        TEST_RESULT_STR_Z(storage->path, TEST_PATH "/db", "check pg write storage path");
        TEST_RESULT_BOOL(storage->write, true, "check pg write storage write");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storageSpool - helper fails because stanza is required");

        // Change the stanza to NULL, stanzaInit flag to false and make sure helper fails because stanza is required
        storageHelper.storageSpool = NULL;
        storageHelper.storageSpoolWrite = NULL;
        storageHelper.stanzaInit = false;
        storageHelper.stanza = NULL;

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        HRN_CFG_LOAD(cfgCmdInfo, argList);

        TEST_ERROR(storageSpool(), AssertError, "stanza cannot be NULL for this storage object");
        TEST_ERROR(storageSpoolWrite(), AssertError, "stanza cannot be NULL for this storage object");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageRepoGet() and StorageDriverCifs"))
    {
        // Load configuration
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoType, "sftp");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpAccount, TEST_USER);
        hrnCfgArgRawZ(argList, cfgOptRepoSftpHost, "localhost");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPrivateKeyfile, "/home/vagrant/.ssh/id_rsa");
        hrnCfgArgRawZ(argList, cfgOptRepoSftpPublicKeyfile, "/home/vagrant/.ssh/authorized_keys");
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
    }

    FUNCTION_HARNESS_RETURN_VOID();
}