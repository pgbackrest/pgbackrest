/***********************************************************************************************************************************
Test Ini
***********************************************************************************************************************************/
#include "common/io/bufferRead.h"
#include "common/type/buffer.h"
#include "storage/posix/storage.h"

/***********************************************************************************************************************************
Test callback to accumulate ini load results
***********************************************************************************************************************************/
static String *
testIniNextValue(Ini *const ini)
{
    String *const result = strNew();
    const IniValue *value = iniValueNext(ini);

    while (value != NULL)
    {
        strCatFmt(result, "%s:%s:%s\n", strZ(value->section), strZ(value->key), strZ(value->value));
        value = iniValueNext(ini);
    }

    return result;
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("iniNewP() strict, iniValid()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("errors");

        TEST_ERROR(
            testIniNextValue(iniNewP(ioBufferReadNew(BUFSTRDEF("key=value\n")), .strict = true)), FormatError,
            "key/value found outside of section at line 1: key=value");
        TEST_ERROR(
            testIniNextValue(iniNewP(ioBufferReadNew(BUFSTRDEF("\n[]\n")), .strict = true)), FormatError,
            "invalid empty section at line 2: []");
        TEST_ERROR(
            testIniNextValue(iniNewP(ioBufferReadNew(BUFSTRDEF("[section]\nkey=value\n")), .strict = true)), FormatError,
            "invalid JSON value at line 2 'key=value': invalid type at: value");
        TEST_ERROR(
            testIniNextValue(iniNewP(ioBufferReadNew(BUFSTRDEF("[section]\nkey")), .strict = true)), FormatError,
            "missing '=' in key/value at line 2: key");
        TEST_ERROR(
            testIniNextValue(iniNewP(ioBufferReadNew(BUFSTRDEF("[section]\n=\"value\"")), .strict = true)), FormatError,
            "key is zero-length at line 2: =\"value\"");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("empty ini");

        TEST_RESULT_STR_Z(testIniNextValue(iniNewP(ioBufferReadNew(BUFSTRDEF("")), .strict = true)), "", "check");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("one section");

        const Buffer *iniBuf = BUFSTRZ(
            "[section1]\n"
            " key1 =\"value1\"\n"
            "key2=\"value2\"\n"
            "key=3==\"value3\"\n"
            "==\"=\"");

        TEST_RESULT_STR_Z(
            testIniNextValue(iniNewP(ioBufferReadNew(iniBuf), .strict = true)),
            "section1: key1 :\"value1\"\n"
            "section1:key2:\"value2\"\n"
            "section1:key=3=:\"value3\"\n"
            "section1:=:\"=\"\n",
            "check");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("two sections");

        iniBuf = BUFSTRZ(
            "[section1]\n"
            "[key1=\"value1\"\n"
            "key2=\"value2\"\n"
            "\n"
            "[section2]\n"
            "\n"
            "#key2=\"value2\"");

        TEST_RESULT_STR_Z(
            testIniNextValue(iniNewP(ioBufferReadNew(iniBuf), .strict = true)),
            "section1:[key1:\"value1\"\n"
            "section1:key2:\"value2\"\n"
            "section2:#key2:\"value2\"\n",
            "check");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("validate");

        TEST_RESULT_VOID(iniValid(iniNewP(ioBufferReadNew(iniBuf), .strict = true)), "ini valid");
        TEST_ERROR(
            iniValid(iniNewP(ioBufferReadNew(BUFSTRDEF("key=value\n")), .strict = true)), FormatError,
            "key/value found outside of section at line 1: key=value");
    }

    // *****************************************************************************************************************************
    if (testBegin("iniNewP() loose"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("errors");

        TEST_ERROR(
            testIniNextValue(iniNewP(ioBufferReadNew(BUFSTRDEF("compress=y\n")))), FormatError,
            "key/value found outside of section at line 1: compress=y");
        TEST_ERROR(
            testIniNextValue(iniNewP(ioBufferReadNew(BUFSTRDEF("[section\n")))), FormatError,
            "ini section should end with ] at line 1: [section");
        TEST_ERROR(
            testIniNextValue(iniNewP(ioBufferReadNew(BUFSTRDEF("[section]\nkey")))), FormatError,
            "missing '=' in key/value at line 2: key");
        TEST_ERROR(
            testIniNextValue(iniNewP(ioBufferReadNew(BUFSTRDEF("[section]\n =value")))), FormatError,
            "key is zero-length at line 2: =value");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("store and retrieve values");

        const Buffer *iniBuf = BUFSTRDEF(
            "# Comment\n"
            "[global] \n"
            "compress=y \n"
            " repeat = 1 \n"
            "repeat=2\n"
            "\n"
            " [db]\n"
            "pg1-path = /path/to/pg");

        Ini *ini = NULL;
        TEST_ASSIGN(ini, iniNewP(ioBufferReadNew(iniBuf), .store = true), "new ini");
        TEST_RESULT_STR_Z(iniGet(ini, STRDEF("global"), STRDEF("compress")), "y", "ini get");
        TEST_RESULT_STR_Z(iniGet(ini, STRDEF("db"), STRDEF("pg1-path")), "/path/to/pg", "ini get");
        TEST_ERROR(iniGet(ini, STRDEF("sec"), STRDEF("key")), FormatError, "section 'sec', key 'key' does not exist");

        TEST_RESULT_BOOL(iniSectionKeyIsList(ini, STRDEF("global"), STRDEF("repeat")), true, "key is list");
        TEST_RESULT_STRLST_Z(iniGetList(ini, STRDEF("global"), STRDEF("repeat")), "1\n2\n", "key list");
        TEST_RESULT_PTR(iniGetList(ini, STRDEF("globalx"), STRDEF("repeat2")), NULL, "null key list");

        TEST_RESULT_STRLST_Z(iniSectionList(ini), "global\ndb\n", "sections");
        TEST_RESULT_STRLST_Z(iniSectionKeyList(ini, STRDEF("global")), "compress\nrepeat\n", "section keys");
        TEST_RESULT_STRLST_Z(iniSectionKeyList(ini, STRDEF("bogus")), NULL, "empty section keys");

        TEST_RESULT_VOID(iniFree(ini), "ini free");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
