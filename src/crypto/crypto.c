/***********************************************************************************************************************************
Crypto Common
***********************************************************************************************************************************/
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>

#include "common/debug.h"
#include "common/error.h"
#include "common/log.h"
#include "crypto/crypto.h"

/***********************************************************************************************************************************
Flag to indicate if OpenSSL has already been initialized
***********************************************************************************************************************************/
static bool cryptoInitDone = false;

/***********************************************************************************************************************************
Throw crypto errors
***********************************************************************************************************************************/
void
cryptoError(bool error, const char *description)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BOOL, error);
        FUNCTION_TEST_PARAM(STRINGZ, description);
    FUNCTION_TEST_END();

    if (error)
        cryptoErrorCode(ERR_get_error(), description);

    FUNCTION_TEST_RESULT_VOID();
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

    FUNCTION_TEST_RESULT_VOID();
}

/***********************************************************************************************************************************
Initialize crypto
***********************************************************************************************************************************/
void
cryptoInit(void)
{
    FUNCTION_DEBUG_VOID(logLevelTrace);

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

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Has crypto been initialized?
***********************************************************************************************************************************/
bool
cryptoIsInit(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RESULT(BOOL, cryptoInitDone);
}

/***********************************************************************************************************************************
Generate random bytes
***********************************************************************************************************************************/
void
cryptoRandomBytes(unsigned char *buffer, size_t size)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(UCHARP, buffer);
        FUNCTION_DEBUG_PARAM(SIZE, size);

        FUNCTION_DEBUG_ASSERT(buffer != NULL);
        FUNCTION_DEBUG_ASSERT(size > 0);
    FUNCTION_DEBUG_END();

    RAND_bytes(buffer, (int)size);

    FUNCTION_DEBUG_RESULT_VOID();
}
