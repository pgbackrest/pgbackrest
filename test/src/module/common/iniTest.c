/***********************************************************************************************************************************
Test Ini
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    // *****************************************************************************************************************************
    if (testBegin("iniNew() and iniFree()"))
    {
        Ini *ini = NULL;

        TEST_ASSIGN(ini, iniNew(), "new ini");
        TEST_RESULT_PTR_NE(ini->memContext, NULL, "mem context is set");
        TEST_RESULT_PTR_NE(ini->store, NULL, "stores is set");
        TEST_RESULT_VOID(iniFree(ini), "free ini");
    }

    // *****************************************************************************************************************************
    if (testBegin("iniSet(), iniGet(), iniGetDefault(), and iniSectionKeyList()"))
    {
        Ini *ini = NULL;

        TEST_ASSIGN(ini, iniNew(), "new ini");

        TEST_RESULT_VOID(iniSet(ini, strNew("section1"), strNew("key1"), varNewInt(11)), "set section, key, int");
        TEST_RESULT_VOID(iniSet(ini, strNew("section1"), strNew("key2"), varNewDbl(1.234)), "set section, key, dbl");
        TEST_RESULT_INT(varInt(iniGet(ini, strNew("section1"), strNew("key1"))), 11, "get section, key, int");
        TEST_RESULT_DOUBLE(varDbl(iniGet(ini, strNew("section1"), strNew("key2"))), 1.234, "get section, key, dbl");

        TEST_ERROR(iniGet(ini, strNew("section2"), strNew("key2")), FormatError, "section 'section2', key 'key2' does not exist");

        TEST_RESULT_PTR(iniGetDefault(ini, strNew("section2"), strNew("key2"), NULL), NULL, "get section, key, NULL");
        TEST_RESULT_BOOL(
                varBool(iniGetDefault(ini, strNew("section3"), strNew("key3"), varNewBool(true))), true, "get section, key, bool");

        TEST_RESULT_INT(strLstSize(iniSectionKeyList(ini, strNew("bogus"))), 0, "get keys for missing section");
        TEST_RESULT_STR(strPtr(strLstJoin(iniSectionKeyList(ini, strNew("section1")), "|")), "key1|key2", "get keys for section");

        TEST_RESULT_VOID(iniFree(ini), "free ini");
    }

    // *****************************************************************************************************************************
    if (testBegin("iniParse() and iniLoad()"))
    {
        Ini *ini = NULL;
        String *content = NULL;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(
            iniParse(iniNew(), strNew("compress=y\n")), FormatError, "key/value found outside of section at line 1: compress=y");
        TEST_ERROR(iniParse(iniNew(), strNew("[section\n")), FormatError, "ini section should end with ] at line 1: [section");
        TEST_ERROR(iniParse(iniNew(), strNew("[section]\nkey")), FormatError, "missing '=' in key/value at line 2: key");
        TEST_ERROR(iniParse(iniNew(), strNew("[section]\n =value")), FormatError, "key is zero-length at line 1: =value");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(ini, iniNew(), "new ini");

        content = strNew
        (
            "[global] \n"
            "compress=y \n"
            "\n"
            " [db]\n"
            "pg1-path = /path/to/pg"
        );

        TEST_RESULT_VOID(iniParse(ini, content), "load ini");

        TEST_RESULT_STR(strPtr(varStr(iniGet(ini, strNew("global"), strNew("compress")))), "y", "get compress");
        TEST_RESULT_STR(strPtr(varStr(iniGet(ini, strNew("db"), strNew("pg1-path")))), "/path/to/pg", "get pg1-path");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(ini, iniNew(), "new ini");
        String *fileName = strNewFmt("%s/test.ini", testPath());

        content = strNew
        (
            "# Comment\n"
            " [global]\n"
            "           \n"
            " compress= y \n"
            "[db]\t\r\n"
            " pg1-path =/path/to/pg\n"
            "\n"
        );

        TEST_RESULT_VOID(storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), bufNewStr(content)), "put ini to file");
        TEST_RESULT_VOID(iniLoad(ini, fileName), "load ini from file");

        TEST_RESULT_STR(strPtr(varStr(iniGet(ini, strNew("global"), strNew("compress")))), "y", "get compress");
        TEST_RESULT_STR(strPtr(varStr(iniGet(ini, strNew("db"), strNew("pg1-path")))), "/path/to/pg", "get pg1-path");
    }
}
