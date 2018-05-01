/***********************************************************************************************************************************
Test Lists
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    // *****************************************************************************************************************************
    if (testBegin("info"))
    {
        // Ini *ini = NULL;
        String *content = NULL;
        String *fileName = strNewFmt("%s/test.ini", testPath());

        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"18a65555903b0e2a3250d141825de809409eb1cf\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.02dev\"\n"
        );

        TEST_RESULT_VOID(storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), bufNewStr(content)), "put ini to file");

        Info *info = NULL;

        TEST_ASSIGN(info, infoNew(fileName, true, false), "new info - load file");

        // CSHANG MUST remove the quotes in the backrestChecksum test after install json parser
        TEST_RESULT_STR(strPtr(info->backrestChecksum), "\"18a65555903b0e2a3250d141825de809409eb1cf\"", "get checksum");
        TEST_RESULT_INT(info->backrestFormat, 5, "get format number");
        TEST_RESULT_STR(strPtr(info->backrestVersion), "2.02dev", "get version");
    }
}
