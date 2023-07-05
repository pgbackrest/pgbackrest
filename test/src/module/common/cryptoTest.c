/***********************************************************************************************************************************
Test Block Cipher
***********************************************************************************************************************************/
#include "common/io/bufferRead.h"
#include "common/io/filter/filter.h"
#include "common/io/io.h"
#include "common/type/json.h"

/***********************************************************************************************************************************
Data for testing
***********************************************************************************************************************************/
#define TEST_CIPHER                                                 "aes-256-cbc"
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

    const Buffer *testPass = BUFSTRDEF(TEST_PASS);
    const Buffer *testPlainText = BUFSTRDEF(TEST_PLAINTEXT);

    // *****************************************************************************************************************************
    if (testBegin("Common"))
    {
        TEST_RESULT_VOID(cryptoInit(), "initialize crypto");
        TEST_RESULT_VOID(cryptoInit(), "initialize crypto again");

        // -------------------------------------------------------------------------------------------------------------------------
        cryptoInit();

        TEST_RESULT_VOID(cryptoError(false, "no error here"), "no error");

        EVP_MD_CTX *context = EVP_MD_CTX_create();
        TEST_ERROR(
            cryptoError(EVP_DigestInit_ex(context, NULL, NULL) != 1, "unable to initialize hash context"), CryptoError,
            "unable to initialize hash context: "
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
            "[50331787]"
#else
            "[101187723]"
#endif
            " no digest set");
        EVP_MD_CTX_destroy(context);

        TEST_ERROR(cryptoError(true, "no error"), CryptoError, "no error: [0] no details available");

        // Test if the buffer was overrun
        // -------------------------------------------------------------------------------------------------------------------------
        unsigned char buffer[256] = {0};

        cryptoRandomBytes(buffer, sizeof(buffer) - 1);
        TEST_RESULT_BOOL(
            buffer[sizeof(buffer) - 1] == 0, true, "check that buffer did not overrun (though random byte could be 0)");

        // Count bytes that are not zero (there shouldn't be all zeroes)
        // -------------------------------------------------------------------------------------------------------------------------
        int nonZeroTotal = 0;

        for (unsigned int charIdx = 0; charIdx < sizeof(buffer) - 1; charIdx++)
            if (buffer[charIdx] != 0)                               // {uncoverable_branch - ok if there are no zeros}
                nonZeroTotal++;

        TEST_RESULT_INT_NE(nonZeroTotal, 0, "check that there are non-zero values in the buffer");
    }

    // *****************************************************************************************************************************
    if (testBegin("CipherBlock"))
    {
        // Cipher and digest errors
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(
            cipherBlockNewP(cipherModeEncrypt, cipherTypeNone, BUFSTRZ(TEST_PASS)), AssertError, "unable to load cipher 'none'");
        TEST_ERROR(
            cipherBlockNewP(cipherModeEncrypt, cipherTypeAes256Cbc, testPass, .digest = STRDEF(BOGUS_STR)), AssertError,
            "unable to load digest 'BOGUS'");

        // Initialization of object
        // -------------------------------------------------------------------------------------------------------------------------
        CipherBlock *cipherBlock = (CipherBlock *)ioFilterDriver(
            cipherBlockNewP(cipherModeEncrypt, cipherTypeAes256Cbc, BUFSTRZ(TEST_PASS)));
        TEST_RESULT_INT(cipherBlock->mode, cipherModeEncrypt, "mode is valid");
        TEST_RESULT_UINT(bufSize(cipherBlock->pass), strlen(TEST_PASS), "passphrase size is valid");
        TEST_RESULT_BOOL(memcmp(bufPtrConst(cipherBlock->pass), TEST_PASS, strlen(TEST_PASS)) == 0, true, "passphrase is valid");
        TEST_RESULT_BOOL(cipherBlock->saltDone, false, "salt done is false");
        TEST_RESULT_BOOL(cipherBlock->processDone, false, "process done is false");
        TEST_RESULT_UINT(cipherBlock->headerSize, 0, "header size is 0");
        TEST_RESULT_PTR_NE(cipherBlock->cipher, NULL, "cipher is set");
        TEST_RESULT_PTR_NE(cipherBlock->digest, NULL, "digest is set");
        TEST_RESULT_PTR(cipherBlock->cipherContext, NULL, "cipher context is not set");

        // Encrypt
        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *encryptBuffer = bufNew(TEST_BUFFER_SIZE);

        IoFilter *blockEncryptFilter = cipherBlockNewP(cipherModeEncrypt, cipherTypeAes256Cbc, testPass);
        blockEncryptFilter = cipherBlockNewPack(ioFilterParamList(blockEncryptFilter));
        CipherBlock *blockEncrypt = (CipherBlock *)ioFilterDriver(blockEncryptFilter);

        TEST_RESULT_UINT(
            cipherBlockProcessSize(blockEncrypt, strlen(TEST_PLAINTEXT)),
            strlen(TEST_PLAINTEXT) + EVP_MAX_BLOCK_LENGTH + CIPHER_BLOCK_MAGIC_SIZE + PKCS5_SALT_LEN, "check process size");

        bufLimitSet(encryptBuffer, CIPHER_BLOCK_MAGIC_SIZE);
        ioFilterProcessInOut(blockEncryptFilter, testPlainText, encryptBuffer);
        TEST_RESULT_UINT(bufUsed(encryptBuffer), CIPHER_BLOCK_MAGIC_SIZE, "cipher size is magic size");
        TEST_RESULT_BOOL(ioFilterInputSame(blockEncryptFilter), true, "filter needs same input");

        bufLimitSet(encryptBuffer, CIPHER_BLOCK_MAGIC_SIZE + PKCS5_SALT_LEN);
        ioFilterProcessInOut(blockEncryptFilter, testPlainText, encryptBuffer);
        TEST_RESULT_BOOL(ioFilterInputSame(blockEncryptFilter), false, "filter does not need same input");

        TEST_RESULT_BOOL(blockEncrypt->saltDone, true, "salt done is true");
        TEST_RESULT_BOOL(blockEncrypt->processDone, true, "process done is true");
        TEST_RESULT_UINT(blockEncrypt->headerSize, 0, "header size is 0");
        TEST_RESULT_UINT(bufUsed(encryptBuffer), CIPHER_BLOCK_HEADER_SIZE, "cipher size is header len");

        TEST_RESULT_UINT(
            cipherBlockProcessSize(blockEncrypt, strlen(TEST_PLAINTEXT)),
            strlen(TEST_PLAINTEXT) + EVP_MAX_BLOCK_LENGTH, "check process size");

        bufLimitSet(
            encryptBuffer, CIPHER_BLOCK_MAGIC_SIZE + PKCS5_SALT_LEN + (size_t)EVP_CIPHER_block_size(blockEncrypt->cipher) / 2);
        ioFilterProcessInOut(blockEncryptFilter, testPlainText, encryptBuffer);
        bufLimitSet(
            encryptBuffer, CIPHER_BLOCK_MAGIC_SIZE + PKCS5_SALT_LEN + (size_t)EVP_CIPHER_block_size(blockEncrypt->cipher));
        ioFilterProcessInOut(blockEncryptFilter, testPlainText, encryptBuffer);
        bufLimitClear(encryptBuffer);

        TEST_RESULT_UINT(
            bufUsed(encryptBuffer), CIPHER_BLOCK_HEADER_SIZE + (size_t)EVP_CIPHER_block_size(blockEncrypt->cipher),
            "cipher size increases by one block");
        TEST_RESULT_BOOL(ioFilterDone(blockEncryptFilter), false, "filter is not done");

        ioFilterProcessInOut(blockEncryptFilter, NULL, encryptBuffer);
        TEST_RESULT_UINT(
            bufUsed(encryptBuffer), CIPHER_BLOCK_HEADER_SIZE + (size_t)(EVP_CIPHER_block_size(blockEncrypt->cipher) * 2),
            "cipher size increases by one block on flush");
        TEST_RESULT_BOOL(ioFilterDone(blockEncryptFilter), true, "filter is done");

        ioFilterFree(blockEncryptFilter);

        // Decrypt in one pass
        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *decryptBuffer = bufNew(TEST_BUFFER_SIZE);

        IoFilter *blockDecryptFilter = cipherBlockNewP(cipherModeDecrypt, cipherTypeAes256Cbc, testPass, .digest = STRDEF("sha1"));
        blockDecryptFilter = cipherBlockNewPack(ioFilterParamList(blockDecryptFilter));
        CipherBlock *blockDecrypt = (CipherBlock *)ioFilterDriver(blockDecryptFilter);

        TEST_RESULT_UINT(
            cipherBlockProcessSize(blockDecrypt, bufUsed(encryptBuffer)), bufUsed(encryptBuffer) + EVP_MAX_BLOCK_LENGTH,
            "check process size");

        ioFilterProcessInOut(blockDecryptFilter, encryptBuffer, decryptBuffer);
        TEST_RESULT_UINT_INT(bufUsed(decryptBuffer), EVP_CIPHER_block_size(blockDecrypt->cipher), "decrypt size is one block");

        ioFilterProcessInOut(blockDecryptFilter, NULL, decryptBuffer);
        TEST_RESULT_UINT(bufUsed(decryptBuffer), strlen(TEST_PLAINTEXT) * 2, "check final decrypt size");

        TEST_RESULT_STR_Z(strNewBuf(decryptBuffer), TEST_PLAINTEXT TEST_PLAINTEXT, "check final decrypt buffer");

        ioFilterFree(blockDecryptFilter);

        // Decrypt in small chunks to test buffering
        // -------------------------------------------------------------------------------------------------------------------------
        blockDecryptFilter = cipherBlockNewP(cipherModeDecrypt, cipherTypeAes256Cbc, testPass);
        blockDecrypt = (CipherBlock *)ioFilterDriver(blockDecryptFilter);

        bufUsedZero(decryptBuffer);

        ioFilterProcessInOut(blockDecryptFilter, bufNewC(bufPtr(encryptBuffer), CIPHER_BLOCK_MAGIC_SIZE), decryptBuffer);
        TEST_RESULT_UINT(bufUsed(decryptBuffer), 0, "no decrypt since header read is not complete");
        TEST_RESULT_BOOL(blockDecrypt->saltDone, false, "salt done is false");
        TEST_RESULT_BOOL(blockDecrypt->processDone, false, "process done is false");
        TEST_RESULT_UINT(blockDecrypt->headerSize, CIPHER_BLOCK_MAGIC_SIZE, "check header size");
        TEST_RESULT_BOOL(
            memcmp(blockDecrypt->header, CIPHER_BLOCK_MAGIC, CIPHER_BLOCK_MAGIC_SIZE) == 0, true, "check header magic");

        ioFilterProcessInOut(
            blockDecryptFilter, bufNewC(bufPtr(encryptBuffer) + CIPHER_BLOCK_MAGIC_SIZE, PKCS5_SALT_LEN), decryptBuffer);
        TEST_RESULT_UINT(bufUsed(decryptBuffer), 0, "no decrypt since no data processed yet");
        TEST_RESULT_BOOL(blockDecrypt->saltDone, true, "salt done is true");
        TEST_RESULT_BOOL(blockDecrypt->processDone, false, "process done is false");
        TEST_RESULT_UINT(blockDecrypt->headerSize, CIPHER_BLOCK_MAGIC_SIZE, "check header size (not increased)");
        TEST_RESULT_BOOL(
            memcmp(
                blockDecrypt->header + CIPHER_BLOCK_MAGIC_SIZE, bufPtr(encryptBuffer) + CIPHER_BLOCK_MAGIC_SIZE,
                PKCS5_SALT_LEN) == 0,
            true, "check header salt");

        ioFilterProcessInOut(
            blockDecryptFilter,
            bufNewC(bufPtr(encryptBuffer) + CIPHER_BLOCK_HEADER_SIZE, bufUsed(encryptBuffer) - CIPHER_BLOCK_HEADER_SIZE),
            decryptBuffer);
        TEST_RESULT_UINT_INT(bufUsed(decryptBuffer), EVP_CIPHER_block_size(blockDecrypt->cipher), "decrypt size is one block");

        ioFilterProcessInOut(blockDecryptFilter, NULL, decryptBuffer);
        TEST_RESULT_UINT(bufUsed(decryptBuffer), strlen(TEST_PLAINTEXT) * 2, "check final decrypt size");

        TEST_RESULT_STR_Z(strNewBuf(decryptBuffer), TEST_PLAINTEXT TEST_PLAINTEXT, "check final decrypt buffer");

        ioFilterFree(blockDecryptFilter);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("encrypt zero byte file with no magic");

        blockEncryptFilter = cipherBlockNewP(cipherModeEncrypt, cipherTypeAes256Cbc, testPass, .raw = true);
        blockEncrypt = (CipherBlock *)ioFilterDriver(blockEncryptFilter);

        bufUsedZero(encryptBuffer);

        ioFilterProcessInOut(blockEncryptFilter, NULL, encryptBuffer);
        TEST_RESULT_UINT(bufUsed(encryptBuffer), 24, "check remaining size");

        ioFilterFree(blockEncryptFilter);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on decrypt expecting magic");

        blockDecryptFilter = cipherBlockNewP(cipherModeDecrypt, cipherTypeAes256Cbc, testPass);
        TEST_ERROR(ioFilterProcessInOut(blockDecryptFilter, encryptBuffer, decryptBuffer), CryptoError, "cipher header invalid");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("decrypt zero byte file with no magic");

        blockDecryptFilter = cipherBlockNewP(cipherModeDecrypt, cipherTypeAes256Cbc, testPass, .raw = true);
        blockDecrypt = (CipherBlock *)ioFilterDriver(blockDecryptFilter);

        bufUsedZero(decryptBuffer);

        ioFilterProcessInOut(blockDecryptFilter, encryptBuffer, decryptBuffer);
        TEST_RESULT_UINT(bufUsed(decryptBuffer), 0, "0 bytes processed");
        ioFilterProcessInOut(blockDecryptFilter, NULL, decryptBuffer);
        TEST_RESULT_UINT(bufUsed(decryptBuffer), 0, "0 bytes on flush");

        ioFilterFree(blockDecryptFilter);

        // Invalid cipher header
        // -------------------------------------------------------------------------------------------------------------------------
        blockDecryptFilter = cipherBlockNewP(cipherModeDecrypt, cipherTypeAes256Cbc, testPass);
        blockDecrypt = (CipherBlock *)ioFilterDriver(blockDecryptFilter);

        TEST_ERROR(
            ioFilterProcessInOut(blockDecryptFilter, BUFSTRDEF("1234567890123456"), decryptBuffer), CryptoError,
            "cipher header invalid");

        ioFilterFree(blockDecryptFilter);

        // Invalid encrypted data cannot be flushed
        // -------------------------------------------------------------------------------------------------------------------------
        blockDecryptFilter = cipherBlockNewP(cipherModeDecrypt, cipherTypeAes256Cbc, testPass);
        blockDecrypt = (CipherBlock *)ioFilterDriver(blockDecryptFilter);

        bufUsedZero(decryptBuffer);

        ioFilterProcessInOut(blockDecryptFilter, BUFSTRDEF(CIPHER_BLOCK_MAGIC "12345678"), decryptBuffer);
        ioFilterProcessInOut(blockDecryptFilter, BUFSTRDEF("1234567890123456"), decryptBuffer);

        TEST_ERROR(ioFilterProcessInOut(blockDecryptFilter, NULL, decryptBuffer), CryptoError, "unable to flush");

        ioFilterFree(blockDecryptFilter);

        // File with no header should not flush
        // -------------------------------------------------------------------------------------------------------------------------
        blockDecryptFilter = cipherBlockNewP(cipherModeDecrypt, cipherTypeAes256Cbc, testPass);
        blockDecrypt = (CipherBlock *)ioFilterDriver(blockDecryptFilter);

        bufUsedZero(decryptBuffer);

        TEST_ERROR(ioFilterProcessInOut(blockDecryptFilter, NULL, decryptBuffer), CryptoError, "cipher header missing");

        ioFilterFree(blockDecryptFilter);

        // File with header only should error
        // -------------------------------------------------------------------------------------------------------------------------
        blockDecryptFilter = cipherBlockNewP(cipherModeDecrypt, cipherTypeAes256Cbc, testPass);
        blockDecrypt = (CipherBlock *)ioFilterDriver(blockDecryptFilter);

        bufUsedZero(decryptBuffer);

        ioFilterProcessInOut(blockDecryptFilter, BUFSTRDEF(CIPHER_BLOCK_MAGIC "12345678"), decryptBuffer);
        TEST_ERROR(ioFilterProcessInOut(blockDecryptFilter, NULL, decryptBuffer), CryptoError, "unable to flush");

        ioFilterFree(blockDecryptFilter);

        // Helper function
        // -------------------------------------------------------------------------------------------------------------------------
        IoFilterGroup *filterGroup = ioFilterGroupNew();

        TEST_RESULT_PTR(
            cipherBlockFilterGroupAdd(filterGroup, cipherTypeNone, cipherModeEncrypt, NULL), filterGroup, "   no filter add");
        TEST_RESULT_UINT(ioFilterGroupSize(filterGroup), 0, "    check no filter add");

        TEST_RESULT_VOID(
            cipherBlockFilterGroupAdd(filterGroup, cipherTypeAes256Cbc, cipherModeEncrypt, STRDEF("X")), "   filter add");
        TEST_RESULT_UINT(ioFilterGroupSize(filterGroup), 1, "    check filter add");
    }

    // *****************************************************************************************************************************
    if (testBegin("CryptoHash"))
    {
        IoFilter *hash = NULL;

        TEST_ERROR(cryptoHashNew(STRID5("bogus", 0x13a9de20)), AssertError, "unable to load hash 'bogus'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(hash, cryptoHashNew(hashTypeSha1), "create sha1 hash");
        TEST_RESULT_VOID(ioFilterFree(hash), "    free hash");

        // -------------------------------------------------------------------------------------------------------------------------
        PackWrite *packWrite = pckWriteNewP();
        pckWriteStrIdP(packWrite, hashTypeSha1);
        pckWriteEndP(packWrite);

        TEST_ASSIGN(hash, cryptoHashNewPack(pckWriteResult(packWrite)), "create sha1 hash");
        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, cryptoHash((CryptoHash *)ioFilterDriver(hash))), HASH_TYPE_SHA1_ZERO, "    check empty hash");
        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, cryptoHash((CryptoHash *)ioFilterDriver(hash))), HASH_TYPE_SHA1_ZERO,
            "    check empty hash again");
        TEST_RESULT_VOID(ioFilterFree(hash), "    free hash");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(hash, cryptoHashNew(hashTypeSha1), "create sha1 hash");
        TEST_RESULT_VOID(ioFilterProcessIn(hash, BUFSTRZ("1")), "    add 1");
        TEST_RESULT_VOID(ioFilterProcessIn(hash, BUFSTR(STRDEF("2"))), "    add 2");
        TEST_RESULT_VOID(ioFilterProcessIn(hash, BUFSTRDEF("3")), "    add 3");
        TEST_RESULT_VOID(ioFilterProcessIn(hash, BUFSTRDEF("4")), "    add 4");
        TEST_RESULT_VOID(ioFilterProcessIn(hash, BUFSTRDEF("5")), "    add 5");

        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, pckReadBinP(pckReadNew(ioFilterResult(hash)))), "8cb2237d0679ca88db6464eac60da96345513964",
            "    check small hash");
        TEST_RESULT_VOID(ioFilterFree(hash), "    free hash");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("md5 hash - zero bytes");

        TEST_ASSIGN(hash, cryptoHashNew(hashTypeMd5), "create md5 hash");
        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, pckReadBinP(pckReadNew(ioFilterResult(hash)))), HASH_TYPE_MD5_ZERO, "check empty hash");

        // Exercise most of the conditions in the local MD5 code
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("md5 hash - mixed bytes");

        TEST_ASSIGN(hash, cryptoHashNew(hashTypeMd5), "create md5 hash");
        TEST_RESULT_VOID(ioFilterProcessIn(hash, BUFSTRZ("1")), "add 1");
        TEST_RESULT_VOID(ioFilterProcessIn(hash, BUFSTRZ("123456789012345678901234567890123")), "add 32 bytes");
        TEST_RESULT_VOID(
            ioFilterProcessIn(
                hash,
                BUFSTRZ(
                    "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
                    "12345678901234567890123456789012345678901234567890")),
            "add 160 bytes");
        TEST_RESULT_VOID(
            ioFilterProcessIn(hash, BUFSTRZ("12345678901234567890123456789001234567890012345678901234")), "add 58 bytes");

        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, pckReadBinP(pckReadNew(ioFilterResult(hash)))), "3318600bc9c1d379e91e4bae90721243",
            "check hash");

        // Full coverage of local MD5 requires processing > 511MB of data but that makes the test run too long. Instead we'll cheat
        // a bit and initialize the context at 511MB to start. This does not produce a valid MD5 hash but does provide coverage of
        // that one condition cheaply.
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("md5 hash - > 0x1fffffff bytes");

        TEST_ASSIGN(hash, cryptoHashNew(hashTypeMd5), "create md5 hash");
        ((CryptoHash *)ioFilterDriver(hash))->md5Context.lo = 0x1fffffff;

        TEST_RESULT_VOID(ioFilterProcessIn(hash, BUFSTRZ("1")), "add 1");
        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, pckReadBinP(pckReadNew(ioFilterResult(hash)))), "5c99876f9cafa7f485eac9c7a8a2764c",
            "check hash");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(hash, cryptoHashNew(hashTypeSha256), "create sha256 hash");
        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, pckReadBinP(pckReadNew(ioFilterResult(hash)))), HASH_TYPE_SHA256_ZERO, "    check empty hash");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, cryptoHashOne(hashTypeSha1, BUFSTRDEF("12345"))), "8cb2237d0679ca88db6464eac60da96345513964",
            "    check small hash");
        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, cryptoHashOne(hashTypeSha1, BUFSTRDEF(""))), HASH_TYPE_SHA1_ZERO, "    check empty hash");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR_Z(
            strNewEncode(
                encodingHex,
                cryptoHmacOne(
                    hashTypeSha256,
                    BUFSTRDEF("AWS4wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY"),
                    BUFSTRDEF("20170412"))),
            "8b05c497afe9e1f42c8ada4cb88392e118649db1e5c98f0f0fb0a158bdd2dd76",
            "    check hmac");
    }

    // *****************************************************************************************************************************
    if (testBegin("XxHash"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("xxHashOne");

        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, xxHashOne(16, BUFSTRDEF(""))), "99aa06d3014798d86001c324468d497f", "check empty hash 16");
        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, xxHashOne(16, BUFSTRDEF("12345\n"))), "1a3e11127b8856b804f0f99dc9fa4b56",
            "check small hash 16");

        TEST_RESULT_STR_Z(strNewEncode(encodingHex, xxHashOne(5, BUFSTRDEF(""))), "99aa06d301", "check empty hash 5");
        TEST_RESULT_STR_Z(strNewEncode(encodingHex, xxHashOne(5, BUFSTRDEF("12345\n"))), "1a3e11127b", "check small hash 5");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("XxHash");

        ioBufferSizeSet(2);

        const Buffer *message = BUFSTRDEF("");
        IoRead *read = ioBufferReadNew(message);
        ioFilterGroupAdd(ioReadFilterGroup(read), xxHashNew(16));

        ioReadDrain(read);

        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, pckReadBinP(ioFilterGroupResultP(ioReadFilterGroup(read), XX_HASH_FILTER_TYPE))),
            "99aa06d3014798d86001c324468d497f", "check empty hash 16");

        message = BUFSTRDEF("12345\n");
        read = ioBufferReadNew(message);
        ioFilterGroupAdd(ioReadFilterGroup(read), xxHashNew(16));

        ioReadDrain(read);

        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, pckReadBinP(ioFilterGroupResultP(ioReadFilterGroup(read), XX_HASH_FILTER_TYPE))),
            "1a3e11127b8856b804f0f99dc9fa4b56", "check small hash 16");

        message = BUFSTRDEF("");
        read = ioBufferReadNew(message);
        ioFilterGroupAdd(ioReadFilterGroup(read), xxHashNew(5));

        ioReadDrain(read);

        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, pckReadBinP(ioFilterGroupResultP(ioReadFilterGroup(read), XX_HASH_FILTER_TYPE))),
            "99aa06d301", "check empty hash 5");

        message = BUFSTRDEF("12345\n");
        read = ioBufferReadNew(message);
        ioFilterGroupAdd(ioReadFilterGroup(read), xxHashNew(5));

        ioReadDrain(read);

        TEST_RESULT_STR_Z(
            strNewEncode(encodingHex, pckReadBinP(ioFilterGroupResultP(ioReadFilterGroup(read), XX_HASH_FILTER_TYPE))),
            "1a3e11127b", "check small hash 5");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
