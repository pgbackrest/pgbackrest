/***********************************************************************************************************************************
Test Block Cipher
***********************************************************************************************************************************/
#include <openssl/evp.h>

/***********************************************************************************************************************************
Data for testing
***********************************************************************************************************************************/
#define TEST_CIPHER                                                 "aes-256-cbc"
#define TEST_PASS                                                   "areallybadpassphrase"
#define TEST_PASS_SIZE                                              strlen(TEST_PASS)
#define TEST_PLAINTEXT                                              "plaintext"
#define TEST_DIGEST                                                 "sha256"
#define TEST_BUFFER_SIZE                                            256

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void testRun()
{
    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("blockCipherNew() and blockCipherFree()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(
            cipherBlockNew(
                cipherModeEncrypt, BOGUS_STR, TEST_PASS, TEST_PASS_SIZE, NULL), AssertError, "unable to load cipher 'BOGUS'");
        TEST_ERROR(
            cipherBlockNew(cipherModeEncrypt, NULL, TEST_PASS, TEST_PASS_SIZE, NULL), AssertError, "unable to load cipher '(null)'");
        TEST_ERROR(
            cipherBlockNew(
                cipherModeEncrypt, TEST_CIPHER, TEST_PASS, TEST_PASS_SIZE, BOGUS_STR), AssertError, "unable to load digest 'BOGUS'");

        // -------------------------------------------------------------------------------------------------------------------------
        CipherBlock *cipherBlock = cipherBlockNew(cipherModeEncrypt, TEST_CIPHER, TEST_PASS, TEST_PASS_SIZE, NULL);
        TEST_RESULT_STR(memContextName(cipherBlock->memContext), "cipherBlock", "mem context name is valid");
        TEST_RESULT_INT(cipherBlock->mode, cipherModeEncrypt, "mode is valid");
        TEST_RESULT_INT(cipherBlock->passSize, TEST_PASS_SIZE, "passphrase size is valid");
        TEST_RESULT_BOOL(memcmp(cipherBlock->pass, TEST_PASS, TEST_PASS_SIZE) == 0, true, "passphrase is valid");
        TEST_RESULT_BOOL(cipherBlock->saltDone, false, "salt done is false");
        TEST_RESULT_BOOL(cipherBlock->processDone, false, "process done is false");
        TEST_RESULT_INT(cipherBlock->headerSize, 0, "header size is 0");
        TEST_RESULT_PTR_NE(cipherBlock->cipher, NULL, "cipher is set");
        TEST_RESULT_PTR_NE(cipherBlock->digest, NULL, "digest is set");
        TEST_RESULT_PTR(cipherBlock->cipherContext, NULL, "cipher context is not set");
        memContextFree(cipherBlock->memContext);
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("Encrypt and Decrypt"))
    {
        char encryptBuffer[TEST_BUFFER_SIZE];
        int encryptSize = 0;
        char decryptBuffer[TEST_BUFFER_SIZE];
        int decryptSize = 0;

        // -------------------------------------------------------------------------------------------------------------------------
        CipherBlock *cipherBlock = cipherBlockNew(cipherModeEncrypt, TEST_CIPHER, TEST_PASS, TEST_PASS_SIZE, NULL);

        encryptSize = cipherBlockProcess(cipherBlock, TEST_PLAINTEXT, strlen(TEST_PLAINTEXT), encryptBuffer);

        TEST_RESULT_BOOL(cipherBlock->saltDone, true, "salt done is true");
        TEST_RESULT_BOOL(cipherBlock->processDone, true, "process done is true");
        TEST_RESULT_INT(cipherBlock->headerSize, 0, "header size is 0");
        TEST_RESULT_INT(encryptSize, CIPHER_BLOCK_HEADER_SIZE, "cipher size is header len");

        TEST_RESULT_INT(
            cipherBlockProcessSize(cipherBlock, strlen(TEST_PLAINTEXT)),
            strlen(TEST_PLAINTEXT) + EVP_MAX_BLOCK_LENGTH + CIPHER_BLOCK_MAGIC_SIZE + PKCS5_SALT_LEN, "check process size");

        encryptSize += cipherBlockProcess(cipherBlock, TEST_PLAINTEXT, strlen(TEST_PLAINTEXT), encryptBuffer + encryptSize);
        TEST_RESULT_INT(
            encryptSize, CIPHER_BLOCK_HEADER_SIZE + EVP_CIPHER_block_size(cipherBlock->cipher),
            "cipher size increases by one block");

        encryptSize += cipherBlockFlush(cipherBlock, encryptBuffer + encryptSize);
        TEST_RESULT_INT(
            encryptSize, CIPHER_BLOCK_HEADER_SIZE + (EVP_CIPHER_block_size(cipherBlock->cipher) * 2),
            "cipher size increases by one block on flush");

        cipherBlockFree(cipherBlock);

        // -------------------------------------------------------------------------------------------------------------------------
        cipherBlock = cipherBlockNew(cipherModeDecrypt, TEST_CIPHER, TEST_PASS, TEST_PASS_SIZE, NULL);

        decryptSize = cipherBlockProcess(cipherBlock, encryptBuffer, encryptSize, decryptBuffer);
        TEST_RESULT_INT(decryptSize, EVP_CIPHER_block_size(cipherBlock->cipher), "decrypt size is one block");

        decryptSize += cipherBlockFlush(cipherBlock, decryptBuffer + decryptSize);
        TEST_RESULT_INT(decryptSize, strlen(TEST_PLAINTEXT) * 2, "check final decrypt size");

        decryptBuffer[decryptSize] = 0;
        TEST_RESULT_STR(decryptBuffer, (TEST_PLAINTEXT TEST_PLAINTEXT), "check final decrypt buffer");

        // -------------------------------------------------------------------------------------------------------------------------
        cipherBlock = cipherBlockNew(cipherModeDecrypt, TEST_CIPHER, TEST_PASS, TEST_PASS_SIZE, NULL);

        decryptSize = 0;
        memset(decryptBuffer, 0, TEST_BUFFER_SIZE);

        decryptSize = cipherBlockProcess(cipherBlock, encryptBuffer, CIPHER_BLOCK_MAGIC_SIZE, decryptBuffer);
        TEST_RESULT_INT(decryptSize, 0, "no decrypt since header read is not complete");
        TEST_RESULT_BOOL(cipherBlock->saltDone, false, "salt done is false");
        TEST_RESULT_BOOL(cipherBlock->processDone, false, "process done is false");
        TEST_RESULT_INT(cipherBlock->headerSize, CIPHER_BLOCK_MAGIC_SIZE, "check header size");
        TEST_RESULT_BOOL(
            memcmp(cipherBlock->header, CIPHER_BLOCK_MAGIC, CIPHER_BLOCK_MAGIC_SIZE) == 0, true, "check header magic");

        decryptSize += cipherBlockProcess(
            cipherBlock, encryptBuffer + CIPHER_BLOCK_MAGIC_SIZE, PKCS5_SALT_LEN, decryptBuffer + decryptSize);
        TEST_RESULT_INT(decryptSize, 0, "no decrypt since no data processed yet");
        TEST_RESULT_BOOL(cipherBlock->saltDone, true, "salt done is true");
        TEST_RESULT_BOOL(cipherBlock->processDone, false, "process done is false");
        TEST_RESULT_INT(cipherBlock->headerSize, CIPHER_BLOCK_MAGIC_SIZE, "check header size (not increased)");
        TEST_RESULT_BOOL(
            memcmp(
                cipherBlock->header + CIPHER_BLOCK_MAGIC_SIZE, encryptBuffer + CIPHER_BLOCK_MAGIC_SIZE,
                PKCS5_SALT_LEN) == 0,
            true, "check header salt");

        decryptSize += cipherBlockProcess(
            cipherBlock, encryptBuffer + CIPHER_BLOCK_HEADER_SIZE, encryptSize - CIPHER_BLOCK_HEADER_SIZE,
            decryptBuffer + decryptSize);
        TEST_RESULT_INT(decryptSize, EVP_CIPHER_block_size(cipherBlock->cipher), "decrypt size is one block");

        decryptSize += cipherBlockFlush(cipherBlock, decryptBuffer + decryptSize);
        TEST_RESULT_INT(decryptSize, strlen(TEST_PLAINTEXT) * 2, "check final decrypt size");

        decryptBuffer[decryptSize] = 0;
        TEST_RESULT_STR(decryptBuffer, (TEST_PLAINTEXT TEST_PLAINTEXT), "check final decrypt buffer");

        // -------------------------------------------------------------------------------------------------------------------------
        cipherBlock = cipherBlockNew(cipherModeDecrypt, TEST_CIPHER, TEST_PASS, TEST_PASS_SIZE, NULL);

        TEST_ERROR(cipherBlockProcess(cipherBlock, "1234567890123456", 16, decryptBuffer), CipherError, "cipher header missing");

        cipherBlockProcess(cipherBlock, CIPHER_BLOCK_MAGIC "12345678", 16, decryptBuffer);
        cipherBlockProcess(cipherBlock, "1234567890123456", 16, decryptBuffer);

        TEST_ERROR(cipherBlockFlush(cipherBlock, decryptBuffer), CipherError, "unable to flush");
    }
}
