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
            cryptoError(EVP_DigestInit_ex(context, NULL, NULL) != 1, "unable to initialize hash context"), CryptoError,
            "unable to initialize hash context: no digest set");
        EVP_MD_CTX_destroy(context);

        TEST_ERROR(cryptoError(true, "no error"), CryptoError, "no error: no details available");
    }

    // *****************************************************************************************************************************
    if (testBegin("cryptoRandomBytes()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        // Test if the buffer was overrun
        size_t bufferSize = 256;
        unsigned char *buffer = memNew(bufferSize + 1);

        cryptoRandomBytes(buffer, bufferSize);
        TEST_RESULT_BOOL(buffer[bufferSize] == 0, true, "check that buffer did not overrun (though random byte could be 0)");

        // -------------------------------------------------------------------------------------------------------------------------
        // Count bytes that are not zero (there shouldn't be all zeroes)
        int nonZeroTotal = 0;

        for (unsigned int charIdx = 0; charIdx < bufferSize; charIdx++)
            if (buffer[charIdx] != 0)                               // {uncovered - ok if there are no zeros}
                nonZeroTotal++;

        TEST_RESULT_INT_NE(nonZeroTotal, 0, "check that there are non-zero values in the buffer");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
