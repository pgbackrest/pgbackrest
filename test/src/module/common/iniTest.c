/***********************************************************************************************************************************
Test Ini
***********************************************************************************************************************************/
#include "common/io/bufferRead.h"
#include "common/type/buffer.h"
#include "storage/posix/storage.h"

/***********************************************************************************************************************************
Test callback to accumulate ini load results
***********************************************************************************************************************************/
static void
testIniLoadCallback(void *data, const String *section, const String *key, const String *value, const Variant *valueVar)
{
    ASSERT(strEq(value, jsonFromVar(valueVar)));
    strCatFmt((String *)data, "%s:%s:%s\n", strZ(section), strZ(key), strZ(value));
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("iniLoad()"))
    {
        // Empty file
        // -------------------------------------------------------------------------------------------------------------------------
        const Buffer *iniBuf = bufNew(0);
        String *result = strNew();

        TEST_RESULT_VOID(iniLoad(ioBufferReadNew(iniBuf), testIniLoadCallback, result), "load ini");
        TEST_RESULT_STR_Z(result, "", "    check ini");

        // Key outside of section
        // -------------------------------------------------------------------------------------------------------------------------
        iniBuf = BUFSTRZ(
            "key=value\n");

        TEST_ERROR(
            iniLoad(ioBufferReadNew(iniBuf), testIniLoadCallback, result), FormatError,
            "key/value found outside of section at line 1: key=value");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid JSON value");

        iniBuf = BUFSTRZ("[section]\nkey=value\n");

        TEST_ERROR(
            iniLoad(ioBufferReadNew(iniBuf), testIniLoadCallback, result), FormatError, "invalid JSON value at line 2: key=value");

        // Key outside of section
        // -------------------------------------------------------------------------------------------------------------------------
        iniBuf = BUFSTRZ(
            "[section]\n"
            "key");

        TEST_ERROR(
            iniLoad(ioBufferReadNew(iniBuf), testIniLoadCallback, result), FormatError,
            "missing '=' in key/value at line 2: key");

        // Zero length key
        // -------------------------------------------------------------------------------------------------------------------------
        iniBuf = BUFSTRZ(
            "[section]\n"
            "=\"value\"");

        TEST_ERROR(
            iniLoad(ioBufferReadNew(iniBuf), testIniLoadCallback, result), FormatError,
            "key is zero-length at line 2: =\"value\"");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("one section");

        iniBuf = BUFSTRZ(
            "[section1]\n"
            " key1 =\"value1\"\n"
            "key2=\"value2\"\n"
            "key=3==\"value3\"\n"
            "==\"=\"");
        result = strNew();

        TEST_RESULT_VOID(iniLoad(ioBufferReadNew(iniBuf), testIniLoadCallback, result), "load ini");
        TEST_RESULT_STR_Z(
            result,
            "section1: key1 :\"value1\"\n"
            "section1:key2:\"value2\"\n"
            "section1:key=3=:\"value3\"\n"
            "section1:=:\"=\"\n",
            "    check ini");

        // Two sections
        // -------------------------------------------------------------------------------------------------------------------------
        iniBuf = BUFSTRZ(
            "[section1]\n"
            "[key1=\"value1\"\n"
            "key2=\"value2\"\n"
            "\n"
            "[section2]\n"
            "\n"
            "#key2=\"value2\"");
        result = strNew();

        TEST_RESULT_VOID(iniLoad(ioBufferReadNew(iniBuf), testIniLoadCallback, result), "load ini");
        TEST_RESULT_STR_Z(
            result,
            "section1:[key1:\"value1\"\n"
            "section1:key2:\"value2\"\n"
            "section2:#key2:\"value2\"\n",
            "    check ini");
    }

    // *****************************************************************************************************************************
    if (testBegin("iniNew() and iniFree()"))
    {
        Ini *ini = NULL;

        TEST_ASSIGN(ini, iniNew(), "new ini");
        TEST_RESULT_PTR_NE(ini->store, NULL, "store is set");
        TEST_RESULT_VOID(iniFree(ini), "free ini");
    }

    // *****************************************************************************************************************************
    if (testBegin("iniSet(), iniGet(), iniGetDefault(), iniSectionList(), and iniSectionKeyList()"))
    {
        Ini *ini = NULL;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(ini, iniNew(), "new ini");

            TEST_RESULT_VOID(iniSet(ini, STRDEF("section1"), STRDEF("key1"), STRDEF("11")), "set section, key");
            TEST_RESULT_VOID(iniSet(ini, STRDEF("section1"), STRDEF("key2"), STRDEF("1.234")), "set section, key");

            TEST_RESULT_VOID(iniMove(ini, memContextPrior()), "move ini");
            TEST_RESULT_VOID(iniMove(NULL, memContextPrior()), "move null ini");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_STR_Z(iniGet(ini, STRDEF("section1"), STRDEF("key1")), "11", "get section, key");
        TEST_RESULT_STR_Z(iniGet(ini, STRDEF("section1"), STRDEF("key2")), "1.234", "get section, key");

        TEST_ERROR(iniGet(ini, STRDEF("section2"), STRDEF("key2")), FormatError, "section 'section2', key 'key2' does not exist");

        TEST_RESULT_STR_Z(iniGetDefault(ini, STRDEF("section1"), STRDEF("key1"), NULL), "11", "get section, key, int");
        TEST_RESULT_STR(iniGetDefault(ini, STRDEF("section2"), STRDEF("key2"), NULL), NULL, "get section, key, NULL");
        TEST_RESULT_STR_Z(iniGetDefault(ini, STRDEF("section3"), STRDEF("key3"), STRDEF("true")), "true", "get section, key, bool");

        TEST_RESULT_INT(strLstSize(iniSectionKeyList(ini, STRDEF("bogus"))), 0, "get keys for missing section");
        TEST_RESULT_STRLST_Z(iniSectionKeyList(ini, STRDEF("section1")), "key1\nkey2\n", "get keys for section");

        TEST_RESULT_VOID(iniSet(ini, STRDEF("section2"), STRDEF("key2"), STRDEF("2")), "set section2, key");
        TEST_RESULT_INT(strLstSize(iniSectionList(ini)), 2, "number of sections");
        TEST_RESULT_STRLST_Z(iniSectionList(ini), "section1\nsection2\n", "get sections");

        TEST_RESULT_BOOL(iniSectionKeyIsList(ini, STRDEF("section1"), STRDEF("key1")), false, "single value is not list");
        TEST_RESULT_VOID(iniSet(ini, STRDEF("section2"), STRDEF("key2"), STRDEF("7")), "set section2, key");
        TEST_RESULT_BOOL(iniSectionKeyIsList(ini, STRDEF("section2"), STRDEF("key2")), true, "section2, key2 is a list");
        TEST_RESULT_STRLST_Z(iniGetList(ini, STRDEF("section2"), STRDEF("key2")), "2\n7\n", "get list");
        TEST_RESULT_PTR(iniGetList(ini, STRDEF("section2"), STRDEF("key-missing")), NULL, "get missing list");

        TEST_RESULT_VOID(iniFree(ini), "free ini");
    }

    // *****************************************************************************************************************************
    if (testBegin("iniParse()"))
    {
        Ini *ini = NULL;
        const String *content = NULL;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(iniParse(iniNew(), NULL), "no content");
        TEST_ERROR(
            iniParse(iniNew(), STRDEF("compress=y\n")), FormatError, "key/value found outside of section at line 1: compress=y");
        TEST_ERROR(iniParse(iniNew(), STRDEF("[section\n")), FormatError, "ini section should end with ] at line 1: [section");
        TEST_ERROR(iniParse(iniNew(), STRDEF("[section]\nkey")), FormatError, "missing '=' in key/value at line 2: key");
        TEST_ERROR(iniParse(iniNew(), STRDEF("[section]\n =value")), FormatError, "key is zero-length at line 1: =value");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(ini, iniNew(), "new ini");

        content = STRDEF
        (
            "# Comment\n"
            "[global] \n"
            "compress=y \n"
            "\n"
            " [db]\n"
            "pg1-path = /path/to/pg"
        );

        TEST_RESULT_VOID(iniParse(ini, content), "load ini");

        TEST_RESULT_STR_Z(iniGet(ini, STRDEF("global"), STRDEF("compress")), "y", "get compress");
        TEST_RESULT_STR_Z(iniGet(ini, STRDEF("db"), STRDEF("pg1-path")), "/path/to/pg", "get pg1-path");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
