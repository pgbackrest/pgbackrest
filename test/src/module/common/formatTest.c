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

        CipherSpecMap *const keyMapDefault = cipherSpecMapNew();
        cipherSpecMapAdd(keyMapDefault, CIPHER_SPEC_MAP_ID_DEFAULT_STR, cipherSpec);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("nothing is added when the repository is not encrypted");

        Buffer *plain = bufNew(0);
        IoWrite *plainWrite = ioBufferWriteNew(plain);

        TEST_RESULT_VOID(
            cipherBlockFormatFilterGroupWriteAddP(ioWriteFilterGroup(plainWrite), cipherSpecNewNone(), REPOSITORY_FORMAT_6),
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

        cipherBlockFormatFilterGroupWriteAddP(ioWriteFilterGroup(headerWrite), cipherSpec, REPOSITORY_FORMAT_6);
        ioWriteOpen(headerWrite);
        ioWrite(headerWrite, testPlainText);
        ioWriteClose(headerWrite);

        TEST_RESULT_BOOL(
            memcmp(bufPtrConst(headerBuffer), CIPHER_BLOCK_FORMAT_MAGIC "006_", CIPHER_BLOCK_FORMAT_HEADER_SIZE) == 0, true,
            "header contains the format");

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

        cipherBlockFormatFilterGroupWriteAddP(ioWriteFilterGroup(piecesWrite), cipherSpec, REPOSITORY_FORMAT_6);
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

        cipherBlockFormatFilterGroupWriteAddP(ioWriteFilterGroup(magicWrite), cipherSpec, REPOSITORY_FORMAT_5);
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
        TEST_TITLE("a format given on decrypt must be the one the header contains");

        IoWrite *const headerMismatch = ioBufferWriteNew(bufNew(0));

        ioFilterGroupAdd(
            ioWriteFilterGroup(headerMismatch), cipherBlockFormatNewP(keyMapDefault, .format = REPOSITORY_FORMAT_5));
        ioWriteOpen(headerMismatch);

        TEST_ERROR(ioWrite(headerMismatch, headerBuffer), FormatError, "expected repository format 5 but found 6");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("a format given on decrypt that the header agrees with is accepted, here from a pack");

        headerResult = bufNew(0);
        headerRead = ioBufferWriteNew(headerResult);

        ioFilterGroupAdd(
            ioWriteFilterGroup(headerRead),
            cipherBlockFormatNewPack(
                ioFilterParamList(cipherBlockFormatNewP(keyMapDefault, .format = REPOSITORY_FORMAT_6))));
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

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read with the key id stored in the header");

        const CipherSpec *const keySpecOld = cipherSpecNewP(cipherTypeAes256Cbc, BUFSTRDEF("oldkey"), .digest = hashTypeSha1);
        const CipherSpec *const keySpecNew = cipherSpecNewP(cipherTypeAes256Cbc, BUFSTRDEF("newkey"));

        CipherSpecMap *const keyMap = cipherSpecMapNew();
        cipherSpecMapAdd(keyMap, STRDEF(CIPHER_SPEC_MAP_ID_DEFAULT), keySpecOld);
        cipherSpecMapAdd(keyMap, STRDEF("7"), keySpecNew);

        Buffer *const keyBuffer = bufNew(0);
        IoRead *const keyWrite = ioBufferReadNew(testPlainText);
        Buffer *const keyPiece = bufNew(4);

        // Build the write filters from their param lists, as the remote protocol does
        IoFilterGroup *const keyWriteGroup = ioFilterGroupNew();
        cipherBlockFormatFilterGroupWriteAddP(keyWriteGroup, keySpecNew, REPOSITORY_FORMAT_6, .keyId = STRDEF("7"));

        PackRead *const keyWriteParam = pckReadNew(ioFilterGroupParamAll(keyWriteGroup));

        TEST_RESULT_UINT(pckReadStrIdP(keyWriteParam), CIPHER_BLOCK_FILTER_TYPE, "block cipher filter");
        ioFilterGroupAdd(ioReadFilterGroup(keyWrite), cipherBlockNewPack(pckReadPackP(keyWriteParam)));
        TEST_RESULT_UINT(pckReadStrIdP(keyWriteParam), CIPHER_BLOCK_FORMAT_HEADER_FILTER_TYPE, "header filter");
        ioFilterGroupAdd(ioReadFilterGroup(keyWrite), cipherBlockFormatHeaderNewPack(pckReadPackP(keyWriteParam)));

        // Take the result in pieces smaller than the header so the header is written in more than one part
        ioBufferSizeSet(4);
        ioReadOpen(keyWrite);

        while (!ioReadEof(keyWrite))
        {
            bufUsedZero(keyPiece);
            ioRead(keyWrite, keyPiece);
            bufCat(keyBuffer, keyPiece);
        }

        ioReadClose(keyWrite);
        ioBufferSizeSet(TEST_BUFFER_SIZE);

        TEST_RESULT_BOOL(
            memcmp(bufPtrConst(keyBuffer), CIPHER_BLOCK_FORMAT_MAGIC "006K\0017", CIPHER_BLOCK_FORMAT_HEADER_SIZE + 2) == 0, true,
            "header contains the key after its length");

        Buffer *keyResult = bufNew(0);
        IoWrite *keyRead = ioBufferWriteNew(keyResult);

        cipherBlockFormatFilterGroupReadAddMap(ioWriteFilterGroup(keyRead), keyMap);
        ioWriteOpen(keyRead);
        ioWrite(keyRead, keyBuffer);
        ioWriteClose(keyRead);

        TEST_RESULT_STR_Z(strNewBuf(keyResult), TEST_PLAINTEXT, "content decrypted with the key the header contains");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("key id split across input, from a pack");

        keyResult = bufNew(0);
        keyRead = ioBufferWriteNew(keyResult);

        // Build from a pack, as the remote protocol does
        ioFilterGroupAdd(
            ioWriteFilterGroup(keyRead), cipherBlockFormatNewPack(ioFilterParamList(cipherBlockFormatNewP(keyMap))));

        // Split so the length byte and the key id each arrive separately
        ioBufferSizeSet(4);
        ioWriteOpen(keyRead);
        ioWrite(keyRead, BUF(bufPtrConst(keyBuffer), CIPHER_BLOCK_FORMAT_HEADER_SIZE));
        ioWrite(keyRead, BUF(bufPtrConst(keyBuffer) + CIPHER_BLOCK_FORMAT_HEADER_SIZE, 1));
        ioWrite(keyRead, BUF(bufPtrConst(keyBuffer) + CIPHER_BLOCK_FORMAT_HEADER_SIZE + 1, 1));
        ioWrite(
            keyRead,
            BUF(
                bufPtrConst(keyBuffer) + CIPHER_BLOCK_FORMAT_HEADER_SIZE + 2,
                bufUsed(keyBuffer) - CIPHER_BLOCK_FORMAT_HEADER_SIZE - 2));
        ioWriteClose(keyRead);
        ioBufferSizeSet(TEST_BUFFER_SIZE);

        TEST_RESULT_STR_Z(strNewBuf(keyResult), TEST_PLAINTEXT, "content decrypted from split input");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("key digest is used, not the digest of the format");

        // A key kept by migration derives with SHA-1 even in a file written at a format that derives with SHA-256
        Buffer *const digestBuffer = bufNew(0);
        IoWrite *const digestWrite = ioBufferWriteNew(digestBuffer);

        cipherBlockFormatFilterGroupWriteAddP(ioWriteFilterGroup(digestWrite), keySpecOld, REPOSITORY_FORMAT_5);
        ioWriteOpen(digestWrite);
        ioWrite(digestWrite, testPlainText);
        ioWriteClose(digestWrite);

        // Replace the magic with a format 6 header, which leaves content that only the SHA-1 key can decrypt
        memcpy(bufPtr(digestBuffer), CIPHER_BLOCK_FORMAT_MAGIC "006_", CIPHER_BLOCK_FORMAT_HEADER_SIZE);

        Buffer *const digestResult = bufNew(0);
        IoWrite *const digestRead = ioBufferWriteNew(digestResult);

        cipherBlockFormatFilterGroupReadAddMap(ioWriteFilterGroup(digestRead), keyMap);
        ioWriteOpen(digestRead);
        ioWrite(digestRead, digestBuffer);
        ioWriteClose(digestRead);

        TEST_RESULT_STR_Z(strNewBuf(digestResult), TEST_PLAINTEXT, "content decrypted with the digest of the key");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file with no key id uses the migrated key");

        Buffer *const migratedBuffer = bufNew(0);
        IoWrite *const migratedWrite = ioBufferWriteNew(migratedBuffer);

        cipherBlockFormatFilterGroupWriteAddP(ioWriteFilterGroup(migratedWrite), keySpecOld, REPOSITORY_FORMAT_5);
        ioWriteOpen(migratedWrite);
        ioWrite(migratedWrite, testPlainText);
        ioWriteClose(migratedWrite);

        Buffer *const migratedResult = bufNew(0);
        IoWrite *const migratedRead = ioBufferWriteNew(migratedResult);

        cipherBlockFormatFilterGroupReadAddMap(ioWriteFilterGroup(migratedRead), keyMap);
        ioWriteOpen(migratedRead);
        ioWrite(migratedRead, migratedBuffer);
        ioWriteClose(migratedRead);

        TEST_RESULT_STR_Z(strNewBuf(migratedResult), TEST_PLAINTEXT, "content decrypted with the migrated key");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("key id not in the map");

        CipherSpecMap *const keyMapShort = cipherSpecMapNew();
        cipherSpecMapAdd(keyMapShort, STRDEF(CIPHER_SPEC_MAP_ID_DEFAULT), keySpecOld);

        IoWrite *const keyMissing = ioBufferWriteNew(bufNew(0));
        cipherBlockFormatFilterGroupReadAddMap(ioWriteFilterGroup(keyMissing), keyMapShort);
        ioWriteOpen(keyMissing);

        TEST_ERROR(ioWrite(keyMissing, keyBuffer), CryptoError, "unable to find cipher key '7'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("key id in a file read with a single key");

        IoWrite *const keyUnexpected = ioBufferWriteNew(bufNew(0));
        cipherBlockFormatFilterGroupReadAdd(ioWriteFilterGroup(keyUnexpected), keySpecNew);
        ioWriteOpen(keyUnexpected);

        TEST_ERROR(ioWrite(keyUnexpected, keyBuffer), CryptoError, "unable to find cipher key '7'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("key id with zero length");

        Buffer *const keyEmpty = bufDup(keyBuffer);
        bufPtr(keyEmpty)[CIPHER_BLOCK_FORMAT_HEADER_SIZE] = 0;

        IoWrite *const keyEmptyRead = ioBufferWriteNew(bufNew(0));
        cipherBlockFormatFilterGroupReadAddMap(ioWriteFilterGroup(keyEmptyRead), keyMap);
        ioWriteOpen(keyEmptyRead);

        TEST_ERROR(ioWrite(keyEmptyRead, keyEmpty), FormatError, "invalid cipher header");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no filter added when the map is empty");

        IoWrite *const keyNone = ioBufferWriteNew(bufNew(0));
        TEST_RESULT_PTR_NE(
            cipherBlockFormatFilterGroupReadAddMap(ioWriteFilterGroup(keyNone), cipherSpecMapNew()), NULL,
            "add read filter for no keys");
        TEST_RESULT_UINT(ioFilterGroupSize(ioWriteFilterGroup(keyNone)), 0, "no filter added");
    }

    // *****************************************************************************************************************************
    if (testBegin("cipherSpecMapNew()"))
    {
        const CipherSpec *const cipherSpec1 = cipherSpecNewP(cipherTypeAes256Cbc, BUFSTRDEF("key1"), .digest = hashTypeSha256);
        const CipherSpec *const cipherSpec2 = cipherSpecNewP(cipherTypeAes256Cbc, BUFSTRDEF("key2"), .digest = hashTypeSha1);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("add and get keys");

        CipherSpecMap *map = NULL;
        TEST_ASSIGN(map, cipherSpecMapNew(), "new map");
        TEST_RESULT_UINT(cipherSpecMapSize(map), 0, "no keys");

        // Add out of order to check that the map sorts
        TEST_RESULT_VOID(cipherSpecMapAdd(map, STRDEF("2"), cipherSpec2), "add 2");
        TEST_RESULT_VOID(cipherSpecMapAdd(map, STRDEF("0"), cipherSpec1), "add 0");

        TEST_RESULT_UINT(cipherSpecMapSize(map), 2, "two keys");
        TEST_RESULT_STR_Z(cipherSpecMapGetIdx(map, 0)->id, "0", "first id");
        TEST_RESULT_STR_Z(cipherSpecMapGetIdx(map, 1)->id, "2", "second id");

        // Each key keeps its own digest
        TEST_RESULT_UINT(cipherSpecDigest(cipherSpecMapGet(map, STRDEF("0"))), hashTypeSha256, "key 0 digest");
        TEST_RESULT_UINT(cipherSpecDigest(cipherSpecMapGet(map, STRDEF("2"))), hashTypeSha1, "key 2 digest");
        TEST_RESULT_STR_Z(strNewBuf(cipherSpecPass(cipherSpecMapGet(map, STRDEF("2")))), "key2", "key 2 pass");

        TEST_RESULT_STR_Z(cipherSpecMapIdCurrent(map), "2", "adding the key with no id did not make it current");

        // A map with only the key that has no id has no current key
        CipherSpecMap *const mapDefault = cipherSpecMapNew();
        TEST_RESULT_VOID(cipherSpecMapAdd(mapDefault, CIPHER_SPEC_MAP_ID_DEFAULT_STR, cipherSpec1), "add key with no id");
        TEST_RESULT_PTR(cipherSpecMapIdCurrent(mapDefault), NULL, "no current key");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("duplicate keeps the current key");

        TEST_RESULT_PTR(cipherSpecMapIdCurrent(cipherSpecMapDup(cipherSpecMapNew())), NULL, "dup empty map");

        CipherSpecMap *mapDup = NULL;
        TEST_ASSIGN(mapDup, cipherSpecMapDup(map), "dup map");
        TEST_RESULT_UINT(cipherSpecMapSize(mapDup), 2, "two keys");
        TEST_RESULT_STR_Z(cipherSpecMapIdCurrent(mapDup), "2", "current key id");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("key id not found");

        TEST_ERROR(cipherSpecMapGet(map, STRDEF("1")), CryptoError, "unable to find cipher key '1'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pack round trip");

        PackWrite *const packWrite = pckWriteNewP();
        TEST_RESULT_VOID(cipherSpecMapPack(packWrite, map), "pack map");
        pckWriteEndP(packWrite);

        CipherSpecMap *mapPack = NULL;
        TEST_ASSIGN(mapPack, cipherSpecMapNewPack(pckReadNew(pckWriteResult(packWrite))), "unpack map");

        TEST_RESULT_UINT(cipherSpecMapSize(mapPack), 2, "two keys");
        TEST_RESULT_STR_Z(strNewBuf(cipherSpecPass(cipherSpecMapGet(mapPack, STRDEF("0")))), "key1", "key 0 pass");
        TEST_RESULT_UINT(cipherSpecDigest(cipherSpecMapGet(mapPack, STRDEF("2"))), hashTypeSha1, "key 2 digest");
        TEST_RESULT_STR_Z(cipherSpecMapIdCurrent(mapPack), "2", "current key id");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("log ids but not keys");

        char logBuf[STACK_TRACE_PARAM_MAX];

        TEST_RESULT_VOID(FUNCTION_LOG_OBJECT_FORMAT(map, cipherSpecMapToLog, logBuf, sizeof(logBuf)), "cipherSpecMapToLog");
        TEST_RESULT_Z(logBuf, "{ids: [0, 2]}", "check log");

        TEST_RESULT_VOID(cipherSpecMapFree(map), "free map");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
