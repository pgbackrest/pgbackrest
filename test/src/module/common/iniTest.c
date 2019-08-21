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
testIniLoadCallback(void *data, const String *section, const String *key, const String *value)
{
    strCatFmt((String *)data, "%s:%s:%s\n", strPtr(section), strPtr(key), strPtr(value));
}

/***********************************************************************************************************************************
Test callback to count ini load results
***********************************************************************************************************************************/
static void
testIniLoadCountCallback(void *data, const String *section, const String *key, const String *value)
{
    (*(unsigned int *)data)++;
    (void)section;
    (void)key;
    (void)value;
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storagePosixNew(
        strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

    // *****************************************************************************************************************************
    if (testBegin("iniLoad()"))
    {
        // Empty file
        // -------------------------------------------------------------------------------------------------------------------------
        const Buffer *iniBuf = bufNew(0);
        String *result = strNew("");

        TEST_RESULT_VOID(iniLoad(ioBufferReadNew(iniBuf), testIniLoadCallback, result), "load ini");
        TEST_RESULT_STR(strPtr(result), "", "    check ini");

        // Invalid section
        // -------------------------------------------------------------------------------------------------------------------------
        iniBuf = BUFSTRZ(
            "  [section  ");

        TEST_ERROR(
            iniLoad(ioBufferReadNew(iniBuf), testIniLoadCallback, result), FormatError,
            "ini section should end with ] at line 1: [section");

        // Key outside of section
        // -------------------------------------------------------------------------------------------------------------------------
        iniBuf = BUFSTRZ(
            "key=value\n");

        TEST_ERROR(
            iniLoad(ioBufferReadNew(iniBuf), testIniLoadCallback, result), FormatError,
            "key/value found outside of section at line 1: key=value");

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
            "=value");

        TEST_ERROR(
            iniLoad(ioBufferReadNew(iniBuf), testIniLoadCallback, result), FormatError,
            "key is zero-length at line 1: =value");

        // One section
        // -------------------------------------------------------------------------------------------------------------------------
        iniBuf = BUFSTRZ(
            "# comment\n"
            "[section1]\n"
            "key1=value1\n"
            "key2=value2");
        result = strNew("");

        TEST_RESULT_VOID(iniLoad(ioBufferReadNew(iniBuf), testIniLoadCallback, result), "load ini");
        TEST_RESULT_STR(
            strPtr(result),
            "section1:key1:value1\n"
                "section1:key2:value2\n",
            "    check ini");

        // Two sections
        // -------------------------------------------------------------------------------------------------------------------------
        iniBuf = BUFSTRZ(
            "# comment\n"
            "[section1]\n"
            "key1=value1\n"
            "key2=value2\n"
            "\n"
            "[section2]\n"
            "key1=\n"
            "\n"
            "key2=value2");
        result = strNew("");

        TEST_RESULT_VOID(iniLoad(ioBufferReadNew(iniBuf), testIniLoadCallback, result), "load ini");
        TEST_RESULT_STR(
            strPtr(result),
            "section1:key1:value1\n"
                "section1:key2:value2\n"
                "section2:key1:\n"
                "section2:key2:value2\n",
            "    check ini");
    }

    // *****************************************************************************************************************************
    if (testBegin("iniNew() and iniFree()"))
    {
        Ini *ini = NULL;

        TEST_ASSIGN(ini, iniNew(), "new ini");
        TEST_RESULT_PTR_NE(ini->memContext, NULL, "mem context is set");
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

            TEST_RESULT_VOID(iniSet(ini, strNew("section1"), strNew("key1"), strNew("11")), "set section, key");
            TEST_RESULT_VOID(iniSet(ini, strNew("section1"), strNew("key2"), strNew("1.234")), "set section, key");

            TEST_RESULT_VOID(iniMove(ini, MEM_CONTEXT_OLD()), "move ini");
            TEST_RESULT_VOID(iniMove(NULL, MEM_CONTEXT_OLD()), "move null ini");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_STR(strPtr(iniGet(ini, strNew("section1"), strNew("key1"))), "11", "get section, key");
        TEST_RESULT_STR(strPtr(iniGet(ini, strNew("section1"), strNew("key2"))), "1.234", "get section, key");

        TEST_ERROR(iniGet(ini, strNew("section2"), strNew("key2")), FormatError, "section 'section2', key 'key2' does not exist");

        TEST_RESULT_STR(strPtr(iniGetDefault(ini, strNew("section1"), strNew("key1"), NULL)), "11", "get section, key, int");
        TEST_RESULT_PTR(iniGetDefault(ini, strNew("section2"), strNew("key2"), NULL), NULL, "get section, key, NULL");
        TEST_RESULT_STR(
                strPtr(iniGetDefault(ini, strNew("section3"), strNew("key3"), strNew("true"))), "true", "get section, key, bool");

        TEST_RESULT_INT(strLstSize(iniSectionKeyList(ini, strNew("bogus"))), 0, "get keys for missing section");
        TEST_RESULT_STR(strPtr(strLstJoin(iniSectionKeyList(ini, strNew("section1")), "|")), "key1|key2", "get keys for section");

        TEST_RESULT_VOID(iniSet(ini, strNew("section2"), strNew("key2"), strNew("2")), "set section2, key");
        TEST_RESULT_INT(strLstSize(iniSectionList(ini)), 2, "number of sections");
        TEST_RESULT_STR(strPtr(strLstJoin(iniSectionList(ini), "|")), "section1|section2", "get sections");

        TEST_RESULT_BOOL(iniSectionKeyIsList(ini, strNew("section1"), strNew("key1")), false, "single value is not list");
        TEST_RESULT_VOID(iniSet(ini, strNew("section2"), strNew("key2"), strNew("7")), "set section2, key");
        TEST_RESULT_BOOL(iniSectionKeyIsList(ini, strNew("section2"), strNew("key2")), true, "section2, key2 is a list");
        TEST_RESULT_STR(strPtr(strLstJoin(iniGetList(ini, strNew("section2"), strNew("key2")), "|")), "2|7", "get list");
        TEST_RESULT_STR(iniGetList(ini, strNew("section2"), strNew("key-missing")), NULL, "get missing list");

        TEST_RESULT_VOID(iniFree(ini), "free ini");
    }

    // *****************************************************************************************************************************
    if (testBegin("iniParse()"))
    {
        Ini *ini = NULL;
        String *content = NULL;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(iniParse(iniNew(), NULL), "no content");
        TEST_ERROR(
            iniParse(iniNew(), strNew("compress=y\n")), FormatError, "key/value found outside of section at line 1: compress=y");
        TEST_ERROR(iniParse(iniNew(), strNew("[section\n")), FormatError, "ini section should end with ] at line 1: [section");
        TEST_ERROR(iniParse(iniNew(), strNew("[section]\nkey")), FormatError, "missing '=' in key/value at line 2: key");
        TEST_ERROR(iniParse(iniNew(), strNew("[section]\n =value")), FormatError, "key is zero-length at line 1: =value");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(ini, iniNew(), "new ini");

        content = strNew
        (
            "# Comment\n"
            "[global] \n"
            "compress=y \n"
            "\n"
            " [db]\n"
            "pg1-path = /path/to/pg"
        );

        TEST_RESULT_VOID(iniParse(ini, content), "load ini");

        TEST_RESULT_STR(strPtr(iniGet(ini, strNew("global"), strNew("compress"))), "y", "get compress");
        TEST_RESULT_STR(strPtr(iniGet(ini, strNew("db"), strNew("pg1-path"))), "/path/to/pg", "get pg1-path");
    }

    // *****************************************************************************************************************************
    if (testBegin("iniSave()"))
    {
        Ini *ini = iniNew();
        iniSet(ini, strNew("section2"), strNew("key1"), strNew("value1"));
        iniSet(ini, strNew("section1"), strNew("key2"), strNew("value2"));
        iniSet(ini, strNew("section1"), strNew("key1"), strNew("value1"));

        StorageWrite *write = storageNewWriteNP(storageTest, strNew("test.ini"));
        TEST_RESULT_VOID(iniSave(ini, storageWriteIo(write)), "save ini");

        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storageTest, strNew("test.ini"))))),
            "[section1]\n"
                "key1=value1\n"
                "key2=value2\n"
                "\n"
                "[section2]\n"
                "key1=value1\n",
            "check ini");
    }

    // *****************************************************************************************************************************
    if (testBegin("iniLoad() performance"))
    {
        String *iniStr = strNew("[section1]\n");
        unsigned int iniMax = 1000;

        // /backrest/test/test.pl --vm=u18 --smart --no-package --module=common --test=ini --run=5 --no-cleanup --vm-out
        // BASELINE PERF ON DESKTOP 50784ms

        for (unsigned int keyIdx = 0; keyIdx < iniMax; keyIdx++)
            strCatFmt(iniStr, "key%u=value%u\n", keyIdx, keyIdx);

        TEST_LOG_FMT("ini size is %zu", strSize(iniStr));

        TimeMSec timeBegin = timeMSec();
        unsigned int iniTotal = 0;

        TEST_RESULT_VOID(iniLoad(ioBufferReadNew(BUFSTR(iniStr)), testIniLoadCountCallback, &iniTotal), "parse ini");
        TEST_LOG_FMT("parse completed in %ums", (unsigned int)(timeMSec() - timeBegin));
        TEST_RESULT_INT(iniTotal, iniMax, "    check ini total");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
