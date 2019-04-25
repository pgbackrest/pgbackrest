/***********************************************************************************************************************************
Test Command Control
***********************************************************************************************************************************/
#include "common/harnessConfig.h"
#include "storage/driver/posix/storage.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create the repo directories
    String *repoPath = strNewFmt("%s/repo", testPath());
    String *archivePath = strNewFmt("%s/%s", strPtr(repoPath), "archive");
    String *backupPath = strNewFmt("%s/%s", strPtr(repoPath), "backup");
    String *stanza = strNew("db");
    String *archiveStanzaPath = strNewFmt("%s/%s", strPtr(archivePath), strPtr(stanza));
    String *backupStanzaPath = strNewFmt("%s/%s", strPtr(backupPath), strPtr(stanza));

    // *****************************************************************************************************************************
    if (testBegin("cmdExpire()"))
    {
        storagePathCreateNP(storageLocalWrite(), archivePath);
        storagePathCreateNP(storageLocalWrite(), backupPath);
        storagePathCreateNP(storageLocalWrite(), backupStanzaPath);
        storagePathCreateNP(storageLocalWrite(), archiveStanzaPath);

        // Create backup directories and manifest files
        String *full1 = strNewFmt("%s/%s", strPtr(backupStanzaPath), "20181119-152138F");
        String *full1Diff1 = strNewFmt("%s/%s", strPtr(backupStanzaPath), "20181119-152138F_20181119-152152D");
        String *full1Incr1 = strNewFmt("%s/%s", strPtr(backupStanzaPath), "20181119-152138F_20181119-152155I");
        String *full1Diff2 = strNewFmt("%s/%s", strPtr(backupStanzaPath), "20181119-152138F_20181119-152600D");
        String *full2 = strNewFmt("%s/%s", strPtr(backupStanzaPath), "20181119-152800F");
        String *full3 = strNewFmt("%s/%s", strPtr(backupStanzaPath), "20181119-152900F");

        storagePathCreateNP(storageLocalWrite(), full1);
        storagePathCreateNP(storageLocalWrite(), full1Diff1);
        storagePathCreateNP(storageLocalWrite(), full1Incr1);
        storagePathCreateNP(storageLocalWrite(), full1Diff2);
        storagePathCreateNP(storageLocalWrite(), full2);
        storagePathCreateNP(storageLocalWrite(), full3);

        system(strPtr(strNewFmt("touch %s/%s", strPtr(full1), INFO_MANIFEST_FILE)));
        system(strPtr(strNewFmt("touch %s/%s", strPtr(full1), INFO_MANIFEST_FILE ".copy")));
        system(strPtr(strNewFmt("touch %s/%s", strPtr(full1Diff1), INFO_MANIFEST_FILE)));
        system(strPtr(strNewFmt("touch %s/%s", strPtr(full1Incr1), INFO_MANIFEST_FILE ".copy")));
        system(strPtr(strNewFmt("touch %s/%s", strPtr(full1Diff2), INFO_MANIFEST_FILE)));
        system(strPtr(strNewFmt("touch %s/%s", strPtr(full1Diff2), INFO_MANIFEST_FILE ".copy")));
        system(strPtr(strNewFmt("touch %s/%s", strPtr(full2), INFO_MANIFEST_FILE)));
        system(strPtr(strNewFmt("touch %s/%s", strPtr(full2), INFO_MANIFEST_FILE ".copy")));

        String *content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"03b699df7362ce202995231c9060c88e6939e481\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.08dev\"\n"
            "\n"
            "[backup:current]\n"
            "20181119-152138F={"
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
            "\"backup-archive-start\":\"000000010000000000000002\",\"backup-archive-stop\":\"000000010000000000000002\","
            "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
            "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
            "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
            "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "20181119-152138F_20181119-152152D={"
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000003\","
            "\"backup-archive-stop\":\"000000010000000000000003\",\"backup-info-repo-size\":2369186,"
            "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
            "\"backup-prior\":\"20181119-152138F\",\"backup-reference\":[\"20181119-152138F\"],"
            "\"backup-timestamp-start\":1542640912,\"backup-timestamp-stop\":1542640915,\"backup-type\":\"diff\","
            "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "20181119-152138F_20181119-152155I={"
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000004\","
            "\"backup-archive-stop\":\"000000010000000000000004\",\"backup-info-repo-size\":2369186,"
            "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
            "\"backup-prior\":\"20181119-152138F_20181119-152152D\","
            "\"backup-reference\":[\"20181119-152138F\",\"20181119-152138F_20181119-152152D\"],"
            "\"backup-timestamp-start\":1542640912,\"backup-timestamp-stop\":1542640915,\"backup-type\":\"incr\","
            "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "20181119-152138F_20181119-152600D={"
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000005\","
            "\"backup-archive-stop\":\"000000010000000000000005\",\"backup-info-repo-size\":2369186,"
            "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
            "\"backup-prior\":\"20181119-152138F\",\"backup-reference\":[\"20181119-152138F\"],"
            "\"backup-timestamp-start\":1542640912,\"backup-timestamp-stop\":1542640915,\"backup-type\":\"diff\","
            "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "20181119-152800F={"
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
            "\"backup-archive-start\":\"000000010000000000000006\",\"backup-archive-stop\":\"000000010000000000000006\","
            "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
            "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
            "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
            "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "20181119-152900F={"
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
            "\"backup-archive-start\":\"000000010000000000000007\",\"backup-archive-stop\":\"000000010000000000000007\","
            "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
            "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
            "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
            "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "20181119-152900F_20181119-152600D={"
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000008\","
            "\"backup-archive-stop\":\"000000010000000000000008\",\"backup-info-repo-size\":2369186,"
            "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
            "\"backup-prior\":\"20181119-152138F\",\"backup-reference\":[\"20181119-152900F\"],"
            "\"backup-timestamp-start\":1542640912,\"backup-timestamp-stop\":1542640915,\"backup-type\":\"diff\","
            "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "\n"
            "[db]\n"
            "db-catalog-version=201510051\n"
            "db-control-version=942\n"
            "db-id=2\n"
            "db-system-id=6626363367545678089\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6625592122879095702,"
                "\"db-version\":\"9.4\"}\n"
            "2={\"db-catalog-version\":201510051,\"db-control-version\":942,\"db-system-id\":6626363367545678089,"
                "\"db-version\":\"9.5\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), strNewFmt("%s/backup.info", strPtr(backupStanzaPath))),
                BUFSTR(content)), "put backup info to file");

        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"075a202d42c3b6a0257da5f73a68fa77b342f777\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.08dev\"\n"
            "\n"
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6626363367545678089\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6625592122879095702,"
                "\"db-version\":\"9.4\"}\n"
            "2={\"db-id\":6626363367545678089,"
                "\"db-version\":\"9.5\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), strNewFmt("%s/archive.info", strPtr(archiveStanzaPath))),
                BUFSTR(content)), "put archive info to file");

        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAdd(argList, strNewFmt("--stanza=%s", strPtr(stanza)));
        strLstAdd(argList, strNewFmt("--repo-path=%s", strPtr(repoPath)));
        strLstAddZ(argList, "--repo1-retention-full=1");
        strLstAddZ(argList, "expire");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

// CSHANG First test should be Nothing is expired? Or maybe that should be the last
        TEST_RESULT_VOID(cmdExpire(), "expire");

// CSHANG Test that only the direcories matching the regex of full, incr or diff are removed - maybe create a file with a FULL exp then _save then make sure it is not deleted but the full fir of the same name is. Also, confirm recursion

    }

    // *****************************************************************************************************************************
    if (testBegin("sortArchiveId()"))
    {
        StringList *list = strLstNew();

        strLstAddZ(list, "11-10");
        strLstAddZ(list, "10-4");
        strLstAddZ(list, "9.4-2");
        strLstAddZ(list, "9.6-1");

        TEST_RESULT_STR(strPtr(strLstJoin(sortArchiveId(list, sortOrderAsc), ", ")), "9.6-1, 9.4-2, 10-4, 11-10", "sort ascending");
        TEST_RESULT_STR(
            strPtr(strLstJoin(sortArchiveId(list, sortOrderDesc), ", ")), "11-10, 10-4, 9.4-2, 9.6-1", "sort descending");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
