/***********************************************************************************************************************************
Test Info Handler
***********************************************************************************************************************************/
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "storage/posix/storage.h"

#include "common/harnessInfo.h"

/***********************************************************************************************************************************
Test load callbacks
***********************************************************************************************************************************/
typedef struct TestInfoLoad
{
    unsigned int errorTotal;
    // bool errorSame;
} TestInfoLoad;

static bool
testInfoLoadCallback(void *data, unsigned int try)
{
    TestInfoLoad *testInfoLoad = (TestInfoLoad *)data;

    if (testInfoLoad->errorTotal == 1 && try == 0)
        THROW(ChecksumError, "checksum error");

    return true;
}

/***********************************************************************************************************************************
Test save callbacks
***********************************************************************************************************************************/
static void
testInfoSaveCallback(void *data, const String *sectionNext, InfoSave *infoSaveData)
{
    if (infoSaveSection(infoSaveData, STRDEF("c"), sectionNext))
        infoSaveValue(infoSaveData, STRDEF("c"), strNew("key"), (String *)data);

    if (infoSaveSection(infoSaveData, STRDEF("d"), sectionNext))
        infoSaveValue(infoSaveData, STRDEF("d"), strNew("key"), (String *)data);
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    // Create default storage object for testing
    // Storage *storageTest = storagePosixNew(
    //     strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

    // *****************************************************************************************************************************
    if (testBegin("infoNew()"))
    {
        Info *info = NULL;

        TEST_ASSIGN(info, infoNew(cipherTypeAes256Cbc, strNew("123xyz")), "infoNew(cipher)");
        TEST_RESULT_STR(strPtr(infoCipherPass(info)), "123xyz", "    cipherPass is set");

        TEST_ASSIGN(info, infoNew(cipherTypeNone, NULL), "infoNew(NULL)");
        TEST_RESULT_PTR(infoCipherPass(info), NULL, "    cipherPass is NULL");

        TEST_ERROR(
            infoNew(cipherTypeNone, strNew("")), AssertError,
            "assertion '!((cipherType == cipherTypeNone && cipherPassSub != NULL) || (cipherType != cipherTypeNone && "
            "(cipherPassSub == NULL || strSize(cipherPassSub) == 0)))' failed");
        TEST_ERROR(
            infoNew(cipherTypeAes256Cbc, strNew("")), AssertError,
            "assertion '!((cipherType == cipherTypeNone && cipherPassSub != NULL) || (cipherType != cipherTypeNone && "
            "(cipherPassSub == NULL || strSize(cipherPassSub) == 0)))' failed");
        TEST_ERROR(
            infoNew(cipherTypeAes256Cbc, NULL), AssertError,
            "assertion '!((cipherType == cipherTypeNone && cipherPassSub != NULL) || (cipherType != cipherTypeNone && "
            "(cipherPassSub == NULL || strSize(cipherPassSub) == 0)))' failed");
    }

    // *****************************************************************************************************************************
    if (testBegin("infoNewLoad() and infoSave()"))
    {
        // Format error
        // --------------------------------------------------------------------------------------------------------------------------
        const Buffer *contentLoad = BUFSTRDEF(
            "[backrest]\n"
            "backrest-format=4\n");

        String *callbackContent = strNew("");

        TEST_ERROR(
            infoNewLoad(ioBufferReadNew(contentLoad), harnessInfoLoadNewCallback, callbackContent), FormatError,
            "expected format 5 but found 4");
        TEST_RESULT_STR(strPtr(callbackContent), "", "    check callback content");

        // Checksum not found
        // --------------------------------------------------------------------------------------------------------------------------
        contentLoad = BUFSTRDEF(
            "[backrest]\n"
            "backrest-format=5\n");

        TEST_ERROR(
            infoNewLoad(ioBufferReadNew(contentLoad), harnessInfoLoadNewCallback, callbackContent), ChecksumError,
            "invalid checksum, actual 'a3765a8c2c1e5d35274a0b0ce118f4031faff0bd' but no checksum found");
        TEST_RESULT_STR(strPtr(callbackContent), "", "    check callback content");

        // Checksum invalid
        // --------------------------------------------------------------------------------------------------------------------------
        contentLoad = BUFSTRDEF(
            "[backrest]\n"
            "backrest-checksum=\"BOGUS\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.17\"\n");

        TEST_ERROR(
            infoNewLoad(ioBufferReadNew(contentLoad), harnessInfoLoadNewCallback, callbackContent), ChecksumError,
            "invalid checksum, actual 'a9e578459485db14eb1093809a7964832be2779a' but expected 'BOGUS'");
        TEST_RESULT_STR(strPtr(callbackContent), "", "    check callback content");

        // Crypto expected
        // --------------------------------------------------------------------------------------------------------------------------
        contentLoad = BUFSTRDEF(
            "[backrest]\n"
            "backrest-checksum=\"BOGUS\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.17\"\n");

        IoRead *read = ioBufferReadNew(contentLoad);
        ioFilterGroupAdd(ioReadFilterGroup(read), cipherBlockNew(cipherModeDecrypt, cipherTypeAes256Cbc, BUFSTRDEF("X"), NULL));

        TEST_ERROR(
            infoNewLoad(read, harnessInfoLoadNewCallback, callbackContent), CryptoError,
            "cipher header invalid\n"
            "HINT: is or was the repo encrypted?");
        TEST_RESULT_STR(strPtr(callbackContent), "", "    check callback content");

        // Base file with other content in cipher (this is to test that future additions don't break the code)
        // --------------------------------------------------------------------------------------------------------------------------
        contentLoad = harnessInfoChecksumZ(
            "[cipher]\n"
            "cipher-other=1\n");

        Info *info = NULL;
        callbackContent = strNew("");

        TEST_ASSIGN(
            info, infoNewLoad(ioBufferReadNew(contentLoad), harnessInfoLoadNewCallback, callbackContent), "info with other cipher");
        TEST_RESULT_STR(strPtr(callbackContent), "", "    check callback content");
        TEST_RESULT_PTR(infoCipherPass(info), NULL, "    check cipher pass not set");

        // Base file with content
        // --------------------------------------------------------------------------------------------------------------------------
        contentLoad = harnessInfoChecksumZ(
            "[c]\n"
            "key=1\n"
            "\n"
            "[d]\n"
            "key=1\n");

        callbackContent = strNew("");

        TEST_ASSIGN(
            info, infoNewLoad(ioBufferReadNew(contentLoad), harnessInfoLoadNewCallback, callbackContent), "info with content");
        TEST_RESULT_STR(strPtr(callbackContent), "[c] key=1\n[d] key=1\n", "    check callback content");
        TEST_RESULT_PTR(infoCipherPass(info), NULL, "    check cipher pass not set");

        Buffer *contentSave = bufNew(0);

        TEST_RESULT_VOID(infoSave(info, ioBufferWriteNew(contentSave), testInfoSaveCallback, strNew("1")), "info save");
        TEST_RESULT_STR(strPtr(strNewBuf(contentSave)), strPtr(strNewBuf(contentLoad)), "   check save");

        TEST_RESULT_VOID(infoFree(info), "    free info");

        // File with content and cipher
        // --------------------------------------------------------------------------------------------------------------------------
        contentLoad = harnessInfoChecksumZ(
            "[c]\n"
            "key=1\n"
            "\n"
            "[cipher]\n"
            "cipher-pass=\"somepass\"\n"
            "\n"
            "[d]\n"
            "key=1\n");

        callbackContent = strNew("");

        TEST_ASSIGN(
            info,
            infoNewLoad(ioBufferReadNew(contentLoad), harnessInfoLoadNewCallback, callbackContent), "info with content and cipher");
        TEST_RESULT_STR(strPtr(callbackContent), "[c] key=1\n[d] key=1\n", "    check callback content");
        TEST_RESULT_STR(strPtr(infoCipherPass(info)), "somepass", "    check cipher pass set");

        contentSave = bufNew(0);

        TEST_RESULT_VOID(infoSave(info, ioBufferWriteNew(contentSave), testInfoSaveCallback, strNew("1")), "info save");
        TEST_RESULT_STR(strPtr(strNewBuf(contentSave)), strPtr(strNewBuf(contentLoad)), "   check save");

        // // Encryption error
        // //--------------------------------------------------------------------------------------------------------------------------
        // storageRemoveNP(storageLocalWrite(), fileName);
        // TEST_ERROR_FMT(
        //     infoNewLoad(storageLocal(), fileName, cipherTypeAes256Cbc, strNew("12345678"), harnessInfoLoadNewCallback, NULL),
        //     CryptoError,
        //     "unable to load info file '%s/test.ini' or '%s/test.ini.copy':\n"
        //         "FileMissingError: " STORAGE_ERROR_READ_MISSING "\n"
        //         "CryptoError: '%s/test.ini.copy' cipher header invalid\n"
        //         "HINT: is or was the repo encrypted?",
        //     testPath(), testPath(), strPtr(strNewFmt("%s/test.ini", testPath())), testPath());
        //
        // storageRemoveNP(storageLocalWrite(), fileNameCopy);
        //
        // // infoFree()
        // //--------------------------------------------------------------------------------------------------------------------------
        // TEST_RESULT_VOID(infoFree(info), "infoFree() - free info memory context");
    }

    // *****************************************************************************************************************************
    if (testBegin("infoLoad()"))
    {
        // One error
        //--------------------------------------------------------------------------------------------------------------------------
        TestInfoLoad testInfoLoad = {.errorTotal = 1};

        infoLoad(testInfoLoadCallback, &testInfoLoad);

        // Sucess
        //--------------------------------------------------------------------------------------------------------------------------
        testInfoLoad = (TestInfoLoad){0};

        infoLoad(testInfoLoadCallback, &testInfoLoad);
        // Info *info = infoNew(cipherTypeNone, NULL);
        //
        // TEST_RESULT_VOID(
        //     infoSave(info, storageTest, fileName, cipherTypeNone, NULL, testInfoSaveCallback, strNew("value1")), "save info");
        //
        // String *callbackContent = strNew("");
        //
        // TEST_RESULT_VOID(
        //     infoNewLoad(storageTest, fileName, cipherTypeNone, NULL, harnessInfoLoadNewCallback, callbackContent), "    reload info");
        // TEST_RESULT_STR(
        //     strPtr(callbackContent),
        //     "BEGIN\n"
        //     "[backup] key=value1\n"
        //     "END\n",
        //     "    check callback content");
        //
        // TEST_RESULT_BOOL(storageExistsNP(storageTest, fileName), true, "check main exists");
        // TEST_RESULT_BOOL(storageExistsNP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strPtr(fileName))), true, "check copy exists");
        //
        // TEST_ERROR(
        //     infoSave(info, storageTest, fileName, cipherTypeAes256Cbc, cipherPass, testInfoSaveCallback, NULL), AssertError,
        //     "assertion '!((cipherType != cipherTypeNone && this->cipherPass == NULL) || "
        //     "(cipherType == cipherTypeNone && this->cipherPass != NULL))' failed");
        //
        // // Add encryption
        // // -------------------------------------------------------------------------------------------------------------------------
        // info = infoNew(cipherTypeAes256Cbc, strNew("/hall-pass"));
        // TEST_RESULT_VOID(
        //     infoSave(info, storageTest, fileName, cipherTypeAes256Cbc, cipherPass, testInfoSaveCallback, strNew("value2")),
        //     "save encrypted info");
        //
        // callbackContent = strNew("");
        //
        // TEST_RESULT_VOID(
        //     infoNewLoad(storageTest, fileName, cipherTypeAes256Cbc, cipherPass, harnessInfoLoadNewCallback, callbackContent),
        //     "    reload info");
        // TEST_RESULT_STR(
        //     strPtr(callbackContent),
        //     "BEGIN\n"
        //     "[backup] key=value2\n"
        //     "END\n",
        //     "    check callback content");
        //
        // TEST_ERROR(
        //     infoSave(info, storageTest, fileName, cipherTypeNone, NULL, testInfoSaveCallback, NULL), AssertError,
        //     "assertion '!((cipherType != cipherTypeNone && this->cipherPass == NULL) || "
        //     "(cipherType == cipherTypeNone && this->cipherPass != NULL))' failed");
    }
}
