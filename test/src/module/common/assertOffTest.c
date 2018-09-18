/***********************************************************************************************************************************
Test Assert Macros and Routines when Disabled
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("ASSERT()"))
    {
        TEST_RESULT_VOID(ASSERT(true), "assert true");
        TEST_ERROR(ASSERT(false || false), AssertError, "assertion 'false || false' failed");
    }

    // *****************************************************************************************************************************
    if (testBegin("ASSERT_DEBUG()"))
    {
        TEST_RESULT_VOID(ASSERT_DEBUG(true), "assert true ignored");
        TEST_RESULT_VOID(ASSERT_DEBUG(false || false), "assert false ignored");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
