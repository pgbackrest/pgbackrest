/***********************************************************************************************************************************
Test Ini
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("iniNew() and iniFree()"))
    {
        Ini *ini = NULL;

        TEST_ASSIGN(ini, iniNew(), "new ini");
        TEST_RESULT_PTR_NE(ini->memContext, NULL, "mem context is set");
        TEST_RESULT_PTR_NE(ini->store, NULL, "store is set");
        TEST_RESULT_VOID(iniFree(ini), "free ini");
        TEST_RESULT_VOID(iniFree(NULL), "free null ini");
    }

    // *****************************************************************************************************************************
    if (testBegin("iniSet(), iniGet(), iniGetDefault(), iniSectionList(), and iniSectionKeyList()"))
    {
        Ini *ini = NULL;

        TEST_ASSIGN(ini, iniNew(), "new ini");

        TEST_RESULT_VOID(iniSet(ini, strNew("section1"), strNew("key1"), varNewInt(11)), "set section, key, int");
        TEST_RESULT_VOID(iniSet(ini, strNew("section1"), strNew("key2"), varNewDbl(1.234)), "set section, key, dbl");
        TEST_RESULT_INT(varInt(iniGet(ini, strNew("section1"), strNew("key1"))), 11, "get section, key, int");
        TEST_RESULT_DOUBLE(varDbl(iniGet(ini, strNew("section1"), strNew("key2"))), 1.234, "get section, key, dbl");

        TEST_ERROR(iniGet(ini, strNew("section2"), strNew("key2")), FormatError, "section 'section2', key 'key2' does not exist");

        TEST_RESULT_INT(varInt(iniGetDefault(ini, strNew("section1"), strNew("key1"), NULL)), 11, "get section, key, int");
        TEST_RESULT_PTR(iniGetDefault(ini, strNew("section2"), strNew("key2"), NULL), NULL, "get section, key, NULL");
        TEST_RESULT_BOOL(
                varBool(iniGetDefault(ini, strNew("section3"), strNew("key3"), varNewBool(true))), true, "get section, key, bool");

        TEST_RESULT_INT(strLstSize(iniSectionKeyList(ini, strNew("bogus"))), 0, "get keys for missing section");
        TEST_RESULT_STR(strPtr(strLstJoin(iniSectionKeyList(ini, strNew("section1")), "|")), "key1|key2", "get keys for section");

        TEST_RESULT_VOID(iniSet(ini, strNew("section2"), strNew("key2"), varNewInt(2)), "set section2, key, int");
        TEST_RESULT_INT(strLstSize(iniSectionList(ini)), 2, "number of sections");
        TEST_RESULT_STR(strPtr(strLstJoin(iniSectionList(ini), "|")), "section1|section2", "get sections");

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

        TEST_RESULT_STR(strPtr(varStr(iniGet(ini, strNew("global"), strNew("compress")))), "y", "get compress");
        TEST_RESULT_STR(strPtr(varStr(iniGet(ini, strNew("db"), strNew("pg1-path")))), "/path/to/pg", "get pg1-path");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
