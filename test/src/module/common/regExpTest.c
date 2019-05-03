/***********************************************************************************************************************************
Test Regular Expression Handler
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
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
    }

    // *****************************************************************************************************************************
    if (testBegin("regExpPrefix()"))
    {
        TEST_RESULT_PTR(regExpPrefix(NULL), NULL, "null expression has no prefix");
        TEST_RESULT_PTR(regExpPrefix(strNew("")), NULL, "empty expression has no prefix");
        TEST_RESULT_PTR(regExpPrefix(strNew("ABC")), NULL, "expression without begin anchor has no prefix");
        TEST_RESULT_PTR(regExpPrefix(strNew("^.")), NULL, "expression with no regular character has no prefix");

        TEST_RESULT_STR(strPtr(regExpPrefix(strNew("^ABC^"))), "ABC", "prefix stops at special character");
        TEST_RESULT_STR(strPtr(regExpPrefix(strNew("^ABC$"))), "ABC", "prefix stops at special character");
        TEST_RESULT_STR(strPtr(regExpPrefix(strNew("^ABC*"))), "ABC", "prefix stops at special character");
        TEST_RESULT_STR(strPtr(regExpPrefix(strNew("^ABC+"))), "ABC", "prefix stops at special character");
        TEST_RESULT_STR(strPtr(regExpPrefix(strNew("^ABC-"))), "ABC", "prefix stops at special character");
        TEST_RESULT_STR(strPtr(regExpPrefix(strNew("^ABC?"))), "ABC", "prefix stops at special character");
        TEST_RESULT_STR(strPtr(regExpPrefix(strNew("^ABC("))), "ABC", "prefix stops at special character");
        TEST_RESULT_STR(strPtr(regExpPrefix(strNew("^ABC["))), "ABC", "prefix stops at special character");
        TEST_RESULT_STR(strPtr(regExpPrefix(strNew("^ABC{"))), "ABC", "prefix stops at special character");
        TEST_RESULT_STR(strPtr(regExpPrefix(strNew("^ABC "))), "ABC", "prefix stops at special character");
        TEST_RESULT_STR(strPtr(regExpPrefix(strNew("^ABC|"))), "ABC", "prefix stops at special character");
        TEST_RESULT_STR(strPtr(regExpPrefix(strNew("^ABC\\"))), "ABC", "prefix stops at special character");

        TEST_RESULT_STR(strPtr(regExpPrefix(strNew("^ABCDEF"))), "ABCDEF", "prefix is entire expression");
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
