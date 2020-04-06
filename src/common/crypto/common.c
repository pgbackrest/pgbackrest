/***********************************************************************************************************************************
Crypto Common
***********************************************************************************************************************************/
#include "build.auto.h"

#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>

#include "common/debug.h"
#include "common/error.h"
#include "common/log.h"
#include "common/crypto/common.h"

/***********************************************************************************************************************************
Cipher types
***********************************************************************************************************************************/
STRING_EXTERN(CIPHER_TYPE_NONE_STR,                                 CIPHER_TYPE_NONE);
STRING_EXTERN(CIPHER_TYPE_AES_256_CBC_STR,                          CIPHER_TYPE_AES_256_CBC);

/***********************************************************************************************************************************
Flag to indicate if OpenSSL has already been initialized
***********************************************************************************************************************************/
static bool cryptoInitDone = false;

/**********************************************************************************************************************************/
void
cryptoError(bool error, const char *description)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BOOL, error);
        FUNCTION_TEST_PARAM(STRINGZ, description);
    FUNCTION_TEST_END();

    if (error)
        cryptoErrorCode(ERR_get_error(), description);

    FUNCTION_TEST_RETURN_VOID();
}

void
cryptoErrorCode(unsigned long code, const char *description)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT64, code);
        FUNCTION_TEST_PARAM(STRINGZ, description);
    FUNCTION_TEST_END();

    const char *errorMessage = ERR_reason_error_string(code);
    THROW_FMT(CryptoError, "%s: [%lu] %s", description, code, errorMessage == NULL ? "no details available" : errorMessage);

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
CipherType
cipherType(const String *name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(name != NULL);

    CipherType result = cipherTypeNone;

    if (strEq(name, CIPHER_TYPE_AES_256_CBC_STR))
        result = cipherTypeAes256Cbc;
    else if (!strEq(name, CIPHER_TYPE_NONE_STR))
        THROW_FMT(AssertError, "invalid cipher name '%s'", strPtr(name));

    FUNCTION_TEST_RETURN(result);
}

const String *
cipherTypeName(CipherType type)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
    FUNCTION_TEST_END();

    const String *result = CIPHER_TYPE_NONE_STR;

    if (type == cipherTypeAes256Cbc)
        result = CIPHER_TYPE_AES_256_CBC_STR;
    else if (type != cipherTypeNone)
        THROW_FMT(AssertError, "invalid cipher type %u", type);

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
void
cryptoInit(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    if (!cryptoInitDone)
    {
        // Load crypto strings and algorithms
        ERR_load_crypto_strings();
        OpenSSL_add_all_algorithms();

        // SSL initialization depends on the version of OpenSSL
#if OPENSSL_VERSION_NUMBER < 0x10100000L
        OPENSSL_config(NULL);
        SSL_library_init();
        SSL_load_error_strings();
#else
        OPENSSL_init_ssl(OPENSSL_INIT_LOAD_CONFIG, NULL);
#endif

        // Mark crypto as initialized
        cryptoInitDone = true;
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
bool
cryptoIsInit(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(cryptoInitDone);
}

/**********************************************************************************************************************************/
void
cryptoRandomBytes(unsigned char *buffer, size_t size)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(UCHARDATA, buffer);
        FUNCTION_LOG_PARAM(SIZE, size);
    FUNCTION_LOG_END();

    ASSERT(buffer != NULL);
    ASSERT(size > 0);

    RAND_bytes(buffer, (int)size);

    FUNCTION_LOG_RETURN_VOID();
}
