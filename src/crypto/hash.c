/***********************************************************************************************************************************
Cryptographic Hashes
***********************************************************************************************************************************/
#include <string.h>

#include <openssl/evp.h>
#include <openssl/err.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "crypto/crypto.h"
#include "crypto/hash.h"

/***********************************************************************************************************************************
Track state during block encrypt/decrypt
***********************************************************************************************************************************/
struct CryptoHash
{
    MemContext *memContext;                                         // Context to store data
    const EVP_MD *hashType;                                         // Hash type
    EVP_MD_CTX *hashContext;                                        // Message hash context
    Buffer *hash;                                                   // Hash in binary form
};

/***********************************************************************************************************************************
New CryptoHash object
***********************************************************************************************************************************/
CryptoHash *
cryptoHashNew(const String *type)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STRING, type);

        FUNCTION_DEBUG_ASSERT(type != NULL);
    FUNCTION_DEBUG_END();

    // Only need to init once
    if (!cryptoIsInit())
        cryptoInit();

    // Allocate memory to hold process state
    CryptoHash *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("CryptoHash")
    {
        // Allocate state and set context
        this = memNew(sizeof(CryptoHash));
        this->memContext = MEM_CONTEXT_NEW();

        // Lookup digest
        if ((this->hashType = EVP_get_digestbyname(strPtr(type))) == NULL)
            THROW_FMT(AssertError, "unable to load hash '%s'", strPtr(type));

        // Create and initialize the context
        if ((this->hashContext = EVP_MD_CTX_create()) == NULL)                      // {uncoverable - no failure condition known}
            THROW(MemoryError, "unable to create hash context");                    // {+uncoverable}

        if (EVP_DigestInit_ex(this->hashContext, this->hashType, NULL) != 1)        // {uncoverable - no failure condition known}
            THROW(MemoryError, "unable to initialize hash context");                // {+uncoverable}

        // Set free callback to ensure hash context is freed
        memContextCallback(this->memContext, (MemContextCallback)cryptoHashFree, this);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_DEBUG_RESULT(CRYPTO_HASH, this);
}

/***********************************************************************************************************************************
Add message data to the hash
***********************************************************************************************************************************/
void
cryptoHashProcessC(CryptoHash *this, const unsigned char *message, size_t messageSize)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(CRYPTO_HASH, this);
        FUNCTION_DEBUG_PARAM(UCHARP, message);
        FUNCTION_DEBUG_PARAM(SIZE, messageSize);

        FUNCTION_DEBUG_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(this->hashContext != NULL);
        FUNCTION_DEBUG_ASSERT(message != NULL);
    FUNCTION_DEBUG_END();

    if (EVP_DigestUpdate(                                                           // {uncoverable - no failure condition known}
            this->hashContext, message, messageSize) != 1)
    {
        THROW(MemoryError, "unable to process message hash");                       // {+uncoverable}
    }

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Add message data to the hash from a Buffer
***********************************************************************************************************************************/
void
cryptoHashProcess(CryptoHash *this, Buffer *message)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CRYPTO_HASH, this);
        FUNCTION_TEST_PARAM(BUFFER, message);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(message != NULL);
    FUNCTION_TEST_END();

    cryptoHashProcessC(this, bufPtr(message), bufSize(message));

    FUNCTION_TEST_RESULT_VOID();
}

/***********************************************************************************************************************************
Add message data to the hash from a String
***********************************************************************************************************************************/
void
cryptoHashProcessStr(CryptoHash *this, String *message)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CRYPTO_HASH, this);
        FUNCTION_TEST_PARAM(STRING, message);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(message != NULL);
    FUNCTION_TEST_END();

    cryptoHashProcessC(this, (const unsigned char *)strPtr(message), strSize(message));

    FUNCTION_TEST_RESULT_VOID();
}

/***********************************************************************************************************************************
Get binary representation of the hash
***********************************************************************************************************************************/
const Buffer *
cryptoHash(CryptoHash *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(CRYPTO_HASH, this);

        FUNCTION_DEBUG_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    if (this->hash == NULL)
    {
        MEM_CONTEXT_BEGIN(this->memContext)
        {
            this->hash = bufNew((size_t)EVP_MD_size(this->hashType));
            unsigned int hashSize;

            if (EVP_DigestFinal_ex(                                                 // {uncoverable - no failure condition known}
                this->hashContext, bufPtr(this->hash), &hashSize) != 1)
            {
                THROW(MemoryError, "unable to finalize message hash");              // {+uncoverable}
            }

            // Free the context
            EVP_MD_CTX_destroy(this->hashContext);
            this->hashContext = NULL;
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_DEBUG_RESULT(BUFFER, this->hash);
}

/***********************************************************************************************************************************
Get string representation of the hash
***********************************************************************************************************************************/
String *
cryptoHashHex(CryptoHash *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(CRYPTO_HASH, this);

        FUNCTION_DEBUG_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    const Buffer *hash = cryptoHash(this);
    String *hashStr = strNew("");

    for (unsigned int hashIdx = 0; hashIdx < bufSize(hash); hashIdx++)
        strCatFmt(hashStr, "%02x", bufPtr(hash)[hashIdx]);

    FUNCTION_DEBUG_RESULT(STRING, hashStr);
}

/***********************************************************************************************************************************
Free memory
***********************************************************************************************************************************/
void
cryptoHashFree(CryptoHash *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(CRYPTO_HASH, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
    {
        if (this->hashContext != NULL)
        {
            EVP_MD_CTX_destroy(this->hashContext);
            this->hashContext = NULL;
        }

        memContextFree(this->memContext);
    }

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Get hash for one C buffer
***********************************************************************************************************************************/
String *
cryptoHashOneC(const String *type, const unsigned char *message, size_t messageSize)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STRING, type);
        FUNCTION_DEBUG_PARAM(UCHARP, message);

        FUNCTION_DEBUG_ASSERT(type != NULL);
        FUNCTION_DEBUG_ASSERT(message != NULL);
    FUNCTION_DEBUG_END();

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        CryptoHash *hash = cryptoHashNew(type);
        cryptoHashProcessC(hash, message, messageSize);

        memContextSwitch(MEM_CONTEXT_OLD());
        result = cryptoHashHex(hash);
        memContextSwitch(MEM_CONTEXT_TEMP());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(STRING, result);
}

/***********************************************************************************************************************************
Get hash for one Buffer
***********************************************************************************************************************************/
String *
cryptoHashOne(const String *type, Buffer *message)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, type);
        FUNCTION_TEST_PARAM(BUFFER, message);

        FUNCTION_TEST_ASSERT(type != NULL);
        FUNCTION_TEST_ASSERT(message != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(STRING, cryptoHashOneC(type, bufPtr(message), bufSize(message)));
}

/***********************************************************************************************************************************
Get hash for one String
***********************************************************************************************************************************/
String *
cryptoHashOneStr(const String *type, String *message)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, type);
        FUNCTION_TEST_PARAM(STRING, message);

        FUNCTION_TEST_ASSERT(type != NULL);
        FUNCTION_TEST_ASSERT(message != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(STRING, cryptoHashOneC(type, (const unsigned char *)strPtr(message), strSize(message)));
}
