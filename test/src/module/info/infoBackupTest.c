/***********************************************************************************************************************************
Test Backup Info Handler
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    // *****************************************************************************************************************************
    if (testBegin("infoBackupNew(), infoBackupCheckPg(), infoBackupFree()"))
    {
        // Initialize test variables
        //--------------------------------------------------------------------------------------------------------------------------
        String *content = NULL;
        String *fileName = strNewFmt("%s/test.ini", testPath());

        // File missing, ignoreMissing=false -- error
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            infoBackupNew(storageLocal(), fileName, false), FileMissingError,
            "unable to open %s/test.ini or %s/test.ini.copy\n"
            "HINT: backup.info does not exist and is required to perform a backup.\n"
            "HINT: has a stanza-create been performed?",
            testPath(), testPath());

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

        InfoBackup *infoBackup = NULL;

        TEST_ASSIGN(infoBackup, infoBackupNew(storageLocal(), fileName, false), "    new backup info");
        TEST_RESULT_PTR(infoBackupPg(infoBackup), infoBackup->infoPg, "    infoPg set");
        TEST_RESULT_PTR(infoBackup->backupCurrentKey, NULL, "    backupCurrentKey NULL");

        // File exists, ignoreMissing=false, backup:current section exists
        //--------------------------------------------------------------------------------------------------------------------------
        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"e7d8fd004c1915dacedc07b28a0814d012de8dd1\"\n"
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
            "20161219-212741F_20161219-212803I={\"backrest-format\":5,\"backrest-version\":\"2.04\","
            "\"backup-archive-start\":\"00000008000000000000001E\",\"backup-archive-stop\":\"00000008000000000000001E\","
            "\"backup-info-repo-size\":3159811,\"backup-info-repo-size-delta\":15765,\"backup-info-size\":26897030,"
            "\"backup-info-size-delta\":163866,\"backup-prior\":\"20161219-212741F\",\"backup-reference\":[\"20161219-212741F\"],"
            "\"backup-timestamp-start\":1482182877,\"backup-timestamp-stop\":1482182883,\"backup-type\":\"incr\",\"db-id\":1,"
            "\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":false,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), fileName), bufNewStr(content)), "put backup info current to file");

        infoBackup = NULL;

        TEST_ASSIGN(infoBackup, infoBackupNew(storageLocal(), fileName, false), "    new backup info");
        TEST_RESULT_PTR_NE(infoBackup->backupCurrentKey, NULL, "    backupCurrentKey not NULL");

        // for (unsigned int keyIdx = 0;  keyIdx < strLstSize(infoBackup->backupCurrentKey); keyIdx++)
        //
        //
        // TEST_RESULT_STR(strPtr(infoBackupId(info)), "9.4-1", "    archiveId set");
        //
        //
        // // Check PG version
        // //--------------------------------------------------------------------------------------------------------------------------
        // TEST_RESULT_VOID(infoBackupCheckPg(info, 90400, 6569239123849665679), "check PG current");
        // TEST_ERROR(infoBackupCheckPg(info, 1, 6569239123849665679), ArchiveMismatchError,
        //     "WAL segment version 1 does not match archive version 90400"
        //     "\nHINT: are you archiving to the correct stanza?");
        // TEST_ERROR(infoBackupCheckPg(info, 90400, 1), ArchiveMismatchError,
        //     "WAL segment system-id 1 does not match archive system-id 6569239123849665679"
        //     "\nHINT: are you archiving to the correct stanza?");
        // TEST_ERROR(infoBackupCheckPg(info, 1, 1), ArchiveMismatchError,
        //     "WAL segment version 1 does not match archive version 90400"
        //     "\nWAL segment system-id 1 does not match archive system-id 6569239123849665679"
        //     "\nHINT: are you archiving to the correct stanza?");
        //
        // // Free
        // //--------------------------------------------------------------------------------------------------------------------------
        // TEST_RESULT_VOID(infoBackupFree(info), "infoBackupFree() - free archive info");
        // TEST_RESULT_VOID(infoBackupFree(NULL), "    NULL ptr");
    }
}
