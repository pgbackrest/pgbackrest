/***********************************************************************************************************************************
Test Random
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    FUNCTION_HARNESS_VOID();

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("randomBytes()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        // Test if the buffer was overrun
        size_t bufferSize = 256;
        unsigned char *buffer = memNew(bufferSize + 1);

        randomBytes(buffer, bufferSize);
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
