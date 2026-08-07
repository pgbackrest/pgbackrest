/***********************************************************************************************************************************
Test Info Handler
***********************************************************************************************************************************/
#include "common/crypto/cipherBlock.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "storage/posix/storage.h"

#include "harness/info.h"

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
    if (infoSaveSection(infoSaveData, "c", sectionNext))
        infoSaveValue(infoSaveData, "c", "key", (String *)data);

    if (infoSaveSection(infoSaveData, "d", sectionNext))
        infoSaveValue(infoSaveData, "d", "key", (String *)data);
}

/***********************************************************************************************************************************
Store info file content the way it would be stored at a format, i.e. with the header and digest that go with the format
***********************************************************************************************************************************/
static Buffer *
testInfoEncrypt(const Buffer *const content, const unsigned int format, const CipherSpec *const cipherSpec)
{
    Buffer *const result = bufNew(0);
    IoWrite *const write = infoWriteNew(result, format, cipherSpec);

    ioWriteOpen(write);
    ioWrite(write, content);
    ioWriteClose(write);

    return result;
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    // *****************************************************************************************************************************
    if (testBegin("infoNew() and infoNewInternal()"))
    {
        Info *info = NULL;

        TEST_ASSIGN(
            info, infoNew(REPOSITORY_FORMAT_DEFAULT, cipherSpecNewP(cipherTypeAes256Cbc, BUFSTRDEF("123xyz"))),
            "infoNew(cipher)");
        TEST_RESULT_STR_Z(strNewBuf(cipherSpecPass(infoCipherSpec(info))), "123xyz", "    cipherPass is set");

        TEST_ASSIGN(info, infoNew(REPOSITORY_FORMAT_DEFAULT, NULL), "infoNew(NULL)");
        TEST_RESULT_UINT(cipherSpecType(infoCipherSpec(info)), cipherTypeNone, "    cipher spec is none");
    }

    // *****************************************************************************************************************************
    if (testBegin("infoNewLoad() and infoSave()"))
    {
        // Format error
        // -------------------------------------------------------------------------------------------------------------------------
        const Buffer *contentLoad = BUFSTRDEF(
            "[backrest]\n"
            "backrest-format=4\n");

        String *callbackContent = strNew();

        TEST_ERROR(
            infoNewLoadP(ioBufferReadNew(contentLoad), cipherSpecNewNone(), harnessInfoLoadNewCallback, callbackContent),
            FormatError,
            "repository format 4 is no longer supported by pgBackRest\n"
            "HINT: pgBackRest " PROJECT_VERSION " supports repository format 5 to 6.");
        TEST_RESULT_STR_Z(callbackContent, "", "    check callback content");

        // Format newer than supported
        // -------------------------------------------------------------------------------------------------------------------------
        contentLoad = BUFSTRDEF(
            "[backrest]\n"
            "backrest-format=7\n");

        TEST_ERROR(
            infoNewLoadP(ioBufferReadNew(contentLoad), cipherSpecNewNone(), harnessInfoLoadNewCallback, callbackContent),
            FormatError,
            "repository format 7 requires a newer version of pgBackRest\n"
            "HINT: pgBackRest " PROJECT_VERSION " supports repository format 5 to 6.");

        // Checksum not found
        // -------------------------------------------------------------------------------------------------------------------------
        contentLoad = BUFSTRDEF(
            "[backrest]\n"
            "backrest-format=5\n");

        TEST_ERROR(
            infoNewLoadP(ioBufferReadNew(contentLoad), cipherSpecNewNone(), harnessInfoLoadNewCallback, callbackContent),
            ChecksumError, "invalid checksum, actual 'a3765a8c2c1e5d35274a0b0ce118f4031faff0bd' but no checksum found");
        TEST_RESULT_STR_Z(callbackContent, "", "    check callback content");

        // Checksum invalid
        // -------------------------------------------------------------------------------------------------------------------------
        contentLoad = BUFSTRDEF(
            "[backrest]\n"
            "backrest-checksum=\"BOGUS\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.17\"\n"
            "bogus=\"BOGUS\"\n");

        TEST_ERROR(
            infoNewLoadP(ioBufferReadNew(contentLoad), cipherSpecNewNone(), harnessInfoLoadNewCallback, callbackContent),
            ChecksumError, "invalid checksum, actual 'fe989a75dcf7a0261e57d210707c0db741462763' but expected 'BOGUS'");
        TEST_RESULT_STR_Z(callbackContent, "", "    check callback content");

        // Crypto expected
        // -------------------------------------------------------------------------------------------------------------------------
        contentLoad = BUFSTRDEF(
            "[backrest]\n"
            "backrest-checksum=\"BOGUS\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.17\"\n");

        IoRead *read = ioBufferReadNew(contentLoad);
        ioFilterGroupAdd(
            ioReadFilterGroup(read),
            cipherBlockNewP(cipherModeDecrypt, cipherSpecNewP(cipherTypeAes256Cbc, BUFSTRDEF("X"))));

        TEST_ERROR(
            infoNewLoadP(read, cipherSpecNewNone(), harnessInfoLoadNewCallback, callbackContent), CryptoError,
            "cipher header invalid\n"
            "HINT: is or was the repo encrypted?");
        TEST_RESULT_STR_Z(callbackContent, "", "    check callback content");

        // Base file with other content in cipher (this is to test that future additions don't break the code)
        // -------------------------------------------------------------------------------------------------------------------------
        contentLoad = harnessInfoChecksumZ(
            "[cipher]\n"
            "cipher-other=1\n");

        Info *info = NULL;
        callbackContent = strNew();

        TEST_ASSIGN(
            info, infoNewLoadP(ioBufferReadNew(contentLoad), cipherSpecNewNone(), harnessInfoLoadNewCallback, callbackContent),
            "info with other cipher");
        TEST_RESULT_STR_Z(callbackContent, "", "    check callback content");
        TEST_RESULT_UINT(cipherSpecType(infoCipherSpec(info)), cipherTypeNone, "    check cipher pass not set");

        // Base file with content
        // -------------------------------------------------------------------------------------------------------------------------
        contentLoad = harnessInfoChecksumZ(
            "[c]\n"
            "key=1\n"
            "\n"
            "[d]\n"
            "key=1\n");

        callbackContent = strNew();

        TEST_ASSIGN(
            info, infoNewLoadP(ioBufferReadNew(contentLoad), cipherSpecNewNone(), harnessInfoLoadNewCallback, callbackContent),
            "info with content");
        TEST_RESULT_STR_Z(callbackContent, "[c] key=1\n[d] key=1\n", "    check callback content");
        TEST_RESULT_UINT(cipherSpecType(infoCipherSpec(info)), cipherTypeNone, "    check cipher pass not set");

        Buffer *contentSave = bufNew(0);

        TEST_RESULT_VOID(infoSave(info, ioBufferWriteNew(contentSave), testInfoSaveCallback, strNewZ("1")), "info save");
        TEST_RESULT_STR(strNewBuf(contentSave), strNewBuf(contentLoad), "   check save");
        TEST_RESULT_UINT(infoFormat(info), REPOSITORY_FORMAT_DEFAULT, "    check format");

        // The format is read from the file rather than assumed, and written back out as read
        // -------------------------------------------------------------------------------------------------------------------------
        contentLoad = harnessInfoChecksumFormat(
            REPOSITORY_FORMAT_6,
            STRDEF(
                "[c]\n"
                "key=1\n"
                "\n"
                "[d]\n"
                "key=1\n"));

        callbackContent = strNew();

        TEST_ASSIGN(
            info, infoNewLoadP(ioBufferReadNew(contentLoad), cipherSpecNewNone(), harnessInfoLoadNewCallback, callbackContent),
            "info format 6");
        TEST_RESULT_UINT(infoFormat(info), REPOSITORY_FORMAT_6, "    check format");

        contentSave = bufNew(0);

        TEST_RESULT_VOID(infoSave(info, ioBufferWriteNew(contentSave), testInfoSaveCallback, strNewZ("1")), "info save");
        TEST_RESULT_STR(strNewBuf(contentSave), strNewBuf(contentLoad), "    check save preserves format");

        // Set the format
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(infoFormatSet(info, REPOSITORY_FORMAT_5), "set format");
        TEST_RESULT_UINT(infoFormat(info), REPOSITORY_FORMAT_5, "    check format");

        // File with content and cipher
        // -------------------------------------------------------------------------------------------------------------------------
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

        const CipherSpec *const cipherSpec = cipherSpecNewP(cipherTypeAes256Cbc, BUFSTRDEF("x"));

        // A file with no header, e.g. a manifest, is decrypted with the spec as it was given since there is nothing to say the
        // digest should be anything else
        TEST_ASSIGN(
            info,
            infoNewLoadP(
                ioBufferReadNew(harnessInfoEncrypt(contentLoad, cipherSpec)), cipherSpec, harnessInfoLoadNewCallback,
                callbackContent),
            "info with content and cipher");
        TEST_RESULT_STR_Z(callbackContent, "[c] key=1\n[d] key=1\n", "    check callback content");
        TEST_RESULT_STR_Z(strNewBuf(cipherSpecPass(infoCipherSpec(info))), "somepass", "    check cipher pass set");
        TEST_RESULT_UINT(cipherSpecDigest(infoCipherSpec(info)), hashTypeSha1, "    check cipher sub digest");
        TEST_RESULT_STR_Z(infoBackrestVersion(info), PROJECT_VERSION, "    check backrest version");

        contentSave = bufNew(0);

        TEST_RESULT_VOID(infoSave(info, ioBufferWriteNew(contentSave), testInfoSaveCallback, strNewZ("1")), "info save");
        TEST_RESULT_STR(strNewBuf(contentSave), strNewBuf(contentLoad), "   check save");

        // Header
        // -------------------------------------------------------------------------------------------------------------------------
        // An unencrypted file has no header no matter the format, since the format is read from the content
        contentSave = bufNew(0);

        TEST_RESULT_VOID(
            infoSave(
                info, infoWriteNew(contentSave, REPOSITORY_FORMAT_6, cipherSpecNewNone()), testInfoSaveCallback, strNewZ("1")),
            "info save");
        TEST_RESULT_BOOL(strBeginsWithZ(strNewBuf(contentSave), "PGBR"), false, "    check no header");

        contentLoad = harnessInfoChecksumFormat(
            REPOSITORY_FORMAT_6,
            STRDEF(
                "[cipher]\n"
                "cipher-pass=\"somepass\"\n"));

        callbackContent = strNew();

        TEST_ASSIGN(
            info,
            infoNewLoadP(
                ioBufferReadNew(testInfoEncrypt(contentLoad, REPOSITORY_FORMAT_6, cipherSpec)), cipherSpec,
                harnessInfoLoadNewCallback, callbackContent, .header = true),
            "info with header");
        TEST_RESULT_UINT(infoFormat(info), REPOSITORY_FORMAT_6, "    check format");
        TEST_RESULT_UINT(cipherSpecDigest(infoCipherSpec(info)), hashTypeSha256, "    check cipher sub digest");

        // The content on its own, which is how a caller that wants the file rather than the values in it reads an info file
        IoRead *const infoRead = ioBufferReadNew(testInfoEncrypt(contentLoad, REPOSITORY_FORMAT_6, cipherSpec));
        ioReadOpen(infoRead);

        IoRead *const contentRead = infoContentRead(infoRead, cipherSpec, NULL);
        ioReadOpen(contentRead);

        TEST_RESULT_STR(strNewBuf(ioReadBuf(contentRead)), strNewBuf(contentLoad), "info content read");

        // A file written before the header existed is read as the format that had none
        contentLoad = harnessInfoChecksumZ("[c]\nkey=1\n");

        TEST_ASSIGN(
            info,
            infoNewLoadP(
                ioBufferReadNew(testInfoEncrypt(contentLoad, REPOSITORY_FORMAT_5, cipherSpec)), cipherSpec,
                harnessInfoLoadNewCallback, callbackContent, .header = true),
            "info with no header");
        TEST_RESULT_UINT(infoFormat(info), REPOSITORY_FORMAT_5, "    check format");

        // A file too short to hold a header cannot have one
        TEST_ERROR(
            infoNewLoadP(
                ioBufferReadNew(BUFSTRDEF("PGBR")), cipherSpec, harnessInfoLoadNewCallback, callbackContent, .header = true),
            CryptoError,
            "cipher header missing\n"
            "HINT: is or was the repo encrypted?");

        // A file with neither header was never encrypted, which is reported the way the cipher reports it
        TEST_ERROR(
            infoNewLoadP(
                ioBufferReadNew(BUFSTRDEF("[backrest]\nbackrest-format=5\n")), cipherSpec, harnessInfoLoadNewCallback,
                callbackContent, .header = true),
            CryptoError,
            "cipher header invalid\n"
            "HINT: is or was the repo encrypted?");

        // Header uses the byte held back for later, so this version does not know what it is looking at
        Buffer *contentHeader = testInfoEncrypt(contentLoad, REPOSITORY_FORMAT_6, cipherSpec);
        bufPtr(contentHeader)[7] = 'X';

        TEST_ERROR(
            infoNewLoadP(
                ioBufferReadNew(contentHeader), cipherSpec, harnessInfoLoadNewCallback, callbackContent, .header = true),
            FormatError, "invalid info file header");

        // Header damaged where the format should be
        contentHeader = testInfoEncrypt(contentLoad, REPOSITORY_FORMAT_6, cipherSpec);
        bufPtr(contentHeader)[5] = 'X';

        TEST_ERROR(
            infoNewLoadP(
                ioBufferReadNew(contentHeader), cipherSpec, harnessInfoLoadNewCallback, callbackContent, .header = true),
            FormatError, "invalid info file header");

        // Header names a format this version cannot read, which is reported before anything is decrypted
        contentHeader = testInfoEncrypt(contentLoad, REPOSITORY_FORMAT_6, cipherSpec);
        bufPtr(contentHeader)[6] = '7';

        TEST_ERROR(
            infoNewLoadP(
                ioBufferReadNew(contentHeader), cipherSpec, harnessInfoLoadNewCallback, callbackContent, .header = true),
            FormatError,
            "repository format 7 requires a newer version of pgBackRest\n"
            "HINT: pgBackRest " PROJECT_VERSION " supports repository format 5 to 6.");

        // Header and content disagree about the format
        TEST_ERROR(
            infoNewLoadP(
                ioBufferReadNew(testInfoEncrypt(contentLoad, REPOSITORY_FORMAT_6, cipherSpec)), cipherSpec,
                harnessInfoLoadNewCallback, callbackContent, .header = true),
            FormatError, "repository format 5 does not match header format 6");
    }

    // *****************************************************************************************************************************
    if (testBegin("infoLoad()"))
    {
        // One error
        // -------------------------------------------------------------------------------------------------------------------------
        TestInfoLoad testInfoLoad = {.test = 1};

        TEST_ERROR(
            infoLoad(STRDEF("unable to load info file"), testInfoLoadCallback, &testInfoLoad), ChecksumError,
            "unable to load info file:\n"
            "ChecksumError: checksum error");

        // Two errors (same error)
        // -------------------------------------------------------------------------------------------------------------------------
        testInfoLoad = (TestInfoLoad){.test = 2};

        TEST_ERROR(
            infoLoad(STRDEF("unable to load info file(s)"), testInfoLoadCallback, &testInfoLoad), FormatError,
            "unable to load info file(s):\n"
            "FormatError: format error\n"
            "FormatError: format error");

        // Four errors (mixed)
        // -------------------------------------------------------------------------------------------------------------------------
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
        // -------------------------------------------------------------------------------------------------------------------------
        testInfoLoad = (TestInfoLoad){0};

        infoLoad(STRDEF("SHOULD BE NO ERROR"), testInfoLoadCallback, &testInfoLoad);
    }
}
