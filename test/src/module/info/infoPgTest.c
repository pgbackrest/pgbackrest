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
    if (testBegin("infoPg"))
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
            "\n"
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6365925855997464783\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6365925855997464783,\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), bufNewStr(content)), "put ini to file");

        InfoPg *infoPg = NULL;

        TEST_ASSIGN(infoPg, infoPgNew(fileName, true, false, infoPgArchive), "new infoPg - load file");

        TEST_RESULT_INT(lstSize(infoPg->history), 1, "history record added");
        TEST_RESULT_INT(infoPg->indexCurrent, 0, "current index");    }
}
