/***********************************************************************************************************************************
Test Crypto Common
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("cryptoInit() and cryptoIsInit()"))
    {
        TEST_RESULT_BOOL(cryptoIsInit(), false, "crypto is not initialized");
        TEST_RESULT_VOID(cryptoInit(), "initialize crypto");
        TEST_RESULT_BOOL(cryptoIsInit(), true, "crypto is initialized");
        TEST_RESULT_VOID(cryptoInit(), "initialize crypto again");
    }

    // *****************************************************************************************************************************
    if (testBegin("cryptoError"))
    {
        cryptoInit();

        TEST_RESULT_VOID(cryptoError(false, "no error here"), "no error");

        EVP_MD_CTX *context = EVP_MD_CTX_create();
        TEST_ERROR(
            cryptoError(EVP_DigestInit_ex(context, NULL, NULL) != 1, "unable to initialize hash context"), CipherError,
            "unable to initialize hash context: no digest set");
        EVP_MD_CTX_destroy(context);

        TEST_ERROR(cryptoError(true, "no error"), CipherError, "no error: no details available");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
