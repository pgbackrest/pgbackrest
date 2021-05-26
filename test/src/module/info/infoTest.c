/***********************************************************************************************************************************
Test Info Handler
***********************************************************************************************************************************/
#include "common/crypto/cipherBlock.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "storage/posix/storage.h"

#include "common/harnessInfo.h"

/***********************************************************************************************************************************
Test load callback
***********************************************************************************************************************************/
typedef struct TestInfoLoad
{
    unsigned int test;
} TestInfoLoad;

static bool
testInfoLoadCallback(void *data, unsigned int try)
{
    TestInfoLoad *testInfoLoad = (TestInfoLoad *)data;

    if (testInfoLoad->test == 1)
    {
        if (try == 0)
            THROW(ChecksumError, "checksum error");
        else
            return false;
    }

    if (testInfoLoad->test == 2)
    {
        if (try < 2)
            THROW(FormatError, "format error");
        else
            return false;
    }

    if (testInfoLoad->test == 3)
    {
        if (try == 0)
            THROW(FileMissingError, "file missing error");
        else if (try == 1)
            THROW(ChecksumError, "checksum error\nHINT: have you checked the thing?");
        else if (try == 2)
            THROW(FormatError, "format error");
        else if (try == 3)
            THROW(FileMissingError, "file missing error");
        else
            return false;
    }

    return true;
}

/***********************************************************************************************************************************
Test save callbacks
***********************************************************************************************************************************/
static void
testInfoSaveCallback(void *data, const String *sectionNext, InfoSave *infoSaveData)
{
    if (infoSaveSection(infoSaveData, STRDEF("c"), sectionNext))
        infoSaveValue(infoSaveData, STRDEF("c"), STRDEF("key"), (String *)data);

    if (infoSaveSection(infoSaveData, STRDEF("d"), sectionNext))
        infoSaveValue(infoSaveData, STRDEF("d"), STRDEF("key"), (String *)data);
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    // *****************************************************************************************************************************
    if (testBegin("infoNew() and infoNewInternal()"))
    {
        Info *info = NULL;

        TEST_ASSIGN(info, infoNew(STRDEF("123xyz")), "infoNew(cipher)");
        TEST_RESULT_STR_Z(infoCipherPass(info), "123xyz", "    cipherPass is set");

        TEST_ASSIGN(info, infoNew(NULL), "infoNew(NULL)");
        TEST_RESULT_STR(infoCipherPass(info), NULL, "    cipherPass is NULL");
    }

    // *****************************************************************************************************************************
    if (testBegin("infoNewLoad() and infoSave()"))
    {
        // Format error
        // --------------------------------------------------------------------------------------------------------------------------
        const Buffer *contentLoad = BUFSTRDEF(
            "[backrest]\n"
            "backrest-format=4\n");

        String *callbackContent = strNew();

        TEST_ERROR(
            infoNewLoad(ioBufferReadNew(contentLoad), harnessInfoLoadNewCallback, callbackContent), FormatError,
            "expected format 5 but found 4");
        TEST_RESULT_STR_Z(callbackContent, "", "    check callback content");

        // Checksum not found
        // --------------------------------------------------------------------------------------------------------------------------
        contentLoad = BUFSTRDEF(
            "[backrest]\n"
            "backrest-format=5\n");

        TEST_ERROR(
            infoNewLoad(ioBufferReadNew(contentLoad), harnessInfoLoadNewCallback, callbackContent), ChecksumError,
            "invalid checksum, actual 'a3765a8c2c1e5d35274a0b0ce118f4031faff0bd' but no checksum found");
        TEST_RESULT_STR_Z(callbackContent, "", "    check callback content");

        // Checksum invalid
        // --------------------------------------------------------------------------------------------------------------------------
        contentLoad = BUFSTRDEF(
            "[backrest]\n"
            "backrest-checksum=\"BOGUS\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.17\"\n"
            "bogus=\"BOGUS\"\n");

        TEST_ERROR(
            infoNewLoad(ioBufferReadNew(contentLoad), harnessInfoLoadNewCallback, callbackContent), ChecksumError,
            "invalid checksum, actual 'fe989a75dcf7a0261e57d210707c0db741462763' but expected 'BOGUS'");
        TEST_RESULT_STR_Z(callbackContent, "", "    check callback content");

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
        TEST_RESULT_STR_Z(callbackContent, "", "    check callback content");

        // Base file with other content in cipher (this is to test that future additions don't break the code)
        // --------------------------------------------------------------------------------------------------------------------------
        contentLoad = harnessInfoChecksumZ(
            "[cipher]\n"
            "cipher-other=1\n");

        Info *info = NULL;
        callbackContent = strNew();

        TEST_ASSIGN(
            info, infoNewLoad(ioBufferReadNew(contentLoad), harnessInfoLoadNewCallback, callbackContent), "info with other cipher");
        TEST_RESULT_STR_Z(callbackContent, "", "    check callback content");
        TEST_RESULT_STR(infoCipherPass(info), NULL, "    check cipher pass not set");

        // Base file with content
        // --------------------------------------------------------------------------------------------------------------------------
        contentLoad = harnessInfoChecksumZ(
            "[c]\n"
            "key=1\n"
            "\n"
            "[d]\n"
            "key=1\n");

        callbackContent = strNew();

        TEST_ASSIGN(
            info, infoNewLoad(ioBufferReadNew(contentLoad), harnessInfoLoadNewCallback, callbackContent), "info with content");
        TEST_RESULT_STR_Z(callbackContent, "[c] key=1\n[d] key=1\n", "    check callback content");
        TEST_RESULT_STR(infoCipherPass(info), NULL, "    check cipher pass not set");

        Buffer *contentSave = bufNew(0);

        TEST_RESULT_VOID(infoSave(info, ioBufferWriteNew(contentSave), testInfoSaveCallback, strNewZ("1")), "info save");
        TEST_RESULT_STR(strNewBuf(contentSave), strNewBuf(contentLoad), "   check save");

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

        callbackContent = strNew();

        TEST_ASSIGN(
            info,
            infoNewLoad(ioBufferReadNew(contentLoad), harnessInfoLoadNewCallback, callbackContent), "info with content and cipher");
        TEST_RESULT_STR_Z(callbackContent, "[c] key=1\n[d] key=1\n", "    check callback content");
        TEST_RESULT_STR_Z(infoCipherPass(info), "somepass", "    check cipher pass set");
        TEST_RESULT_STR_Z(infoBackrestVersion(info), PROJECT_VERSION, "    check backrest version");

        contentSave = bufNew(0);

        TEST_RESULT_VOID(infoSave(info, ioBufferWriteNew(contentSave), testInfoSaveCallback, strNewZ("1")), "info save");
        TEST_RESULT_STR(strNewBuf(contentSave), strNewBuf(contentLoad), "   check save");
    }

    // *****************************************************************************************************************************
    if (testBegin("infoLoad()"))
    {
        // One error
        //--------------------------------------------------------------------------------------------------------------------------
        TestInfoLoad testInfoLoad = {.test = 1};

        TEST_ERROR(
            infoLoad(STRDEF("unable to load info file"), testInfoLoadCallback, &testInfoLoad), ChecksumError,
            "unable to load info file:\n"
                "ChecksumError: checksum error");

        // Two errors (same error)
        //--------------------------------------------------------------------------------------------------------------------------
        testInfoLoad = (TestInfoLoad){.test = 2};

        TEST_ERROR(
            infoLoad(STRDEF("unable to load info file(s)"), testInfoLoadCallback, &testInfoLoad), FormatError,
            "unable to load info file(s):\n"
                "FormatError: format error\n"
                "FormatError: format error");

        // Four errors (mixed)
        //--------------------------------------------------------------------------------------------------------------------------
        testInfoLoad = (TestInfoLoad){.test = 3};

        TEST_ERROR(
            infoLoad(STRDEF("unable to load info file(s)"), testInfoLoadCallback, &testInfoLoad), FileOpenError,
            "unable to load info file(s):\n"
                "FileMissingError: file missing error\n"
                "ChecksumError: checksum error\n"
                "HINT: have you checked the thing?\n"
                "FormatError: format error\n"
                "FileMissingError: file missing error");

        // Success
        //--------------------------------------------------------------------------------------------------------------------------
        testInfoLoad = (TestInfoLoad){0};

        infoLoad(STRDEF("SHOULD BE NO ERROR"), testInfoLoadCallback, &testInfoLoad);
    }
}
