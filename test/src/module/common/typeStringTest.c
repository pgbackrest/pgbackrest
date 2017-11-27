/***********************************************************************************************************************************
Test Strings
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void testRun()
{
    // *****************************************************************************************************************************
    if (testBegin("strNew() and strFree()"))
    {
        String *string = strNew("static string");
        TEST_RESULT_STR(strPtr(string), "static string", "new with static string");
        TEST_RESULT_INT(strSize(string), 13, "check size");
        TEST_RESULT_INT(strlen(strPtr(string)), 13, "check size with strlen()");
        TEST_RESULT_CHAR(strPtr(string)[2], 'a', "check character");
        strFree(string);

        string = strNewFmt("formatted %s %04d", "string", 1);
        TEST_RESULT_STR(strPtr(string), "formatted string 0001", "new with formatted string");
        strFree(string);
    }

    // *****************************************************************************************************************************
    if (testBegin("strCat and strCatFmt()"))
    {
        String *string = strNew("XXXX");
        String *string2 = strNew("ZZZZ");

        strCat(string, "YYYY");
        TEST_RESULT_STR(strPtr(string), "XXXXYYYY", "cat string");

        strCatFmt(string, "%05d", 777);
        TEST_RESULT_STR(strPtr(string), "XXXXYYYY00777", "cat formatted string");

        TEST_RESULT_STR(strPtr(string2), "ZZZZ", "check unaltered string");

        strFree(string);
        strFree(string2);
    }
}
