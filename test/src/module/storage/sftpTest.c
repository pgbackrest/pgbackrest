/***********************************************************************************************************************************
Test SFTP Storage
***********************************************************************************************************************************/
#include "common/io/io.h"
#include "common/io/socket/client.h"
#include "common/time.h"
#include "storage/read.h"
#include "storage/sftp/storage.h"
#include "storage/write.h"

#include "common/harnessConfig.h"
#include "common/harnessFork.h"
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("storageNew()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create new storage with defaults");

        Storage *storageTest = NULL;
        TEST_ASSIGN(storageTest, storageSftpNewP(STRDEF("/tmp"), STRDEF("localhost"), 22, 100, 100, .user = strNewZ("vagrant"),
                    .password = strNewZ("vagrant")), "new storage (defaults)");
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
                STRDEF("/path/to"), STRDEF("localhost"), 22, 100, 100, .user = strNewZ("vagrant"), .password = strNewZ("vagrant"),
                .modeFile = 0600, .modePath = 0700, .write = true), "new storage (non-default)");
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

    FUNCTION_HARNESS_RETURN_VOID();
}
