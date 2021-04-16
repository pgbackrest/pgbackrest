/***********************************************************************************************************************************
Cryptographic Hash
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/hmac.h>

#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/io/filter/filter.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"
#include "common/crypto/common.h"

/***********************************************************************************************************************************
Filter type constant
***********************************************************************************************************************************/
STRING_EXTERN(CRYPTO_HASH_FILTER_TYPE_STR,                          CRYPTO_HASH_FILTER_TYPE);

/***********************************************************************************************************************************
Hash types
***********************************************************************************************************************************/
STRING_EXTERN(HASH_TYPE_MD5_STR,                                    HASH_TYPE_MD5);
STRING_EXTERN(HASH_TYPE_SHA1_STR,                                   HASH_TYPE_SHA1);
STRING_EXTERN(HASH_TYPE_SHA256_STR,                                 HASH_TYPE_SHA256);

/***********************************************************************************************************************************
Hashes for zero-length files (i.e., seed value)
***********************************************************************************************************************************/
STRING_EXTERN(HASH_TYPE_SHA1_ZERO_STR,                              HASH_TYPE_SHA1_ZERO);
STRING_EXTERN(HASH_TYPE_SHA256_ZERO_STR,                            HASH_TYPE_SHA256_ZERO);

/***********************************************************************************************************************************
Include local MD5 code
***********************************************************************************************************************************/
#include "common/crypto/md5.vendor.c"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct CryptoHash
{
    MemContext *memContext;                                         // Context to store data
    const EVP_MD *hashType;                                         // Hash type (sha1, md5, etc.)
    EVP_MD_CTX *hashContext;                                        // Message hash context
    MD5_CTX *md5Context;                                            // MD5 context (used to bypass FIPS restrictions)
    Buffer *hash;                                                   // Hash in binary form
} CryptoHash;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_CRYPTO_HASH_TYPE                                                                                              \
    CryptoHash *
#define FUNCTION_LOG_CRYPTO_HASH_FORMAT(value, buffer, bufferSize)                                                                 \
    objToLog(value, "CryptoHash", buffer, bufferSize)

/***********************************************************************************************************************************
Free hash context
***********************************************************************************************************************************/
static void
cryptoHashFreeResource(THIS_VOID)
{
    THIS(CryptoHash);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(CRYPTO_HASH, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    EVP_MD_CTX_destroy(this->hashContext);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Add message data to the hash from a Buffer
***********************************************************************************************************************************/
static void
cryptoHashProcess(THIS_VOID, const Buffer *message)
{
    THIS(CryptoHash);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(CRYPTO_HASH, this);
        FUNCTION_LOG_PARAM(BUFFER, message);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->hash == NULL);
    ASSERT(message != NULL);

    // Standard OpenSSL implementation
    if (this->hashContext != NULL)
    {
        cryptoError(!EVP_DigestUpdate(this->hashContext, bufPtrConst(message), bufUsed(message)), "unable to process message hash");
    }
    // Else local MD5 implementation
    else
        MD5_Update(this->md5Context, bufPtrConst(message), bufUsed(message));

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Get binary representation of the hash
***********************************************************************************************************************************/
static const Buffer *
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
            // Standard OpenSSL implementation
            if (this->hashContext != NULL)
            {
                this->hash = bufNew((size_t)EVP_MD_size(this->hashType));
                cryptoError(!EVP_DigestFinal_ex(this->hashContext, bufPtr(this->hash), NULL), "unable to finalize message hash");
            }
            // Else local MD5 implementation
            else
            {
                this->hash = bufNew(HASH_TYPE_M5_SIZE);
                MD5_Final(bufPtr(this->hash), this->md5Context);
            }

            bufUsedSet(this->hash, bufSize(this->hash));
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_LOG_RETURN(BUFFER, this->hash);
}

/***********************************************************************************************************************************
Get string representation of the hash as a filter result
***********************************************************************************************************************************/
static Variant *
cryptoHashResult(THIS_VOID)
{
    THIS(CryptoHash);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(CRYPTO_HASH, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(VARIANT, varNewStr(bufHex(cryptoHash(this))));
}

/**********************************************************************************************************************************/
IoFilter *
cryptoHashNew(const String *type)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, type);
    FUNCTION_LOG_END();

    ASSERT(type != NULL);

    // Init crypto subsystem
    cryptoInit();

    // Allocate memory to hold process state
    IoFilter *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("CryptoHash")
    {
        CryptoHash *driver = memNew(sizeof(CryptoHash));

        *driver = (CryptoHash)
        {
            .memContext = MEM_CONTEXT_NEW(),
        };

        // Use local MD5 implementation since FIPS-enabled systems do not allow MD5. This is a bit misguided since there are valid
        // cases for using MD5 which do not involve, for example, password hashes. Since popular object stores, e.g. S3, require
        // MD5 for verifying payload integrity we are simply forced to provide MD5 functionality.
        if (strEq(type, HASH_TYPE_MD5_STR))
        {
            driver->md5Context = memNew(sizeof(MD5_CTX));

            MD5_Init(driver->md5Context);
        }
        // Else use the standard OpenSSL implementation
        else
        {
            // Lookup digest
            if ((driver->hashType = EVP_get_digestbyname(strZ(type))) == NULL)
                THROW_FMT(AssertError, "unable to load hash '%s'", strZ(type));

            // Create context
            cryptoError((driver->hashContext = EVP_MD_CTX_create()) == NULL, "unable to create hash context");

            // Set free callback to ensure hash context is freed
            memContextCallbackSet(driver->memContext, cryptoHashFreeResource, driver);

            // Initialize context
            cryptoError(!EVP_DigestInit_ex(driver->hashContext, driver->hashType, NULL), "unable to initialize hash context");
        }

        // Create param list
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStr(type));

        // Create filter interface
        this = ioFilterNewP(CRYPTO_HASH_FILTER_TYPE_STR, driver, paramList, .in = cryptoHashProcess, .result = cryptoHashResult);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_FILTER, this);
}

IoFilter *
cryptoHashNewVar(const VariantList *paramList)
{
    return cryptoHashNew(varStr(varLstGet(paramList, 0)));
}

/**********************************************************************************************************************************/
Buffer *
cryptoHashOne(const String *type, const Buffer *message)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, type);
        FUNCTION_LOG_PARAM(BUFFER, message);
    FUNCTION_LOG_END();

    ASSERT(type != NULL);
    ASSERT(message != NULL);

    Buffer *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        IoFilter *hash = cryptoHashNew(type);

        if (!bufEmpty(message))
            ioFilterProcessIn(hash, message);

        const Buffer *buffer = cryptoHash((CryptoHash *)ioFilterDriver(hash));

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = bufDup(buffer);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BUFFER, result);
}

/**********************************************************************************************************************************/
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

    // Init crypto subsystem
    cryptoInit();

    const EVP_MD *hashType = EVP_get_digestbyname(strZ(type));
    ASSERT(hashType != NULL);

    // Allocate a buffer to hold the hmac
    Buffer *result = bufNew((size_t)EVP_MD_size(hashType));
    bufUsedSet(result, bufSize(result));

    // Calculate the HMAC
    HMAC(hashType, bufPtrConst(key), (int)bufUsed(key), bufPtrConst(message), bufUsed(message), bufPtr(result), NULL);

    FUNCTION_LOG_RETURN(BUFFER, result);
}
