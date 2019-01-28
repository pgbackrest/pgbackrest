/***********************************************************************************************************************************
Cryptographic Hash
***********************************************************************************************************************************/
#include <string.h>

#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/hmac.h>

#include "common/debug.h"
#include "common/io/filter/filter.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "crypto/crypto.h"
#include "crypto/hash.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
#define CRYPTO_HASH_FILTER_TYPE                                     "hash"
    STRING_STATIC(CRYPTO_HASH_FILTER_TYPE_STR,                      CRYPTO_HASH_FILTER_TYPE);

/***********************************************************************************************************************************
Hash types
***********************************************************************************************************************************/
STRING_EXTERN(HASH_TYPE_MD5_STR,                                    HASH_TYPE_MD5);
STRING_EXTERN(HASH_TYPE_SHA1_STR,                                   HASH_TYPE_SHA1);
STRING_EXTERN(HASH_TYPE_SHA256_STR,                                 HASH_TYPE_SHA256);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct CryptoHash
{
    MemContext *memContext;                                         // Context to store data
    const EVP_MD *hashType;                                         // Hash type (sha1, md5, etc.)
    EVP_MD_CTX *hashContext;                                        // Message hash context
    Buffer *hash;                                                   // Hash in binary form
    IoFilter *filter;                                               // Filter interface
};

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
CryptoHash *
cryptoHashNew(const String *type)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, type);
    FUNCTION_LOG_END();

    ASSERT(type != NULL);

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

        // Create context
        cryptoError((this->hashContext = EVP_MD_CTX_create()) == NULL, "unable to create hash context");

        // Set free callback to ensure hash context is freed
        memContextCallback(this->memContext, (MemContextCallback)cryptoHashFree, this);

        // Initialize context
        cryptoError(!EVP_DigestInit_ex(this->hashContext, this->hashType, NULL), "unable to initialize hash context");

        // Create filter interface
        this->filter = ioFilterNewP(
            CRYPTO_HASH_FILTER_TYPE_STR, this, .in = (IoFilterInterfaceProcessIn)cryptoHashProcess,
            .result = (IoFilterInterfaceResult)cryptoHashResult);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(CRYPTO_HASH, this);
}

/***********************************************************************************************************************************
Add message data to the hash
***********************************************************************************************************************************/
void
cryptoHashProcessC(CryptoHash *this, const unsigned char *message, size_t messageSize)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(CRYPTO_HASH, this);
        FUNCTION_LOG_PARAM_P(UCHARDATA, message);
        FUNCTION_LOG_PARAM(SIZE, messageSize);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->hashContext != NULL);
    ASSERT(message != NULL);

    cryptoError(!EVP_DigestUpdate(this->hashContext, message, messageSize), "unable to process message hash");

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Add message data to the hash from a Buffer
***********************************************************************************************************************************/
void
cryptoHashProcess(CryptoHash *this, const Buffer *message)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CRYPTO_HASH, this);
        FUNCTION_TEST_PARAM(BUFFER, message);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(message != NULL);

    cryptoHashProcessC(this, bufPtr(message), bufUsed(message));

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Add message data to the hash from a String
***********************************************************************************************************************************/
void
cryptoHashProcessStr(CryptoHash *this, const String *message)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CRYPTO_HASH, this);
        FUNCTION_TEST_PARAM(STRING, message);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(message != NULL);

    cryptoHashProcessC(this, (const unsigned char *)strPtr(message), strSize(message));

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Get binary representation of the hash
***********************************************************************************************************************************/
const Buffer *
cryptoHash(CryptoHash *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(CRYPTO_HASH, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    if (this->hash == NULL)
    {
        MEM_CONTEXT_BEGIN(this->memContext)
        {
            this->hash = bufNew((size_t)EVP_MD_size(this->hashType));

            cryptoError(!EVP_DigestFinal_ex(this->hashContext, bufPtr(this->hash), NULL), "unable to finalize message hash");

            // Free the context
            EVP_MD_CTX_destroy(this->hashContext);
            this->hashContext = NULL;
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_LOG_RETURN(BUFFER, this->hash);
}

/***********************************************************************************************************************************
Get filter interface
***********************************************************************************************************************************/
IoFilter *
cryptoHashFilter(CryptoHash *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(CRYPTO_HASH, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(IO_FILTER, this->filter);
}

/***********************************************************************************************************************************
Get string representation of the hash as a filter result
***********************************************************************************************************************************/
const Variant *
cryptoHashResult(CryptoHash *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(CRYPTO_HASH, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    Variant *result = NULL;

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        result = varNewStr(bufHex(cryptoHash(this)));
    }
    MEM_CONTEXT_END();

    FUNCTION_LOG_RETURN(VARIANT, result);
}

/***********************************************************************************************************************************
Free memory
***********************************************************************************************************************************/
void
cryptoHashFree(CryptoHash *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(CRYPTO_HASH, this);
    FUNCTION_LOG_END();

    if (this != NULL)
    {
        EVP_MD_CTX_destroy(this->hashContext);

        memContextCallbackClear(this->memContext);
        memContextFree(this->memContext);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Get hash for one C buffer
***********************************************************************************************************************************/
Buffer *
cryptoHashOneC(const String *type, const unsigned char *message, size_t messageSize)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, type);
        FUNCTION_LOG_PARAM_P(UCHARDATA, message);
    FUNCTION_LOG_END();

    ASSERT(type != NULL);
    ASSERT(message != NULL);

    Buffer *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        CryptoHash *hash = cryptoHashNew(type);
        cryptoHashProcessC(hash, message, messageSize);

        memContextSwitch(MEM_CONTEXT_OLD());
        result = bufNewC(bufSize(cryptoHash(hash)), bufPtr(cryptoHash(hash)));
        memContextSwitch(MEM_CONTEXT_TEMP());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BUFFER, result);
}

/***********************************************************************************************************************************
Get hash for one Buffer
***********************************************************************************************************************************/
Buffer *
cryptoHashOne(const String *type, Buffer *message)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, type);
        FUNCTION_TEST_PARAM(BUFFER, message);
    FUNCTION_TEST_END();

    ASSERT(type != NULL);
    ASSERT(message != NULL);

    FUNCTION_TEST_RETURN(cryptoHashOneC(type, bufPtr(message), bufSize(message)));
}

/***********************************************************************************************************************************
Get hash for one String
***********************************************************************************************************************************/
Buffer *
cryptoHashOneStr(const String *type, String *message)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, type);
        FUNCTION_TEST_PARAM(STRING, message);
    FUNCTION_TEST_END();

    ASSERT(type != NULL);
    ASSERT(message != NULL);

    FUNCTION_TEST_RETURN(cryptoHashOneC(type, (const unsigned char *)strPtr(message), strSize(message)));
}

/***********************************************************************************************************************************
Get hmac for one message/key
***********************************************************************************************************************************/
Buffer *
cryptoHmacOne(const String *type, const Buffer *key, const Buffer *message)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, type);
        FUNCTION_LOG_PARAM(BUFFER, key);
        FUNCTION_LOG_PARAM(BUFFER, message);
    FUNCTION_LOG_END();

    ASSERT(type != NULL);
    ASSERT(key != NULL);
    ASSERT(message != NULL);

    const EVP_MD *hashType = EVP_get_digestbyname(strPtr(type));
    ASSERT(hashType != NULL);

    // Allocate a buffer to hold the hmac
    Buffer *result = bufNew((size_t)EVP_MD_size(hashType));

    // Calculate the HMAC
    HMAC(hashType, bufPtr(key), (int)bufSize(key), bufPtr(message), bufSize(message), bufPtr(result), NULL);

    FUNCTION_TEST_RETURN(result);
}
