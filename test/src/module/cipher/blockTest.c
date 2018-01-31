/***********************************************************************************************************************************
Test Block Cipher
***********************************************************************************************************************************/
#include <openssl/evp.h>

#include "cipher/cipher.h"

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
void
testRun()
{
    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("blockCipherNew() and blockCipherFree()"))
    {
        // Cipher and digest errors
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(
            cipherBlockNew(
                cipherModeEncrypt, BOGUS_STR, (unsigned char *)TEST_PASS, TEST_PASS_SIZE, NULL), AssertError,
                "unable to load cipher 'BOGUS'");
        TEST_ERROR(
            cipherBlockNew(cipherModeEncrypt, NULL, (unsigned char *)TEST_PASS, TEST_PASS_SIZE, NULL), AssertError,
            "unable to load cipher '(null)'");
        TEST_ERROR(
            cipherBlockNew(
                cipherModeEncrypt, TEST_CIPHER, (unsigned char *)TEST_PASS, TEST_PASS_SIZE, BOGUS_STR), AssertError,
                "unable to load digest 'BOGUS'");

        // Initialization of object
        // -------------------------------------------------------------------------------------------------------------------------
        CipherBlock *cipherBlock = cipherBlockNew(cipherModeEncrypt, TEST_CIPHER, (unsigned char *)TEST_PASS, TEST_PASS_SIZE, NULL);
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
        unsigned char encryptBuffer[TEST_BUFFER_SIZE];
        int encryptSize = 0;
        unsigned char decryptBuffer[TEST_BUFFER_SIZE];
        int decryptSize = 0;

        // Encrypt
        // -------------------------------------------------------------------------------------------------------------------------
        CipherBlock *blockEncrypt = cipherBlockNew(
            cipherModeEncrypt, TEST_CIPHER, (unsigned char *)TEST_PASS, TEST_PASS_SIZE, NULL);

        TEST_RESULT_INT(
            cipherBlockProcessSize(blockEncrypt, strlen(TEST_PLAINTEXT)),
            strlen(TEST_PLAINTEXT) + EVP_MAX_BLOCK_LENGTH + CIPHER_BLOCK_MAGIC_SIZE + PKCS5_SALT_LEN, "check process size");

        encryptSize = cipherBlockProcess(blockEncrypt, (unsigned char *)TEST_PLAINTEXT, strlen(TEST_PLAINTEXT), encryptBuffer);

        TEST_RESULT_BOOL(blockEncrypt->saltDone, true, "salt done is true");
        TEST_RESULT_BOOL(blockEncrypt->processDone, true, "process done is true");
        TEST_RESULT_INT(blockEncrypt->headerSize, 0, "header size is 0");
        TEST_RESULT_INT(encryptSize, CIPHER_BLOCK_HEADER_SIZE, "cipher size is header len");

        TEST_RESULT_INT(
            cipherBlockProcessSize(blockEncrypt, strlen(TEST_PLAINTEXT)),
            strlen(TEST_PLAINTEXT) + EVP_MAX_BLOCK_LENGTH, "check process size");

        encryptSize += cipherBlockProcess(
            blockEncrypt, (unsigned char *)TEST_PLAINTEXT, strlen(TEST_PLAINTEXT), encryptBuffer + encryptSize);
        TEST_RESULT_INT(
            encryptSize, CIPHER_BLOCK_HEADER_SIZE + EVP_CIPHER_block_size(blockEncrypt->cipher),
            "cipher size increases by one block");

        encryptSize += cipherBlockFlush(blockEncrypt, encryptBuffer + encryptSize);
        TEST_RESULT_INT(
            encryptSize, CIPHER_BLOCK_HEADER_SIZE + (EVP_CIPHER_block_size(blockEncrypt->cipher) * 2),
            "cipher size increases by one block on flush");

        cipherBlockFree(blockEncrypt);

        // Decrypt in one pass
        // -------------------------------------------------------------------------------------------------------------------------
        CipherBlock *blockDecrypt = cipherBlockNew(
            cipherModeDecrypt, TEST_CIPHER, (unsigned char *)TEST_PASS, TEST_PASS_SIZE, NULL);

        TEST_RESULT_INT(
            cipherBlockProcessSize(blockDecrypt, encryptSize),
            encryptSize + EVP_MAX_BLOCK_LENGTH, "check process size");

        decryptSize = cipherBlockProcess(blockDecrypt, encryptBuffer, encryptSize, decryptBuffer);
        TEST_RESULT_INT(decryptSize, EVP_CIPHER_block_size(blockDecrypt->cipher), "decrypt size is one block");

        decryptSize += cipherBlockFlush(blockDecrypt, decryptBuffer + decryptSize);
        TEST_RESULT_INT(decryptSize, strlen(TEST_PLAINTEXT) * 2, "check final decrypt size");

        decryptBuffer[decryptSize] = 0;
        TEST_RESULT_STR(decryptBuffer, (TEST_PLAINTEXT TEST_PLAINTEXT), "check final decrypt buffer");

        cipherBlockFree(blockDecrypt);

        // Decrypt in small chunks to test buffering
        // -------------------------------------------------------------------------------------------------------------------------
        blockDecrypt = cipherBlockNew(cipherModeDecrypt, TEST_CIPHER, (unsigned char *)TEST_PASS, TEST_PASS_SIZE, NULL);

        decryptSize = 0;
        memset(decryptBuffer, 0, TEST_BUFFER_SIZE);

        decryptSize = cipherBlockProcess(blockDecrypt, encryptBuffer, CIPHER_BLOCK_MAGIC_SIZE, decryptBuffer);
        TEST_RESULT_INT(decryptSize, 0, "no decrypt since header read is not complete");
        TEST_RESULT_BOOL(blockDecrypt->saltDone, false, "salt done is false");
        TEST_RESULT_BOOL(blockDecrypt->processDone, false, "process done is false");
        TEST_RESULT_INT(blockDecrypt->headerSize, CIPHER_BLOCK_MAGIC_SIZE, "check header size");
        TEST_RESULT_BOOL(
            memcmp(blockDecrypt->header, CIPHER_BLOCK_MAGIC, CIPHER_BLOCK_MAGIC_SIZE) == 0, true, "check header magic");

        decryptSize += cipherBlockProcess(
            blockDecrypt, encryptBuffer + CIPHER_BLOCK_MAGIC_SIZE, PKCS5_SALT_LEN, decryptBuffer + decryptSize);
        TEST_RESULT_INT(decryptSize, 0, "no decrypt since no data processed yet");
        TEST_RESULT_BOOL(blockDecrypt->saltDone, true, "salt done is true");
        TEST_RESULT_BOOL(blockDecrypt->processDone, false, "process done is false");
        TEST_RESULT_INT(blockDecrypt->headerSize, CIPHER_BLOCK_MAGIC_SIZE, "check header size (not increased)");
        TEST_RESULT_BOOL(
            memcmp(
                blockDecrypt->header + CIPHER_BLOCK_MAGIC_SIZE, encryptBuffer + CIPHER_BLOCK_MAGIC_SIZE,
                PKCS5_SALT_LEN) == 0,
            true, "check header salt");

        decryptSize += cipherBlockProcess(
            blockDecrypt, encryptBuffer + CIPHER_BLOCK_HEADER_SIZE, encryptSize - CIPHER_BLOCK_HEADER_SIZE,
            decryptBuffer + decryptSize);
        TEST_RESULT_INT(decryptSize, EVP_CIPHER_block_size(blockDecrypt->cipher), "decrypt size is one block");

        decryptSize += cipherBlockFlush(blockDecrypt, decryptBuffer + decryptSize);
        TEST_RESULT_INT(decryptSize, strlen(TEST_PLAINTEXT) * 2, "check final decrypt size");

        decryptBuffer[decryptSize] = 0;
        TEST_RESULT_STR(decryptBuffer, (TEST_PLAINTEXT TEST_PLAINTEXT), "check final decrypt buffer");

        cipherBlockFree(blockDecrypt);

        // Encrypt zero byte file and decrypt it
        // -------------------------------------------------------------------------------------------------------------------------
        blockEncrypt = cipherBlockNew(cipherModeEncrypt, TEST_CIPHER, (unsigned char *)TEST_PASS, TEST_PASS_SIZE, NULL);

        TEST_RESULT_INT(cipherBlockProcess(blockEncrypt, NULL, 0, encryptBuffer), 16, "process header");
        TEST_RESULT_INT(cipherBlockFlush(blockEncrypt, encryptBuffer + 16), 16, "flush remaining bytes");

        cipherBlockFree(blockEncrypt);

        blockDecrypt = cipherBlockNew(cipherModeDecrypt, TEST_CIPHER, (unsigned char *)TEST_PASS, TEST_PASS_SIZE, NULL);

        TEST_RESULT_INT(cipherBlockProcess(blockDecrypt, encryptBuffer, 32, decryptBuffer), 0, "0 bytes processed");
        TEST_RESULT_INT(cipherBlockFlush(blockDecrypt, decryptBuffer), 0, "0 bytes on flush");

        cipherBlockFree(blockDecrypt);

        // Invalid cipher header
        // -------------------------------------------------------------------------------------------------------------------------
        blockDecrypt = cipherBlockNew(cipherModeDecrypt, TEST_CIPHER, (unsigned char *)TEST_PASS, TEST_PASS_SIZE, NULL);

        TEST_ERROR(
            cipherBlockProcess(
                blockDecrypt, (unsigned char *)"1234567890123456", 16, decryptBuffer), CipherError, "cipher header invalid");

        cipherBlockFree(blockDecrypt);

        // Invalid encrypted data cannot be flushed
        // -------------------------------------------------------------------------------------------------------------------------
        blockDecrypt = cipherBlockNew(cipherModeDecrypt, TEST_CIPHER, (unsigned char *)TEST_PASS, TEST_PASS_SIZE, NULL);

        TEST_RESULT_INT(
            cipherBlockProcess(
                blockDecrypt, (unsigned char *)(CIPHER_BLOCK_MAGIC "12345678"), 16, decryptBuffer), 0, "process header");
        TEST_RESULT_INT(
            cipherBlockProcess(
                blockDecrypt, (unsigned char *)"1234567890123456", 16, decryptBuffer), 0, "process 0 bytes");

        TEST_ERROR(cipherBlockFlush(blockDecrypt, decryptBuffer), CipherError, "unable to flush");

        cipherBlockFree(blockDecrypt);

        // File with no header should not flush
        // -------------------------------------------------------------------------------------------------------------------------
        blockDecrypt = cipherBlockNew(cipherModeDecrypt, TEST_CIPHER, (unsigned char *)TEST_PASS, TEST_PASS_SIZE, NULL);

        TEST_RESULT_INT(cipherBlockProcess(blockDecrypt, NULL, 0, decryptBuffer), 0, "no header processed");
        TEST_ERROR(cipherBlockFlush(blockDecrypt, decryptBuffer), CipherError, "cipher header missing");

        cipherBlockFree(blockDecrypt);

        // File with header only should error
        // -------------------------------------------------------------------------------------------------------------------------
        blockDecrypt = cipherBlockNew(cipherModeDecrypt, TEST_CIPHER, (unsigned char *)TEST_PASS, TEST_PASS_SIZE, NULL);

        TEST_RESULT_INT(
            cipherBlockProcess(
                blockDecrypt, (unsigned char *)(CIPHER_BLOCK_MAGIC "12345678"), 16, decryptBuffer), 0, "0 bytes processed");
        TEST_ERROR(cipherBlockFlush(blockDecrypt, decryptBuffer), CipherError, "unable to flush");

        cipherBlockFree(blockDecrypt);
    }

    cipherFree();
}
