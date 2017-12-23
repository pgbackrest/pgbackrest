/***********************************************************************************************************************************
Test Strings
***********************************************************************************************************************************/
#include "common/type/buffer.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void testRun()
{
    // *****************************************************************************************************************************
    if (testBegin("strNew(), strNewBuf(), strNewSzN(), and strFree()"))
    {
        String *string = strNew("static string");
        TEST_RESULT_STR(strPtr(string), "static string", "new with static string");
        TEST_RESULT_INT(strSize(string), 13, "check size");
        TEST_RESULT_INT(strlen(strPtr(string)), 13, "check size with strlen()");
        TEST_RESULT_CHAR(strPtr(string)[2], 'a', "check character");

        TEST_RESULT_VOID(strFree(string), "free string");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR(strPtr(strNewSzN("testmorestring", 4)), "test", "new string with size limit");

        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *buffer = bufNew(8);
        memcpy(bufPtr(buffer), "12345678", 8);

        TEST_RESULT_STR(strPtr(strNewBuf(buffer)), "12345678", "new string from buffer");

        // -------------------------------------------------------------------------------------------------------------------------
        string = strNewFmt("formatted %s %04d", "string", 1);
        TEST_RESULT_STR(strPtr(string), "formatted string 0001", "new with formatted string");

        TEST_RESULT_VOID(strFree(string), "free string");
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

    // *****************************************************************************************************************************
    if (testBegin("strDup()"))
    {
        String *string = strNew("duplicated string");
        String *stringDup = strDup(string);
        TEST_RESULT_STR(strPtr(stringDup), strPtr(string), "duplicated strings match");
    }

    // *****************************************************************************************************************************
    if (testBegin("strEq()"))
    {
        TEST_RESULT_BOOL(strEq(strNew("equalstring"), strNew("equalstring")), true, "strings equal");
        TEST_RESULT_BOOL(strEq(strNew("astring"), strNew("anotherstring")), false, "strings not equal");
        TEST_RESULT_BOOL(strEq(strNew("astring"), strNew("bstring")), false, "equal length strings not equal");
    }

    // *****************************************************************************************************************************
    if (testBegin("strTrim()"))
    {
        TEST_RESULT_STR(strPtr(strTrim(strNew(""))), "", "trim empty");
        TEST_RESULT_STR(strPtr(strTrim(strNew("X"))), "X", "no trim (one char)");
        TEST_RESULT_STR(strPtr(strTrim(strNew("no-trim"))), "no-trim", "no trim (string)");
        TEST_RESULT_STR(strPtr(strTrim(strNew(" \tbegin-only"))), "begin-only", "trim begin");
        TEST_RESULT_STR(strPtr(strTrim(strNew("end-only\t "))), "end-only", "trim end");
        TEST_RESULT_STR(strPtr(strTrim(strNew("\n\rboth\r\n"))), "both", "trim both");
        TEST_RESULT_STR(strPtr(strTrim(strNew("begin \r\n\tend"))), "begin \r\n\tend", "ignore whitespace in middle");
    }
}
