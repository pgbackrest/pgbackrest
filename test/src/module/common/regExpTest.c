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
        // Error message varies based on the libc version
        TRY_BEGIN()
        {
            TEST_ERROR(regExpNew(strNew("[[[")), FormatError, "Unmatched [ or [^");
        }
        CATCH(TestError)
        {
            TEST_ERROR(regExpNew(strNew("[[[")), FormatError, "Unmatched [, [^, [:, [., or [=");
        }
        TRY_END();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("new regexp");

        RegExp *regExp = NULL;
        TEST_ASSIGN(regExp, regExpNew(strNew("^abc")), "new regexp");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("regexp match");

        const String *string = STRDEF("abcdef");
        TEST_RESULT_BOOL(regExpMatch(regExp, string), true, "match regexp");
        TEST_RESULT_PTR(regExpMatchPtr(regExp), strPtr(string), "check ptr");
        TEST_RESULT_SIZE(regExpMatchSize(regExp), 3, "check size");
        TEST_RESULT_STR_Z(regExpMatchStr(regExp), "abc", "check str");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no regexp match");

        TEST_RESULT_BOOL(regExpMatch(regExp, strNew("bcdef")), false, "no match regexp");
        TEST_RESULT_PTR(regExpMatchPtr(regExp), NULL, "check ptr");
        TEST_RESULT_SIZE(regExpMatchSize(regExp), 0, "check size");
        TEST_RESULT_PTR(regExpMatchStr(regExp), NULL, "check str");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("free regexp");

        TEST_RESULT_VOID(regExpFree(regExp), "free regexp");
    }

    // *****************************************************************************************************************************
    if (testBegin("regExpPrefix()"))
    {
        TEST_RESULT_PTR(regExpPrefix(NULL), NULL, "null expression has no prefix");
        TEST_RESULT_PTR(regExpPrefix(strNew("")), NULL, "empty expression has no prefix");
        TEST_RESULT_PTR(regExpPrefix(strNew("ABC")), NULL, "expression without begin anchor has no prefix");
        TEST_RESULT_PTR(regExpPrefix(strNew("^.")), NULL, "expression with no regular character has no prefix");

        TEST_RESULT_STR_Z(regExpPrefix(strNew("^ABC$")), "ABC", "prefix stops at special character");
        TEST_RESULT_STR_Z(regExpPrefix(strNew("^ABC*")), "ABC", "prefix stops at special character");
        TEST_RESULT_STR_Z(regExpPrefix(strNew("^ABC+")), "ABC", "prefix stops at special character");
        TEST_RESULT_STR_Z(regExpPrefix(strNew("^ABC-")), "ABC", "prefix stops at special character");
        TEST_RESULT_STR_Z(regExpPrefix(strNew("^ABC?")), "ABC", "prefix stops at special character");
        TEST_RESULT_STR_Z(regExpPrefix(strNew("^ABC(")), "ABC", "prefix stops at special character");
        TEST_RESULT_STR_Z(regExpPrefix(strNew("^ABC[")), "ABC", "prefix stops at special character");
        TEST_RESULT_STR_Z(regExpPrefix(strNew("^ABC{")), "ABC", "prefix stops at special character");
        TEST_RESULT_STR_Z(regExpPrefix(strNew("^ABC ")), "ABC", "prefix stops at special character");
        TEST_RESULT_STR_Z(regExpPrefix(strNew("^ABC|")), "ABC", "prefix stops at special character");
        TEST_RESULT_STR_Z(regExpPrefix(strNew("^ABC\\")), "ABC", "prefix stops at special character");

        TEST_RESULT_STR_Z(regExpPrefix(strNew("^ABC^")), NULL, "no prefix when more than one begin anchor");
        TEST_RESULT_STR_Z(regExpPrefix(strNew("^ABC|^DEF")), NULL, "no prefix when more than one begin anchor");
        TEST_RESULT_STR_Z(regExpPrefix(strNew("^ABC[^DEF]")), "ABC", "prefix when ^ used for exclusion");
        TEST_RESULT_STR_Z(regExpPrefix(strNew("^ABC\\^DEF]")), "ABC", "prefix when ^ is escaped");

        TEST_RESULT_STR_Z(regExpPrefix(strNew("^ABCDEF")), "ABCDEF", "prefix is entire expression");
    }

    // *****************************************************************************************************************************
    if (testBegin("regExpMatchOne()"))
    {
        TEST_RESULT_BOOL(regExpMatchOne(strNew("^abc"), strNew("abcdef")), true, "match regexp");
        TEST_RESULT_BOOL(regExpMatchOne(strNew("^abc"), strNew("bcdef")), false, "no match regexp");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
