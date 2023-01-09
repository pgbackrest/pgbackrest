/***********************************************************************************************************************************
Test Assert Macros and Routines
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("ASSERT(), CHECK*()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("ASSERT()");

        TEST_RESULT_VOID(ASSERT(true), "assert true");
        TEST_ERROR(ASSERT(false || false), AssertError, "assertion 'false || false' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("CHECK*()");

        TEST_RESULT_VOID(CHECK(AssertError, true, "check succeeded"), "check true");
        TEST_RESULT_VOID(CHECK_FMT(AssertError, true, "check %s", "succeeded"), "check true");

        TEST_ERROR(CHECK(AssertError, false || false, "message"), AssertError, "message");
        TEST_ERROR(CHECK_FMT(AssertError, false || false, "%s", "message"), AssertError, "message");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
