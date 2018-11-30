/***********************************************************************************************************************************
Test Backup Info Handler
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    // Initialize test variables
    //--------------------------------------------------------------------------------------------------------------------------
    String *content = NULL;
    String *fileName = strNewFmt("%s/test.ini", testPath());
    InfoBackup *infoBackup = NULL;

    // *****************************************************************************************************************************
    if (testBegin("infoBackupNew(), infoBackupCurrentSet(), infoBackupCurrentKeyGet(), infoBackupCheckPg(), infoBackupFree()"))
    {
        // File missing, ignoreMissing=false -- error
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            infoBackupNew(storageLocal(), fileName, false, cipherTypeNone, NULL), FileOpenError,
            "unable to load info file '%s/test.ini' or '%s/test.ini.copy':\n"
            "FileMissingError: unable to open '%s/test.ini' for read: [2] No such file or directory\n"
            "FileMissingError: unable to open '%s/test.ini.copy' for read: [2] No such file or directory\n"
            "HINT: backup.info does not exist and is required to perform a backup.\n"
            "HINT: has a stanza-create been performed?",
            testPath(), testPath(), testPath(), testPath());

        // File exists, ignoreMissing=false, no backup:current section
        //--------------------------------------------------------------------------------------------------------------------------
        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"5c17df9523543f5283efdc3c5aa7eb933c63ea0b\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.04\"\n"
            "\n"
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), bufNewStr(content)), "put backup info to file");

        TEST_ASSIGN(infoBackup, infoBackupNew(storageLocal(), fileName, false, cipherTypeNone, NULL), "    new backup info");
        TEST_RESULT_PTR(infoBackupPg(infoBackup), infoBackup->infoPg, "    infoPg set");
        TEST_RESULT_PTR(infoBackup->backupCurrent, NULL, "    backupCurrent NULL");
        TEST_RESULT_PTR(infoBackupCurrentKeyGet(infoBackup),  NULL, "    infoBackupCurrentKeyGet returns NULL");

        // infoBackupCheckPg
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(infoBackupCheckPg(infoBackup, 90400, 6569239123849665679, 201409291, 942), 1, "check PG data");

        TEST_ERROR_FMT(
            infoBackupCheckPg(infoBackup, 90500, 6569239123849665679, 201409291, 942), BackupMismatchError,
            "database version = 9.5, system-id 6569239123849665679 does not match "
            "backup version = 9.4, system-id = 6569239123849665679\n"
            "HINT: is this the correct stanza?");

        TEST_ERROR_FMT(
            infoBackupCheckPg(infoBackup, 90400, 6569239123849665999, 201409291, 942), BackupMismatchError,
            "database version = 9.4, system-id 6569239123849665999 does not match "
            "backup version = 9.4, system-id = 6569239123849665679\n"
            "HINT: is this the correct stanza?");

        TEST_ERROR_FMT(
            infoBackupCheckPg(infoBackup, 90400, 6569239123849665679, 201409291, 941), BackupMismatchError,
            "database control-version = 941, catalog-version 201409291"
            " does not match backup control-version = 942, catalog-version = 201409291\n"
            "HINT: this may be a symptom of database or repository corruption!");

        TEST_ERROR_FMT(
            infoBackupCheckPg(infoBackup, 90400, 6569239123849665679, 201509291, 942), BackupMismatchError,
            "database control-version = 942, catalog-version 201509291"
            " does not match backup control-version = 942, catalog-version = 201409291\n"
            "HINT: this may be a symptom of database or repository corruption!");

        // Free
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(infoBackupFree(infoBackup), "infoBackupFree() - free backup info");
        TEST_RESULT_VOID(infoBackupFree(NULL), "    NULL ptr");
    }
    // *****************************************************************************************************************************
    if (testBegin("infoBackupCurrentGet(), infoBackupCurrentKeyGet()"))
    {
        // File exists, ignoreMissing=false, backup:current section exists
        //--------------------------------------------------------------------------------------------------------------------------
        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"1d29626cbe8f405074d325c586d70a2d87e16bad\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.04\"\n"
            "\n"
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[backup:current]\n"
            "20161219-212741F={\"backrest-format\":5,\"backrest-version\":\"2.04\","
            "\"backup-archive-start\":\"00000007000000000000001C\",\"backup-archive-stop\":\"00000007000000000000001C\","
            "\"backup-info-repo-size\":3159776,\"backup-info-repo-size-delta\":3159776,\"backup-info-size\":26897030,"
            "\"backup-info-size-delta\":26897030,\"backup-timestamp-start\":1482182846,\"backup-timestamp-stop\":1482182861,"
            "\"backup-type\":\"full\",\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,"
            "\"option-backup-standby\":false,\"option-checksum-page\":false,\"option-compress\":true,\"option-hardlink\":false,"
            "\"option-online\":true}\n"
            "20161219-212741F_20161219-212803D={\"backrest-format\":5,\"backrest-version\":\"2.04\","
            "\"backup-archive-start\":\"00000008000000000000001E\",\"backup-archive-stop\":\"00000008000000000000001E\","
            "\"backup-info-repo-size\":3159811,\"backup-info-repo-size-delta\":15765,\"backup-info-size\":26897030,"
            "\"backup-info-size-delta\":163866,\"backup-prior\":\"20161219-212741F\",\"test-reference\":[2,3,1],"
            "\"backup-timestamp-start\":1482182877,\"backup-timestamp-stop\":1482182883,\"backup-type\":\"diff\",\"db-id\":1,"
            "\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":false,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "20161219-212741F_20161219-212918I={\"backrest-format\":5,\"backrest-version\":\"2.04\","
            "\"backup-archive-start\":\"00000008000000000000001E\",\"backup-archive-stop\":\"00000008000000000000001E\","
            "\"backup-info-repo-size\":3159811,\"backup-info-repo-size-delta\":15765,\"backup-info-size\":26897030,"
            "\"backup-info-size-delta\":163866,\"backup-prior\":\"20161219-212741F\",\"backup-reference\":[\"20161219-212741F\","
            "\"20161219-212741F_20161219-212803D\"],\"backup-type\":\"incr\",\"db-id\":1,"
            "\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":false,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), bufNewStr(content)), "put backup info current to file");

        TEST_ASSIGN(infoBackup, infoBackupNew(storageLocal(), fileName, false, cipherTypeNone, NULL), "    new backup info");
        TEST_RESULT_PTR_NE(infoBackup->backupCurrent, NULL, "    backupCurrent not NULL");

        TEST_RESULT_PTR(infoBackupCurrentGet(infoBackup, strNew("section-not-exist"), strNew("backup-timestamp-start")),
            NULL, "empty section returns NULL");

        StringList *backupLabelList = infoBackupCurrentKeyGet(infoBackup);
        String *backupLabel = strLstGet(backupLabelList, 0);

        TEST_RESULT_STR(strPtr(backupLabel), "20161219-212741F", "full backup label");
        TEST_RESULT_INT(varIntForce(infoBackupCurrentGet(infoBackup, backupLabel, strNew("backrest-format"))), 5,
            "    check first kv");
        TEST_RESULT_STR(strPtr(varStrForce(infoBackupCurrentGet(infoBackup, backupLabel, strNew("backup-type")))), "full",
            "    check string");
        TEST_RESULT_INT(varIntForce(infoBackupCurrentGet(infoBackup, backupLabel, strNew("db-id"))), 1,
            "    check int");
        TEST_RESULT_BOOL(varBoolForce(infoBackupCurrentGet(infoBackup, backupLabel, strNew("option-archive-check"))), true,
            "    check bool");

        backupLabel = strLstGet(backupLabelList, 1);
        VariantList *testArray = varVarLst(infoBackupCurrentGet(infoBackup, backupLabel, strNew("test-reference")));

        TEST_RESULT_INT(varLstSize(testArray), 3, "    check int array size");
        TEST_RESULT_INT(varIntForce(varLstGet(testArray, 0)), 2, "        first array element");
        TEST_RESULT_INT(varIntForce(varLstGet(testArray, 1)), 3, "        middle array element");
        TEST_RESULT_INT(varIntForce(varLstGet(testArray, 2)), 1, "        last array element");
        TEST_RESULT_INT(varIntForce(infoBackupCurrentGet(infoBackup, backupLabel, strNew("backup-timestamp-start"))), 1482182877,
            "    check int after array");

        backupLabel = strLstGet(backupLabelList, 2);
        backupLabelList = strLstNewVarLst(varVarLst(infoBackupCurrentGet(infoBackup, backupLabel, strNew("backup-reference"))));

        TEST_RESULT_INT(strLstSize(backupLabelList), 2, "    check string array size");
        TEST_RESULT_STR(strPtr(strLstGet(backupLabelList, 0)), "20161219-212741F",
            "        first array element");
        TEST_RESULT_STR(strPtr(strLstGet(backupLabelList, 1)), "20161219-212741F_20161219-212803D",
            "        last array element");
        TEST_RESULT_STR(strPtr(varStrForce(infoBackupCurrentGet(infoBackup, backupLabel, strNew("backup-type")))), "incr",
            "    check string after array");
    }
}
