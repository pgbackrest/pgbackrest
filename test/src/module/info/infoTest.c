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
// CSHANG In the info files, the string values (and in the history, the keys) are enclosed in quotes, but when I store it that way,
// then the test returns the value with the quotes, which is not the way we usually have to test for these things.
// db-version="9.4"
//
// [db:history]
// 1={"db-id":6548821124626494332,"db-version":"9.4"}

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

        Info *info = NULL;

        TEST_ASSIGN(info, infoNew(fileName, true, false), "new info - load file");
// CSHANG MUST USE TRIM AND PUT A NOTE THAT IT WILL NEED TO BE REMOVED WHEN WE HAVE PROPER JSON CONVERTING
        TEST_RESULT_STR(strPtr(info->backrestChecksum), "\"18a65555903b0e2a3250d141825de809409eb1cf\"", "get checksum");
    }
}
