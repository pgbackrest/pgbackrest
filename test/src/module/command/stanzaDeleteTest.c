/***********************************************************************************************************************************
Test Stanza Delete Command
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "command/stanza/stanzaCreate.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storagePosixNew(
        strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

    String *stanza = strNew("db");
    String *stanzaOther = strNew("otherstanza");

    StringList *argListBase = strLstNew();
    strLstAddZ(argListBase, "pgbackrest");
    strLstAdd(argListBase, strNewFmt("--repo1-path=%s/repo", testPath()));

    // *****************************************************************************************************************************
    if (testBegin("cmdStanzaDelete(), stanzaDelete()"))
    {
// CSHANG lockStop (and lockStart) are not yet implemented in c but should probably have tests here to issue lockStop instead of just manually creating it?
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        strLstAdd(argList,  strNewFmt("--stanza=%s", strPtr(stanzaOther)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/%s", testPath(), strPtr(stanzaOther)));
        strLstAddZ(argList, "stanza-create");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Create pg_control for stanza-create
        storagePutNP(
            storageNewWriteNP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(stanzaOther))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 6569239123849665679}));

        TEST_RESULT_VOID(cmdStanzaCreate(), "create a stanza that will not be deleted");

        argList = strLstDup(argListBase);
        strLstAdd(argList,  strNewFmt("--stanza=%s", strPtr(stanza)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/%s", testPath(), strPtr(stanza)));
        strLstAddZ(argList, "stanza-delete");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // stanza already deleted
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete - success on stanza does not exist");
        TEST_RESULT_BOOL(stanzaDelete(storageRepoWrite(), NULL, NULL), true, "    archiveList=NULL, backupList=NULL");
        TEST_RESULT_BOOL(stanzaDelete(storageRepoWrite(), strLstNew(), NULL), true, "    archiveList=0, backupList=NULL");
        TEST_RESULT_BOOL(stanzaDelete(storageRepoWrite(), NULL, strLstNew()), true, "    archiveList=NULL, backupList=0");
        TEST_RESULT_BOOL(stanzaDelete(storageRepoWrite(), strLstNew(), strLstNew()), true, "    archiveList=0, backupList=0");

        // create and delete a stanza
        //--------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argListBase);
        strLstAdd(argList,  strNewFmt("--stanza=%s", strPtr(stanza)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/%s", testPath(), strPtr(stanza)));
        strLstAddZ(argList, "stanza-create");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Create pg_control for stanza-create
        storagePutNP(
            storageNewWriteNP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(stanza))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 6569239123849665679}));

        TEST_RESULT_VOID(cmdStanzaCreate(), "create a stanza to be deleted");
        TEST_RESULT_BOOL(
            storageExistsNP(storageTest, strNewFmt("repo/archive/%s/archive.info", strPtr(stanza))), true, "    stanza created");
        TEST_ERROR_FMT(cmdStanzaDelete(), FileMissingError, "stop file does not exist for stanza 'db'\n"
            "HINT: has the pgbackrest stop command been run on this server for this stanza?");

        // Create the stop file
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), BUFSTRDEF("")),
                "create stop file");

        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete");
        TEST_RESULT_BOOL(
            storagePathExistsNP(storageTest, strNewFmt("repo/archive/%s", strPtr(stanza))), false, "    stanza deleted");
        TEST_RESULT_BOOL(
            storageExistsNP(storageLocal(), lockStopFileName(cfgOptionStr(cfgOptStanza))), false, "    stop file removed");

        // Create stanza with directories only
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storagePathCreateNP(storageTest, strNewFmt("repo/archive/%s/9.6-1/1234567812345678", strPtr(stanza))),
            "create archive sub directory");
        TEST_RESULT_VOID(storagePathCreateNP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F", strPtr(stanza))),
            "create backup sub directory");
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), BUFSTRDEF("")),
                "create stop file");
        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete - sub directories only");
        TEST_RESULT_BOOL(
            storagePathExistsNP(storageTest, strNewFmt("repo/archive/%s", strPtr(stanza))), false, "    stanza archive deleted");
        TEST_RESULT_BOOL(
            storagePathExistsNP(storageTest, strNewFmt("repo/backup/%s", strPtr(stanza))), false, "    stanza backup deleted");

        // Create stanza archive only
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, strNewFmt("repo/archive/%s/archive.info", strPtr(stanza))), BUFSTRDEF("")),
                "create archive.info");
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), BUFSTRDEF("")),
                "create stop file");
        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete - archive only");
        TEST_RESULT_BOOL(
            storagePathExistsNP(storageTest, strNewFmt("repo/archive/%s", strPtr(stanza))), false, "    stanza deleted");

        // Create stanza backup only
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, strNewFmt("repo/backup/%s/backup.info", strPtr(stanza))), BUFSTRDEF("")),
                "create backup.info");
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), BUFSTRDEF("")),
                "create stop file");
        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete - backup only");
        TEST_RESULT_BOOL(
            storagePathExistsNP(storageTest, strNewFmt("repo/backup/%s", strPtr(stanza))), false, "    stanza deleted");

        // Create a backup file that matches the regex for a backup directory
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F", strPtr(stanza))), BUFSTRDEF("")),
                "backup file that looks like a directory");
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), BUFSTRDEF("")),
                "create stop file");
        TEST_ERROR_FMT(cmdStanzaDelete(), FileRemoveError,
            "unable to remove '%s/repo/backup/%s/20190708-154306F/backup.manifest': [20] Not a directory", testPath(),
            strPtr(stanza));
        TEST_RESULT_VOID(
            storageRemoveNP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F", strPtr(stanza))), "remove backup directory");

        // Create backup manifest
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F/backup.manifest", strPtr(stanza))),
                BUFSTRDEF("")), "create backup.manifest only");
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F_20190716-191726I/backup.manifest.copy",
                strPtr(stanza))), BUFSTRDEF("")), "create backup.manifest.copy only");
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F_20190716-191800D/backup.manifest",
                strPtr(stanza))), BUFSTRDEF("")), "create backup.manifest");
        TEST_RESULT_VOID(
            storagePutNP(
                storageNewWriteNP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F_20190716-191800D/backup.manifest.copy",
                strPtr(stanza))), BUFSTRDEF("")), "create backup.manifest.copy");
        TEST_RESULT_VOID(manifestDelete(storageRepoWrite()), "delete manifests");
        TEST_RESULT_BOOL(
            (storageExistsNP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F/backup.manifest", strPtr(stanza))) &&
            storageExistsNP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F_20190716-191726I/backup.manifest.copy",
            strPtr(stanza))) &&
            storageExistsNP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F_20190716-191800D/backup.manifest",
            strPtr(stanza))) &&
            storageExistsNP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F_20190716-191800D/backup.manifest.copy",
            strPtr(stanza)))), false, "    all manifests deleted");

        // Create only stanza paths
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete");
        TEST_RESULT_VOID(storagePathCreateNP(storageTest, strNewFmt("repo/archive/%s", strPtr(stanza))),
            "create empty stanza archive path");
        TEST_RESULT_VOID(storagePathCreateNP(storageTest, strNewFmt("repo/backup/%s", strPtr(stanza))),
            "create empty stanza backup path");
        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete - empty directories");

        // Ensure other stanza never deleted
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(
            storageExistsNP(storageTest, strNewFmt("repo/archive/%s/archive.info", strPtr(stanzaOther))), true,
            "otherstanza exists");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
