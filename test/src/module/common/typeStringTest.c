/***********************************************************************************************************************************
Test Strings
***********************************************************************************************************************************/
#include "common/type/buffer.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    // *****************************************************************************************************************************
    if (testBegin("strNew(), strNewBuf(), strNewN(), and strFree()"))
    {
        String *string = strNew("static string");
        TEST_RESULT_STR(strPtr(string), "static string", "new with static string");
        TEST_RESULT_INT(strSize(string), 13, "check size");
        TEST_RESULT_INT(strlen(strPtr(string)), 13, "check size with strlen()");
        TEST_RESULT_CHAR(strPtr(string)[2], 'a', "check character");

        TEST_RESULT_VOID(strFree(string), "free string");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR(strPtr(strNewN("testmorestring", 4)), "test", "new string with size limit");

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
    if (testBegin("strBase()"))
    {
        TEST_RESULT_STR(strPtr(strBase(strNew(""))), "", "empty string");
        TEST_RESULT_STR(strPtr(strBase(strNew("/"))), "", "/ only");
        TEST_RESULT_STR(strPtr(strBase(strNew("/file"))), "file", "root file");
        TEST_RESULT_STR(strPtr(strBase(strNew("/dir1/dir2/file"))), "file", "subdirectory file");
    }

    // *****************************************************************************************************************************
    if (testBegin("strCat() and strCatFmt()"))
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
    if (testBegin("strBeginsWith() and strBeginsWithZ()"))
    {
        TEST_RESULT_BOOL(strBeginsWith(strNew(""), strNew("aaa")), false, "empty string");
        TEST_RESULT_BOOL(strBeginsWith(strNew("astring"), strNew("")), true, "empty begins with");
        TEST_RESULT_BOOL(strBeginsWithZ(strNew("astring"), "astr"), true, "partial begins with");
        TEST_RESULT_BOOL(strBeginsWithZ(strNew("astring"), "astring"), true, "equal strings");
    }

    // *****************************************************************************************************************************
    if (testBegin("strEndsWith() and strEndsWithZ()"))
    {
        TEST_RESULT_BOOL(strEndsWith(strNew(""), strNew(".doc")), false, "empty string");
        TEST_RESULT_BOOL(strEndsWith(strNew("astring"), strNew("")), true, "empty ends with");
        TEST_RESULT_BOOL(strEndsWithZ(strNew("astring"), "ing"), true, "partial ends with");
        TEST_RESULT_BOOL(strEndsWithZ(strNew("astring"), "astring"), true, "equal strings");
    }

    // *****************************************************************************************************************************
    if (testBegin("strEq(), strEqZ(), strCmp(), strCmpZ()"))
    {
        TEST_RESULT_BOOL(strEq(strNew("equalstring"), strNew("equalstring")), true, "strings equal");
        TEST_RESULT_BOOL(strEq(strNew("astring"), strNew("anotherstring")), false, "strings not equal");
        TEST_RESULT_BOOL(strEq(strNew("astring"), strNew("bstring")), false, "equal length strings not equal");

        TEST_RESULT_INT(strCmp(strNew("equalstring"), strNew("equalstring")), 0, "strings equal");
        TEST_RESULT_INT(strCmp(strNew("a"), strNew("b")), -1, "a < b");
        TEST_RESULT_INT(strCmp(strNew("b"), strNew("a")), 1, "b > a");

        TEST_RESULT_BOOL(strEqZ(strNew("equalstring"), "equalstring"), true, "strings equal");
        TEST_RESULT_BOOL(strEqZ(strNew("astring"), "anotherstring"), false, "strings not equal");
        TEST_RESULT_BOOL(strEqZ(strNew("astring"), "bstring"), false, "equal length strings not equal");

        TEST_RESULT_INT(strCmpZ(strNew("equalstring"), "equalstring"), 0, "strings equal");
        TEST_RESULT_INT(strCmpZ(strNew("a"), "b"), -1, "a < b");
        TEST_RESULT_INT(strCmpZ(strNew("b"), "a"), 1, "b > a");
    }

    // *****************************************************************************************************************************
    if (testBegin("strFirstUpper() and strFirstLower()"))
    {
        TEST_RESULT_STR(strPtr(strFirstUpper(strNew(""))), "", "empty first upper");
        TEST_RESULT_STR(strPtr(strFirstUpper(strNew("aaa"))), "Aaa", "first upper");
        TEST_RESULT_STR(strPtr(strFirstUpper(strNew("Aaa"))), "Aaa", "first already upper");

        TEST_RESULT_STR(strPtr(strFirstLower(strNew(""))), "", "empty first lower");
        TEST_RESULT_STR(strPtr(strFirstLower(strNew("AAA"))), "aAA", "first lower");
        TEST_RESULT_STR(strPtr(strFirstLower(strNew("aAA"))), "aAA", "first already lower");
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
