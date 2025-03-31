/***********************************************************************************************************************************
Cryptographic Hash
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include "common/crypto/common.h"
#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/io/filter/filter.h"
#include "common/log.h"
#include "common/type/object.h"
#include "common/type/pack.h"

/***********************************************************************************************************************************
Hashes for zero-length files (i.e., seed value)
***********************************************************************************************************************************/
BUFFER_EXTERN(
    HASH_TYPE_SHA1_ZERO_BUF, 0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55, 0xbf, 0xef, 0x95, 0x60, 0x18, 0x90, 0xaf,
    0xd8, 0x07, 0x09);
BUFFER_EXTERN(
    HASH_TYPE_SHA256_ZERO_BUF, 0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24, 0x27,
    0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55);

/***********************************************************************************************************************************
Include local MD5 code
***********************************************************************************************************************************/
#include "common/crypto/md5.vendor.c.inc"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct CryptoHash
{
    const EVP_MD *hashType;                                         // Hash type (sha1, md5, etc.)
    EVP_MD_CTX *hashContext;                                        // Message hash context
    MD5_CTX md5Context;                                             // MD5 context (used to bypass FIPS restrictions)
    Buffer *hash;                                                   // Hash in binary form
} CryptoHash;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_CRYPTO_HASH_TYPE                                                                                              \
    CryptoHash *
#define FUNCTION_LOG_CRYPTO_HASH_FORMAT(value, buffer, bufferSize)                                                                 \
    objNameToLog(value, "CryptoHash", buffer, bufferSize)

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
cryptoHashProcess(THIS_VOID, const Buffer *const message)
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
        MD5_Update(&this->md5Context, bufPtrConst(message), bufUsed(message));

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Get binary representation of the hash
***********************************************************************************************************************************/
static const Buffer *
cryptoHash(CryptoHash *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(CRYPTO_HASH, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    if (this->hash == NULL)
    {
        MEM_CONTEXT_OBJ_BEGIN(this)
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
                MD5_Final(bufPtr(this->hash), &this->md5Context);
            }

            bufUsedSet(this->hash, bufSize(this->hash));
        }
        MEM_CONTEXT_OBJ_END();
    }

    FUNCTION_LOG_RETURN(BUFFER, this->hash);
}

/***********************************************************************************************************************************
Get string representation of the hash as a filter result
***********************************************************************************************************************************/
static Pack *
cryptoHashResult(THIS_VOID)
{
    THIS(CryptoHash);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(CRYPTO_HASH, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    Pack *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackWrite *const packWrite = pckWriteNewP();

        pckWriteBinP(packWrite, cryptoHash(this));
        pckWriteEndP(packWrite);

        result = pckMove(pckWriteResult(packWrite), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PACK, result);
}

/**********************************************************************************************************************************/
FN_EXTERN IoFilter *
cryptoHashNew(const HashType type)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING_ID, type);
    FUNCTION_LOG_END();

    ASSERT(type != 0);

    // Init crypto subsystem
    cryptoInit();

    OBJ_NEW_BEGIN(CryptoHash, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        *this = (CryptoHash){0};

        // Use local MD5 implementation since FIPS-enabled systems do not allow MD5. This is a bit misguided since there are valid
        // cases for using MD5 which do not involve, for example, password hashes. Since popular object stores, e.g. S3, require
        // MD5 for verifying payload integrity we are simply forced to provide MD5 functionality.
        if (type == hashTypeMd5)
        {
            MD5_Init(&this->md5Context);
        }
        // Else use the standard OpenSSL implementation
        else
        {
            // Lookup digest
            char typeZ[STRID_MAX + 1];
            strIdToZ(type, typeZ);

            if ((this->hashType = EVP_get_digestbyname(typeZ)) == NULL)
                THROW_FMT(AssertError, "unable to load hash '%s'", typeZ);

            // Create context
            cryptoError((this->hashContext = EVP_MD_CTX_create()) == NULL, "unable to create hash context");

            // Set free callback to ensure hash context is freed
            memContextCallbackSet(objMemContext(this), cryptoHashFreeResource, this);

            // Initialize context
            cryptoError(!EVP_DigestInit_ex(this->hashContext, this->hashType, NULL), "unable to initialize hash context");
        }
    }
    OBJ_NEW_END();

    // Create param list
    Pack *paramList;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackWrite *const packWrite = pckWriteNewP();

        pckWriteStrIdP(packWrite, type);
        pckWriteEndP(packWrite);

        paramList = pckMove(pckWriteResult(packWrite), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(
        IO_FILTER, ioFilterNewP(CRYPTO_HASH_FILTER_TYPE, this, paramList, .in = cryptoHashProcess, .result = cryptoHashResult));
}

FN_EXTERN IoFilter *
cryptoHashNewPack(const Pack *const paramList)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PACK, paramList);
    FUNCTION_TEST_END();

    IoFilter *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = ioFilterMove(cryptoHashNew(pckReadStrIdP(pckReadNew(paramList))), memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(IO_FILTER, result);
}

/**********************************************************************************************************************************/
FN_EXTERN Buffer *
cryptoHashOne(const HashType type, const Buffer *const message)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING_ID, type);
        FUNCTION_LOG_PARAM(BUFFER, message);
    FUNCTION_LOG_END();

    ASSERT(type != 0);
    ASSERT(message != NULL);

    Buffer *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        IoFilter *const hash = cryptoHashNew(type);

        if (!bufEmpty(message))
            ioFilterProcessIn(hash, message);

        const Buffer *const buffer = cryptoHash((CryptoHash *)ioFilterDriver(hash));

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
FN_EXTERN Buffer *
cryptoHmacOne(const HashType type, const Buffer *const key, const Buffer *const message)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING_ID, type);
        FUNCTION_LOG_PARAM(BUFFER, key);
        FUNCTION_LOG_PARAM(BUFFER, message);
    FUNCTION_LOG_END();

    ASSERT(type != 0);
    ASSERT(key != NULL);
    ASSERT(message != NULL);

    // Init crypto subsystem
    cryptoInit();

    // Lookup digest
    char typeZ[STRID_MAX + 1];
    strIdToZ(type, typeZ);

    const EVP_MD *const hashType = EVP_get_digestbyname(typeZ);
    ASSERT(hashType != NULL);

    // Allocate a buffer to hold the hmac
    Buffer *const result = bufNew((size_t)EVP_MD_size(hashType));
    bufUsedSet(result, bufSize(result));

    // Calculate the HMAC
    HMAC(hashType, bufPtrConst(key), (int)bufUsed(key), bufPtrConst(message), bufUsed(message), bufPtr(result), NULL);

    FUNCTION_LOG_RETURN(BUFFER, result);
}
