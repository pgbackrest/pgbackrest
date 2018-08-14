/***********************************************************************************************************************************
Test Strings
***********************************************************************************************************************************/
#include "common/type/buffer.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("strNew(), strNewBuf(), strNewN(), strPtr(), and strFree()"))
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
        bufUsedSet(buffer, 8);

        TEST_RESULT_STR(strPtr(strNewBuf(buffer)), "12345678", "new string from buffer");

        // -------------------------------------------------------------------------------------------------------------------------
        string = strNewFmt("formatted %s %04d", "string", 1);
        TEST_RESULT_STR(strPtr(string), "formatted string 0001", "new with formatted string");
        TEST_RESULT_PTR(strPtr(NULL), NULL, "null string pointer");

        TEST_RESULT_VOID(strFree(string), "free string");
        TEST_RESULT_VOID(strFree(NULL), "free null string");
    }

    // *****************************************************************************************************************************
    if (testBegin("strBase() and strPath()"))
    {
        TEST_RESULT_STR(strPtr(strBase(strNew(""))), "", "empty string");
        TEST_RESULT_STR(strPtr(strBase(strNew("/"))), "", "/ only");
        TEST_RESULT_STR(strPtr(strBase(strNew("/file"))), "file", "root file");
        TEST_RESULT_STR(strPtr(strBase(strNew("/dir1/dir2/file"))), "file", "subdirectory file");

        TEST_RESULT_STR(strPtr(strPath(strNew(""))), "", "empty string");
        TEST_RESULT_STR(strPtr(strPath(strNew("/"))), "/", "/ only");
        TEST_RESULT_STR(strPtr(strPath(strNew("/file"))), "/", "root path");
        TEST_RESULT_STR(strPtr(strPath(strNew("/dir1/dir2/file"))), "/dir1/dir2", "subdirectory file");
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

        TEST_RESULT_PTR(strDup(NULL), NULL, "duplicate null string");
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
    if (testBegin("strFirstUpper(), strFirstLower(), strUpper(), strLower()"))
    {
        TEST_RESULT_STR(strPtr(strFirstUpper(strNew(""))), "", "empty first upper");
        TEST_RESULT_STR(strPtr(strFirstUpper(strNew("aaa"))), "Aaa", "first upper");
        TEST_RESULT_STR(strPtr(strFirstUpper(strNew("Aaa"))), "Aaa", "first already upper");

        TEST_RESULT_STR(strPtr(strFirstLower(strNew(""))), "", "empty first lower");
        TEST_RESULT_STR(strPtr(strFirstLower(strNew("AAA"))), "aAA", "first lower");
        TEST_RESULT_STR(strPtr(strFirstLower(strNew("aAA"))), "aAA", "first already lower");

        TEST_RESULT_STR(strPtr(strLower(strNew("K123aBc"))), "k123abc", "all lower");
        TEST_RESULT_STR(strPtr(strLower(strNew("k123abc"))), "k123abc", "already lower");
        TEST_RESULT_STR(strPtr(strLower(strNew("C"))), "c", "char lower");
        TEST_RESULT_STR(strPtr(strLower(strNew(""))), "", "empty lower");

        TEST_RESULT_STR(strPtr(strUpper(strNew("K123aBc"))), "K123ABC", "all upper");
        TEST_RESULT_STR(strPtr(strUpper(strNew("K123ABC"))), "K123ABC", "already upper");
        TEST_RESULT_STR(strPtr(strUpper(strNew("c"))), "C", "char upper");
        TEST_RESULT_STR(strPtr(strUpper(strNew(""))), "", "empty upper");
    }

    // *****************************************************************************************************************************
    if (testBegin("strQuote()"))
    {
        TEST_RESULT_STR(strPtr(strQuote(strNew("abcd"), strNew("'"))), "'abcd'", "quote string");
    }

    // *****************************************************************************************************************************
    if (testBegin("strReplaceChr()"))
    {
        TEST_RESULT_STR(strPtr(strReplaceChr(strNew("ABCD"), 'B', 'R')), "ARCD", "replace chr");
    }

    // *****************************************************************************************************************************
    if (testBegin("strSub() and strSubN()"))
    {
        TEST_RESULT_STR(strPtr(strSub(strNew("ABCD"), 2)), "CD", "sub string");
        TEST_RESULT_STR(strPtr(strSubN(strNew("ABCD"), 1, 2)), "BC", "sub string with length");
    }

    // *****************************************************************************************************************************
    if (testBegin("strTrim()"))
    {
        TEST_RESULT_STR(strPtr(strTrim(strNew(""))), "", "trim empty");
        TEST_RESULT_STR(strPtr(strTrim(strNew("X"))), "X", "no trim (one char)");
        TEST_RESULT_STR(strPtr(strTrim(strNew("no-trim"))), "no-trim", "no trim (string)");
        TEST_RESULT_STR(strPtr(strTrim(strNew(" \t\r\n"))), "", "all whitespace");
        TEST_RESULT_STR(strPtr(strTrim(strNew(" \tbegin-only"))), "begin-only", "trim begin");
        TEST_RESULT_STR(strPtr(strTrim(strNew("end-only\t "))), "end-only", "trim end");
        TEST_RESULT_STR(strPtr(strTrim(strNew("\n\rboth\r\n"))), "both", "trim both");
        TEST_RESULT_STR(strPtr(strTrim(strNew("begin \r\n\tend"))), "begin \r\n\tend", "ignore whitespace in middle");
    }

    // *****************************************************************************************************************************
    if (testBegin("strChr() and strTrunc()"))
    {
        TEST_RESULT_INT(strChr(strNew("abcd"), 'c'), 2, "c found");
        TEST_RESULT_INT(strChr(strNew("abcd"), 'C'), -1, "capital C not found");
        TEST_RESULT_INT(strChr(strNew("abcd"), 'i'), -1, "i not found");
        TEST_RESULT_INT(strChr(strNew(""), 'x'), -1, "empty string - x not found");

        String *val = strNew("abcdef");
        TEST_ERROR(
            strTrunc(val, (int)(strSize(val) + 1)), AssertError,
            "function test assertion 'idx >= 0 && (size_t)idx <= this->size' failed");
        TEST_ERROR(strTrunc(val, -1), AssertError, "function test assertion 'idx >= 0 && (size_t)idx <= this->size' failed");

        TEST_RESULT_STR(strPtr(strTrunc(val, strChr(val, 'd'))), "abc", "simple string truncated");
        strCat(val, "\r\n to end");
        TEST_RESULT_STR(strPtr(strTrunc(val, strChr(val, 'n'))), "abc\r\n to e", "complex string truncated");
        TEST_RESULT_STR(strPtr(strTrunc(val, strChr(val, 'a'))), "", "complete string truncated - empty string");

        TEST_RESULT_INT(strSize(val), 0, "0 size");
        TEST_RESULT_STR(strPtr(strTrunc(val, 0)), "", "test coverage of empty string - no error thrown for index 0");
    }

    // *****************************************************************************************************************************
    if (testBegin("strToLog()"))
    {
        char buffer[STACK_TRACE_PARAM_MAX];

        TEST_RESULT_INT(strToLog(NULL, buffer, 4), 4, "format null string with too small buffer");
        TEST_RESULT_STR(buffer, "nul", "    check format");

        TEST_RESULT_INT(strToLog(NULL, buffer, STACK_TRACE_PARAM_MAX), 4, "format null string");
        TEST_RESULT_STR(buffer, "null", "    check format");

        TEST_RESULT_INT(strToLog(strNew("test"), buffer, STACK_TRACE_PARAM_MAX), 8, "format string");
        TEST_RESULT_STR(buffer, "{\"test\"}", "    check format");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
