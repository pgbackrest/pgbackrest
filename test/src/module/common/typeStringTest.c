/***********************************************************************************************************************************
Test Strings
***********************************************************************************************************************************/
#include "common/type/buffer.h"

// Declare a static const string for testing
STRING_STATIC(TEST_STRING, "a very interesting string!");

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("strNew(), strNewBuf(), strNewN(), strEmpty(), strPtr(), strSize(), and strFree()"))
    {
        // We don't want this struct to grow since there are generally a lot of strings, so make sure it doesn't grow without us
        // knowing about it
        TEST_RESULT_UINT(sizeof(StringConst), TEST_64BIT() ? 16 : 12, "check StringConst struct size");

        // Test the size macro
        TEST_RESULT_VOID(CHECK_SIZE(555), "valid size");
        TEST_ERROR(CHECK_SIZE(STRING_SIZE_MAX + 1), AssertError, "string size must be <= 1073741824 bytes");

        String *string = strNew("static string");
        TEST_RESULT_STR(strPtr(string), "static string", "new with static string");
        TEST_RESULT_INT(strSize(string), 13, "check size");
        TEST_RESULT_BOOL(strEmpty(string), false, "is not empty");
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
    if (testBegin("STRING_STATIC()"))
    {
        TEST_RESULT_STR(strPtr(TEST_STRING), "a very interesting string!", "check static string");
        TEST_RESULT_STR(strPtr(strSubN(TEST_STRING, 0, 6)), "a very", "read-only strSub() works");
    }

    // *****************************************************************************************************************************
    if (testBegin("strBase() and strPath()"))
    {
        TEST_RESULT_STR(strPtr(strBase(STRDEF(""))), "", "empty string");
        TEST_RESULT_STR(strPtr(strBase(STRDEF("/"))), "", "/ only");
        TEST_RESULT_STR(strPtr(strBase(STRDEF("/file"))), "file", "root file");
        TEST_RESULT_STR(strPtr(strBase(STRDEF("/dir1/dir2/file"))), "file", "subdirectory file");

        TEST_RESULT_STR(strPtr(strPath(STRDEF(""))), "", "empty string");
        TEST_RESULT_STR(strPtr(strPath(STRDEF("/"))), "/", "/ only");
        TEST_RESULT_STR(strPtr(strPath(STRDEF("/file"))), "/", "root path");
        TEST_RESULT_STR(strPtr(strPath(STRDEF("/dir1/dir2/file"))), "/dir1/dir2", "subdirectory file");
    }

    // *****************************************************************************************************************************
    if (testBegin("strCat(), strCatChr(), and strCatFmt()"))
    {
        String *string = strNew("XXXX");
        String *string2 = strNew("ZZZZ");

        TEST_RESULT_STR(strPtr(strCat(string, "YYYY")), "XXXXYYYY", "cat string");
        TEST_RESULT_SIZE(string->extra, 4, "check extra");
        TEST_RESULT_STR(strPtr(strCatFmt(string, "%05d", 777)), "XXXXYYYY00777", "cat formatted string");
        TEST_RESULT_SIZE(string->extra, 6, "check extra");
        TEST_RESULT_STR(strPtr(strCatChr(string, '!')), "XXXXYYYY00777!", "cat chr");
        TEST_RESULT_SIZE(string->extra, 5, "check extra");

        TEST_RESULT_STR(strPtr(string2), "ZZZZ", "check unaltered string");
    }

    // *****************************************************************************************************************************
    if (testBegin("strDup()"))
    {
        const String *string = STRDEF("duplicated string");
        String *stringDup = strDup(string);
        TEST_RESULT_STR(strPtr(stringDup), strPtr(string), "duplicated strings match");

        TEST_RESULT_PTR(strDup(NULL), NULL, "duplicate null string");
    }

    // *****************************************************************************************************************************
    if (testBegin("strBeginsWith() and strBeginsWithZ()"))
    {
        TEST_RESULT_BOOL(strBeginsWith(STRDEF(""), STRDEF("aaa")), false, "empty string");
        TEST_RESULT_BOOL(strBeginsWith(STRDEF("astring"), STRDEF("")), true, "empty begins with");
        TEST_RESULT_BOOL(strBeginsWithZ(STRDEF("astring"), "astr"), true, "partial begins with");
        TEST_RESULT_BOOL(strBeginsWithZ(STRDEF("astring"), "astring"), true, "equal strings");
    }

    // *****************************************************************************************************************************
    if (testBegin("strEndsWith() and strEndsWithZ()"))
    {
        TEST_RESULT_BOOL(strEndsWith(STRDEF(""), STRDEF(".doc")), false, "empty string");
        TEST_RESULT_BOOL(strEndsWith(STRDEF("astring"), STRDEF("")), true, "empty ends with");
        TEST_RESULT_BOOL(strEndsWithZ(STRDEF("astring"), "ing"), true, "partial ends with");
        TEST_RESULT_BOOL(strEndsWithZ(STRDEF("astring"), "astring"), true, "equal strings");
    }

    // *****************************************************************************************************************************
    if (testBegin("strEq(), strEqZ(), strCmp(), strCmpZ()"))
    {
        TEST_RESULT_BOOL(strEq(STRDEF("equalstring"), STRDEF("equalstring")), true, "strings equal");
        TEST_RESULT_BOOL(strEq(STRDEF("astring"), STRDEF("anotherstring")), false, "strings not equal");
        TEST_RESULT_BOOL(strEq(STRDEF("astring"), STRDEF("bstring")), false, "equal length strings not equal");
        TEST_RESULT_BOOL(strEq(NULL, STRDEF("bstring")), false, "null is not equal to bstring");
        TEST_RESULT_BOOL(strEq(STRDEF("astring"), NULL), false, "null is not equal to astring");
        TEST_RESULT_BOOL(strEq(NULL, NULL), true, "null is equal to null");

        TEST_RESULT_INT(strCmp(STRDEF("equalstring"), STRDEF("equalstring")), 0, "strings equal");
        TEST_RESULT_INT(strCmp(STRDEF("a"), STRDEF("b")), -1, "a < b");
        TEST_RESULT_INT(strCmp(STRDEF("b"), STRDEF("a")), 1, "b > a");
        TEST_RESULT_INT(strCmp(NULL, NULL), 0, "null == null");
        TEST_RESULT_INT(strCmp(NULL, STRDEF("x")), -1, "null < not null");
        TEST_RESULT_INT(strCmp(STRDEF("x"), NULL), 1, "not null > null");

        TEST_RESULT_BOOL(strEqZ(STRDEF("equalstring"), "equalstring"), true, "strings equal");
        TEST_RESULT_BOOL(strEqZ(STRDEF("astring"), "anotherstring"), false, "strings not equal");
        TEST_RESULT_BOOL(strEqZ(STRDEF("astring"), "bstring"), false, "equal length strings not equal");

        TEST_RESULT_INT(strCmpZ(STRDEF("equalstring"), "equalstring"), 0, "strings equal");
        TEST_RESULT_INT(strCmpZ(STRDEF("a"), "b"), -1, "a < b");
        TEST_RESULT_INT(strCmpZ(STRDEF("b"), "a"), 1, "b > a");
        TEST_RESULT_INT(strCmpZ(STRDEF("b"), NULL), 1, "b > null");
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
        TEST_RESULT_STR(strPtr(strQuote(STRDEF("abcd"), STRDEF("'"))), "'abcd'", "quote string");
    }

    // *****************************************************************************************************************************
    if (testBegin("strReplaceChr()"))
    {
        TEST_RESULT_STR(strPtr(strReplaceChr(strNew("ABCD"), 'B', 'R')), "ARCD", "replace chr");
    }

    // *****************************************************************************************************************************
    if (testBegin("strSub() and strSubN()"))
    {
        TEST_RESULT_STR(strPtr(strSub(STRDEF("ABCD"), 2)), "CD", "sub string");
        TEST_RESULT_STR(strPtr(strSubN(STRDEF("ABCD"), 1, 2)), "BC", "sub string with length");
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
        TEST_RESULT_INT(strChr(STRDEF("abcd"), 'c'), 2, "c found");
        TEST_RESULT_INT(strChr(STRDEF("abcd"), 'C'), -1, "capital C not found");
        TEST_RESULT_INT(strChr(STRDEF("abcd"), 'i'), -1, "i not found");
        TEST_RESULT_INT(strChr(STRDEF(""), 'x'), -1, "empty string - x not found");

        String *val = strNew("abcdef");
        TEST_ERROR(
            strTrunc(val, (int)(strSize(val) + 1)), AssertError,
            "assertion 'idx >= 0 && (size_t)idx <= this->size' failed");
        TEST_ERROR(strTrunc(val, -1), AssertError, "assertion 'idx >= 0 && (size_t)idx <= this->size' failed");

        TEST_RESULT_STR(strPtr(strTrunc(val, strChr(val, 'd'))), "abc", "simple string truncated");
        strCat(val, "\r\n to end");
        TEST_RESULT_STR(strPtr(strTrunc(val, strChr(val, 'n'))), "abc\r\n to e", "complex string truncated");
        TEST_RESULT_STR(strPtr(strTrunc(val, strChr(val, 'a'))), "", "complete string truncated - empty string");

        TEST_RESULT_INT(strSize(val), 0, "0 size");
        TEST_RESULT_STR(strPtr(strTrunc(val, 0)), "", "test coverage of empty string - no error thrown for index 0");
    }

    // *****************************************************************************************************************************
    if (testBegin("strToLog() and strObjToLog()"))
    {
        TEST_RESULT_STR(strPtr(strToLog(STRDEF("test"))), "{\"test\"}", "format string");
        TEST_RESULT_STR(strPtr(strToLog(NULL)), "null", "format null string");

        char buffer[256];
        TEST_RESULT_UINT(strObjToLog(NULL, (StrObjToLogFormat)strToLog, buffer, sizeof(buffer)), 4, "format null string");
        TEST_RESULT_STR(buffer, "null", "check null string");

        TEST_RESULT_UINT(strObjToLog(STRDEF("teststr"), (StrObjToLogFormat)strToLog, buffer, sizeof(buffer)), 11, "format string");
        TEST_RESULT_STR(buffer, "{\"teststr\"}", "check string");
    }

    // *****************************************************************************************************************************
    if (testBegin("strSizeFormat()"))
    {
        TEST_RESULT_STR(strPtr(strSizeFormat(0)), "0B", "zero bytes");
        TEST_RESULT_STR(strPtr(strSizeFormat(1023)), "1023B", "1023 bytes");
        TEST_RESULT_STR(strPtr(strSizeFormat(1024)), "1KB", "1 KB");
        TEST_RESULT_STR(strPtr(strSizeFormat(2200)), "2.1KB", "2.1 KB");
        TEST_RESULT_STR(strPtr(strSizeFormat(1048576)), "1MB", "1 MB");
        TEST_RESULT_STR(strPtr(strSizeFormat(20162900)), "19.2MB", "19.2 MB");
        TEST_RESULT_STR(strPtr(strSizeFormat(1073741824)), "1GB", "1 GB");
        TEST_RESULT_STR(strPtr(strSizeFormat(1073741824 + 107374183)), "1.1GB", "1.1 GB");
        TEST_RESULT_STR(strPtr(strSizeFormat(UINT64_MAX)), "17179869183GB", "uint64 max");
    }

    // *****************************************************************************************************************************
    if (testBegin("strLstNew(), strLstAdd*(), strLstGet(), strLstMove(), strLstSize(), and strLstFree()"))
    {
        // Add strings to the list
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *list = NULL;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            list = strLstNew();

            for (int listIdx = 0; listIdx <= LIST_INITIAL_SIZE; listIdx++)
            {
                if (listIdx == 0)
                {
                    TEST_RESULT_PTR(strLstAdd(list, NULL), NULL, "add null item");
                }
                else
                {
                    TEST_RESULT_STR_STR(
                        strLstAdd(list, strNewFmt("STR%02d", listIdx)), strNewFmt("STR%02d", listIdx), "add item %d", listIdx);
                }
            }

            strLstMove(list, MEM_CONTEXT_OLD());
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_INT(strLstSize(list), 9, "list size");

        // Read them back and check values
        // -------------------------------------------------------------------------------------------------------------------------
        for (unsigned int listIdx = 0; listIdx < strLstSize(list); listIdx++)
        {
            if (listIdx == 0)
            {
                TEST_RESULT_PTR(strLstGet(list, listIdx), NULL, "check null item");
            }
            else
                TEST_RESULT_STR(strPtr(strLstGet(list, listIdx)), strPtr(strNewFmt("STR%02u", listIdx)), "check item %u", listIdx);
        }

        TEST_RESULT_VOID(strLstFree(list), "free string list");
        TEST_RESULT_VOID(strLstFree(NULL), "free null string list");

        // Add if missing and remove
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(list, strLstNew(), "new list");
        TEST_RESULT_VOID(strLstAddIfMissing(list, STRDEF("item1")), "add item 1");
        TEST_RESULT_UINT(strLstSize(list), 1, "check size");
        TEST_RESULT_BOOL(strLstExistsZ(list, "item1"), true, "check exists");
        TEST_RESULT_VOID(strLstAddIfMissing(list, STRDEF("item1")), "add item 1 again");
        TEST_RESULT_UINT(strLstSize(list), 1, "check size");

        TEST_RESULT_BOOL(strLstRemove(list, STRDEF("item1")), true, "remove item 1");
        TEST_RESULT_BOOL(strLstRemove(list, STRDEF("item1")), false, "remove item 1 fails");
        TEST_RESULT_UINT(strLstSize(list), 0, "    check size");
    }

    // *****************************************************************************************************************************
    if (testBegin("strLstNewSplit()"))
    {
        TEST_RESULT_STR(strPtr(strLstJoin(strLstNewSplit(STRDEF(""), STRDEF(", ")), ", ")), "", "empty list");
        TEST_RESULT_STR(strPtr(strLstJoin(strLstNewSplit(STRDEF("item1"), STRDEF(", ")), ", ")), "item1", "one item");
        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstNewSplit(STRDEF("item1, item2"), STRDEF(", ")), ", ")), "item1, item2", "two items");
    }

    // *****************************************************************************************************************************
    if (testBegin("strLstNewSplitSize()"))
    {
        TEST_RESULT_STR(strPtr(strLstJoin(strLstNewSplitSize(STRDEF(""), STRDEF(" "), 0), ", ")), "", "empty list");
        TEST_RESULT_STR(strPtr(strLstJoin(strLstNewSplitSizeZ(STRDEF("abc def"), " ", 3), "-")), "abc-def", "two items");
        TEST_RESULT_STR(strPtr(strLstJoin(strLstNewSplitSizeZ(STRDEF("abc def"), " ", 4), "-")), "abc-def", "one items");
        TEST_RESULT_STR(strPtr(strLstJoin(strLstNewSplitSizeZ(STRDEF("abc def ghi"), " ", 4), "-")), "abc-def-ghi", "three items");
        TEST_RESULT_STR(strPtr(strLstJoin(strLstNewSplitSizeZ(STRDEF("abc def ghi"), " ", 8), "-")), "abc def-ghi", "three items");
        TEST_RESULT_STR(strPtr(strLstJoin(strLstNewSplitSizeZ(STRDEF("abc def "), " ", 4), "-")), "abc-def ", "two items");

        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstNewSplitSize(STRDEF("this is a short sentence"), STRDEF(" "), 10), "\n")),
            "this is a\n"
            "short\n"
            "sentence",
            "empty list");
    }

    // *****************************************************************************************************************************
    if (testBegin("strLstNewVarLst()"))
    {
        VariantList *varList = varLstNew();

        varLstAdd(varList, varNewStr(STRDEF("string1")));
        varLstAdd(varList, varNewStr(STRDEF("string2")));

        TEST_RESULT_STR(strPtr(strLstJoin(strLstNewVarLst(varList), ", ")), "string1, string2", "string list from variant list");
        TEST_RESULT_PTR(strLstNewVarLst(NULL), NULL, "null list from null var list");

        varLstFree(varList);
    }

    // *****************************************************************************************************************************
    if (testBegin("strLstPtr()"))
    {
        StringList *list = strLstNew();

        // Add strings to the list
        // -------------------------------------------------------------------------------------------------------------------------
        for (int listIdx = 0; listIdx <= 3; listIdx++)
        {
            if (listIdx == 0)
                strLstAdd(list, NULL);
            else
                strLstAdd(list, strNewFmt("STR%02d", listIdx));
        }

        // Check pointer
        // -------------------------------------------------------------------------------------------------------------------------
        const char **szList = strLstPtr(list);

        for (unsigned int listIdx = 0; listIdx < strLstSize(list); listIdx++)
        {
            if (listIdx == 0)
            {
                TEST_RESULT_PTR(szList[listIdx], NULL, "check null item");
            }
            else
                TEST_RESULT_STR(szList[listIdx], strPtr(strNewFmt("STR%02u", listIdx)), "check item %u", listIdx);
        }

        TEST_RESULT_PTR(szList[strLstSize(list)], NULL, "check null terminator");

        strLstFree(list);
    }

    // *****************************************************************************************************************************
    if (testBegin("strLstExists() and strLstExistsZ()"))
    {
        StringList *list = strLstNew();
        strLstAddZ(list, "A");
        strLstAddZ(list, "C");

        TEST_RESULT_BOOL(strLstExists(list, STRDEF("B")), false, "string does not exist");
        TEST_RESULT_BOOL(strLstExists(list, STRDEF("C")), true, "string exists");
        TEST_RESULT_BOOL(strLstExistsZ(list, "B"), false, "string does not exist");
        TEST_RESULT_BOOL(strLstExistsZ(list, "C"), true, "string exists");
    }

    // *****************************************************************************************************************************
    if (testBegin("strLstJoin()"))
    {
        StringList *list = strLstNew();

        TEST_RESULT_STR(strPtr(strLstJoin(list, ", ")), "", "empty list");

        strLstAdd(list, STRDEF("item1"));
        strLstAddZ(list, "item2");

        TEST_RESULT_STR(strPtr(strLstJoin(list, ", ")), "item1, item2", "list");

        strLstAdd(list, NULL);

        TEST_RESULT_STR(strPtr(strLstJoin(list, ", ")), "item1, item2, [NULL]", "list with NULL at end");

        TEST_RESULT_STR(strPtr(strLstJoin(strLstDup(list), ", ")), "item1, item2, [NULL]", "dup'd list with NULL at end");
        TEST_RESULT_PTR(strLstDup(NULL), NULL, "dup NULL list");

        strLstFree(list);
    }

    // *****************************************************************************************************************************
    if (testBegin("strLstMergeAnti()"))
    {
        StringList *list = strLstNew();
        StringList *anti = strLstNew();

        TEST_RESULT_STR(strPtr(strLstJoin(strLstMergeAnti(list, anti), ", ")), "", "list and anti empty");

        strLstAddZ(anti, "item2");
        strLstAddZ(anti, "item3");

        TEST_RESULT_STR(strPtr(strLstJoin(strLstMergeAnti(list, anti), ", ")), "", "list empty");

        strLstAddZ(list, "item1");
        strLstAddZ(list, "item3");
        strLstAddZ(list, "item4");
        strLstAddZ(list, "item5");

        TEST_RESULT_STR(strPtr(strLstJoin(strLstMergeAnti(list, anti), ", ")), "item1, item4, item5", "list results");
        TEST_RESULT_STR(strPtr(strLstJoin(strLstMergeAnti(list, strLstNew()), ", ")), "item1, item3, item4, item5", "anti empty");

        list = strLstNew();
        strLstAddZ(list, "item2");
        strLstAddZ(list, "item4");
        strLstAddZ(list, "item6");

        anti = strLstNew();
        strLstAddZ(anti, "item1");
        strLstAddZ(anti, "item4");
        strLstAddZ(anti, "item7");

        TEST_RESULT_STR(strPtr(strLstJoin(strLstMergeAnti(list, anti), ", ")), "item2, item6", "list results");

        list = strLstNew();
        strLstAddZ(list, "item7");

        anti = strLstNew();
        strLstAddZ(anti, "item1");
        strLstAddZ(anti, "item4");
        strLstAddZ(anti, "item6");

        TEST_RESULT_STR(strPtr(strLstJoin(strLstMergeAnti(list, anti), ", ")), "item7", "list results");
    }

    // *****************************************************************************************************************************
    if (testBegin("strLstSort()"))
    {
        StringList *list = strLstNew();

        strLstAddZ(list, "c");
        strLstAddZ(list, "a");
        strLstAddZ(list, "b");

        TEST_RESULT_STR(strPtr(strLstJoin(strLstSort(list, sortOrderAsc), ", ")), "a, b, c", "sort ascending");
        TEST_RESULT_STR(strPtr(strLstJoin(strLstSort(list, sortOrderDesc), ", ")), "c, b, a", "sort descending");

        strLstComparatorSet(list, lstComparatorStr);
        TEST_RESULT_STR(strPtr(strLstJoin(strLstSort(list, sortOrderAsc), ", ")), "a, b, c", "sort ascending");
    }

    // *****************************************************************************************************************************
    if (testBegin("strLstToLog()"))
    {
        StringList *list = strLstNew();

        TEST_RESULT_STR(strPtr(strLstToLog(list)), "{[]}", "format empty list");

        strLstInsertZ(list, 0, "item3");
        TEST_RESULT_STR(strPtr(strLstToLog(list)), "{[\"item3\"]}", "format 1 item list");

        strLstInsert(list, 0, STRDEF("item1"));
        strLstInsertZ(list, 1, "item2");
        TEST_RESULT_STR(strPtr(strLstToLog(list)), "{[\"item1\", \"item2\", \"item3\"]}", "format 3 item list");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
