/***********************************************************************************************************************************
Test Regular Expression Handler
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("regExpNew(), regExpMatch(), and regExpFree()"))
    {
        TEST_ERROR(regExpNew(strNew("[[[")), FormatError, "Unmatched [ or [^");

        RegExp *regExp = NULL;
        TEST_ASSIGN(regExp, regExpNew(strNew("^abc")), "new regexp");
        TEST_RESULT_BOOL(regExpMatch(regExp, strNew("abcdef")), true, "match regexp");
        TEST_RESULT_BOOL(regExpMatch(regExp, strNew("bcdef")), false, "no match regexp");

        TEST_RESULT_VOID(regExpFree(regExp), "free regexp");
        TEST_RESULT_VOID(regExpFree(NULL), "free NULL regexp");
    }

    // *****************************************************************************************************************************
    if (testBegin("regExpMatchOne()"))
    {
        TEST_ERROR(regExpMatchOne(strNew("[[["), strNew("")), FormatError, "Unmatched [ or [^");
        TEST_RESULT_BOOL(regExpMatchOne(strNew("^abc"), strNew("abcdef")), true, "match regexp");
        TEST_RESULT_BOOL(regExpMatchOne(strNew("^abc"), strNew("bcdef")), false, "no match regexp");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
