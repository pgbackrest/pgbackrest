/***********************************************************************************************************************************
Test Repository Format
***********************************************************************************************************************************/
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "common/io/io.h"
#include "version.h"

/***********************************************************************************************************************************
Data for testing
***********************************************************************************************************************************/
#define TEST_PASS                                                   "areallybadpassphrase"
#define TEST_PLAINTEXT                                              "plaintext"
#define TEST_BUFFER_SIZE                                            256

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("repoFormatValidate()"))
    {
        TEST_ERROR(
            repoFormatValidate(REPOSITORY_FORMAT_MIN - 1), FormatError,
            "repository format 4 is no longer supported by " PROJECT_NAME "\n"
            "HINT: " PROJECT_NAME " " PROJECT_VERSION " supports repository format 5 to 6.");
        TEST_ERROR(
            repoFormatValidate(REPOSITORY_FORMAT_MAX + 1), FormatError,
            "repository format 7 requires a newer version of " PROJECT_NAME "\n"
            "HINT: " PROJECT_NAME " " PROJECT_VERSION " supports repository format 5 to 6.");

        TEST_RESULT_VOID(repoFormatValidate(REPOSITORY_FORMAT_5), "format 5 is readable");
        TEST_RESULT_VOID(repoFormatValidate(REPOSITORY_FORMAT_6), "format 6 is readable");
    }

    // *****************************************************************************************************************************
    if (testBegin("repoFormatDigest()"))
    {
        TEST_RESULT_UINT(repoFormatDigest(REPOSITORY_FORMAT_5), hashTypeSha1, "format 5 derives with sha1");
        TEST_RESULT_UINT(repoFormatDigest(REPOSITORY_FORMAT_6), hashTypeSha256, "format 6 derives with sha256");
    }

    // *****************************************************************************************************************************
    if (testBegin("cipherBlockFormatNew()"))
    {
        const Buffer *const testPlainText = BUFSTRDEF(TEST_PLAINTEXT);
        const Buffer *const testPass = BUFSTRDEF(TEST_PASS);
        const CipherSpec *const cipherSpec = cipherSpecNewP(cipherTypeAes256Cbc, testPass);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("nothing is added when the repository is not encrypted");

        Buffer *plain = bufNew(0);
        IoWrite *plainWrite = ioBufferWriteNew(plain);

        TEST_RESULT_VOID(
            cipherBlockFormatFilterGroupWriteAdd(plain, ioWriteFilterGroup(plainWrite), cipherSpecNewNone(), REPOSITORY_FORMAT_6),
            "add write filter for no cipher");
        ioWriteOpen(plainWrite);
        ioWrite(plainWrite, testPlainText);
        ioWriteClose(plainWrite);

        TEST_RESULT_STR_Z(strNewBuf(plain), TEST_PLAINTEXT, "content is stored as it is");

        IoWrite *plainRead = ioBufferWriteNew(bufNew(0));
        TEST_RESULT_PTR_NE(
            cipherBlockFormatFilterGroupReadAdd(ioWriteFilterGroup(plainRead), cipherSpecNewNone()), NULL,
            "add read filter for no cipher");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write a header and read the format back from it");

        Buffer *headerBuffer = bufNew(0);
        IoWrite *headerWrite = ioBufferWriteNew(headerBuffer);

        cipherBlockFormatFilterGroupWriteAdd(headerBuffer, ioWriteFilterGroup(headerWrite), cipherSpec, REPOSITORY_FORMAT_6);
        ioWriteOpen(headerWrite);
        ioWrite(headerWrite, testPlainText);
        ioWriteClose(headerWrite);

        TEST_RESULT_BOOL(
            memcmp(bufPtrConst(headerBuffer), CIPHER_BLOCK_FORMAT_MAGIC "006_", CIPHER_BLOCK_FORMAT_HEADER_SIZE) == 0, true,
            "header names the format");

        // The format is not given on decrypt, so it comes from the header and is what the pass derives with
        Buffer *headerResult = bufNew(0);
        IoWrite *headerRead = ioBufferWriteNew(headerResult);
        IoFilterGroup *headerFilterGroup = ioWriteFilterGroup(headerRead);

        cipherBlockFormatFilterGroupReadAdd(headerFilterGroup, cipherSpec);
        ioWriteOpen(headerRead);
        ioWrite(headerRead, headerBuffer);
        ioWriteClose(headerRead);

        TEST_RESULT_STR_Z(strNewBuf(headerResult), TEST_PLAINTEXT, "content decrypted with the digest the header called for");
        TEST_RESULT_UINT(
            cipherBlockFormatResult(ioFilterGroupResultP(headerFilterGroup, CIPHER_BLOCK_FORMAT_FILTER_TYPE)),
            REPOSITORY_FORMAT_6, "filter reports the format");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("content is handed on in pieces when the destination cannot take it whole");

        // Content long enough that decryption has output to hand on while input is still arriving, so the destination fills before
        // the source has been consumed
        Buffer *const piecesPlainText = bufNew(TEST_BUFFER_SIZE);
        memset(bufPtr(piecesPlainText), 'x', bufSize(piecesPlainText));
        bufUsedSet(piecesPlainText, bufSize(piecesPlainText));

        Buffer *const piecesBuffer = bufNew(0);
        IoWrite *const piecesWrite = ioBufferWriteNew(piecesBuffer);

        cipherBlockFormatFilterGroupWriteAdd(piecesBuffer, ioWriteFilterGroup(piecesWrite), cipherSpec, REPOSITORY_FORMAT_6);
        ioWriteOpen(piecesWrite);
        ioWrite(piecesWrite, piecesPlainText);
        ioWriteClose(piecesWrite);

        IoRead *const readPieces = ioBufferReadNew(piecesBuffer);
        Buffer *const piecesResult = bufNew(0);
        Buffer *const piece = bufNew(4);

        cipherBlockFormatFilterGroupReadAdd(ioReadFilterGroup(readPieces), cipherSpec);

        ioBufferSizeSet(4);
        ioReadOpen(readPieces);

        while (!ioReadEof(readPieces))
        {
            bufUsedZero(piece);
            ioRead(readPieces, piece);
            bufCat(piecesResult, piece);
        }

        ioReadClose(readPieces);
        ioBufferSizeSet(TEST_BUFFER_SIZE);

        TEST_RESULT_BOOL(bufEq(piecesResult, piecesPlainText), true, "content decrypted in pieces");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("the header is read no matter how the input is split");

        Buffer *const splitResult = bufNew(0);
        IoWrite *const splitWrite = ioBufferWriteNew(splitResult);

        cipherBlockFormatFilterGroupReadAdd(ioWriteFilterGroup(splitWrite), cipherSpec);

        // Write in pieces smaller than the header so it arrives in more than one part, then a piece that completes it exactly, so
        // that what follows arrives with the header already read
        ioBufferSizeSet(4);
        ioWriteOpen(splitWrite);
        ioWrite(splitWrite, BUF(bufPtrConst(headerBuffer), 4));
        ioWrite(splitWrite, BUF(bufPtrConst(headerBuffer) + 4, 4));
        ioWrite(splitWrite, BUF(bufPtrConst(headerBuffer) + 8, bufUsed(headerBuffer) - 8));
        ioWriteClose(splitWrite);
        ioBufferSizeSet(TEST_BUFFER_SIZE);

        TEST_RESULT_STR_Z(strNewBuf(splitResult), TEST_PLAINTEXT, "content decrypted from split input");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("a file that begins with the magic was written before there was a header");

        Buffer *magicBuffer = bufNew(0);
        IoWrite *magicWrite = ioBufferWriteNew(magicBuffer);

        cipherBlockFormatFilterGroupWriteAdd(magicBuffer, ioWriteFilterGroup(magicWrite), cipherSpec, REPOSITORY_FORMAT_5);
        ioWriteOpen(magicWrite);
        ioWrite(magicWrite, testPlainText);
        ioWriteClose(magicWrite);

        TEST_RESULT_BOOL(
            memcmp(bufPtrConst(magicBuffer), CIPHER_BLOCK_MAGIC, CIPHER_BLOCK_MAGIC_SIZE) == 0, true,
            "a format before the header writes the magic");

        headerResult = bufNew(0);
        headerRead = ioBufferWriteNew(headerResult);
        headerFilterGroup = ioWriteFilterGroup(headerRead);

        cipherBlockFormatFilterGroupReadAdd(headerFilterGroup, cipherSpec);
        ioWriteOpen(headerRead);
        ioWrite(headerRead, magicBuffer);
        ioWriteClose(headerRead);

        TEST_RESULT_STR_Z(strNewBuf(headerResult), TEST_PLAINTEXT, "content decrypted");
        TEST_RESULT_UINT(
            cipherBlockFormatResult(ioFilterGroupResultP(headerFilterGroup, CIPHER_BLOCK_FORMAT_FILTER_TYPE)),
            REPOSITORY_FORMAT_5, "filter reports the format before the header");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("a format given on decrypt must be the one the header names");

        IoWrite *const headerMismatch = ioBufferWriteNew(bufNew(0));

        ioFilterGroupAdd(
            ioWriteFilterGroup(headerMismatch), cipherBlockFormatNewP(cipherSpec, .format = REPOSITORY_FORMAT_5));
        ioWriteOpen(headerMismatch);

        TEST_ERROR(ioWrite(headerMismatch, headerBuffer), FormatError, "expected repository format 5 but found 6");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("a format given on decrypt that the header agrees with is accepted, here from a pack");

        headerResult = bufNew(0);
        headerRead = ioBufferWriteNew(headerResult);

        ioFilterGroupAdd(
            ioWriteFilterGroup(headerRead),
            cipherBlockFormatNewPack(
                ioFilterParamList(cipherBlockFormatNewP(cipherSpec, .format = REPOSITORY_FORMAT_6))));
        ioWriteOpen(headerRead);
        ioWrite(headerRead, headerBuffer);
        ioWriteClose(headerRead);

        TEST_RESULT_STR_Z(strNewBuf(headerResult), TEST_PLAINTEXT, "content decrypted");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("a file too short to hold a header cannot have one");

        IoWrite *const headerShort = ioBufferWriteNew(bufNew(0));

        cipherBlockFormatFilterGroupReadAdd(ioWriteFilterGroup(headerShort), cipherSpec);
        ioWriteOpen(headerShort);
        ioWrite(headerShort, BUFSTRDEF("PGBR"));

        TEST_ERROR(ioWriteClose(headerShort), CryptoError, "cipher header missing");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("damaged headers");

        // Decrypt the buffer above after damaging a byte of the header, which is the same buffer for each case
        #define TEST_HEADER_DAMAGE(damageIdx, damageChar, errorType, errorMessage)                                                 \
            do                                                                                                                     \
            {                                                                                                                      \
                Buffer *const damaged = bufDup(headerBuffer);                                                                      \
                bufPtr(damaged)[damageIdx] = damageChar;                                                                           \
                                                                                                                                   \
                IoWrite *const write = ioBufferWriteNew(bufNew(0));                                                                \
                cipherBlockFormatFilterGroupReadAdd(ioWriteFilterGroup(write), cipherSpec);                                    \
                ioWriteOpen(write);                                                                                                \
                                                                                                                                   \
                TEST_ERROR(ioWrite(write, damaged), errorType, errorMessage);                                                      \
            }                                                                                                                      \
            while (0)

        // Where the format should be, so this version cannot tell what it is looking at
        TEST_HEADER_DAMAGE(CIPHER_BLOCK_FORMAT_MAGIC_SIZE, 'X', FormatError, "invalid cipher header");

        // The byte held back for later, which must be the one this version writes
        TEST_HEADER_DAMAGE(CIPHER_BLOCK_FORMAT_HEADER_SIZE - 1, 'X', FormatError, "invalid cipher header");

        // A format newer than this version can read, reported before anything is decrypted
        TEST_HEADER_DAMAGE(
            CIPHER_BLOCK_FORMAT_HEADER_SIZE - 2, '7', FormatError,
            "repository format 7 requires a newer version of " PROJECT_NAME "\n"
            "HINT: " PROJECT_NAME " " PROJECT_VERSION " supports repository format 5 to 6.");

        // A format older than this version can read
        TEST_HEADER_DAMAGE(
            CIPHER_BLOCK_FORMAT_HEADER_SIZE - 2, '4', FormatError,
            "repository format 4 is no longer supported by " PROJECT_NAME "\n"
            "HINT: " PROJECT_NAME " " PROJECT_VERSION " supports repository format 5 to 6.");

        // Neither a header nor the magic, which is what a file that was never encrypted looks like from here
        TEST_HEADER_DAMAGE(0, 'X', CryptoError, "cipher header invalid");

        #undef TEST_HEADER_DAMAGE
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
