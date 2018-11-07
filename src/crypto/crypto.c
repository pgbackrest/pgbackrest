/***********************************************************************************************************************************
Crypto Common
***********************************************************************************************************************************/
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>

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
    {
        const char *errorMessage = ERR_reason_error_string(ERR_get_error());
        THROW_FMT(CryptoError, "%s: %s", description, errorMessage == NULL ? "no details available" : errorMessage);
    }

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
