/***********************************************************************************************************************************
Test Stanza Create Command
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"
#include "postgres/version.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storagePosixNew(
        strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

    // String *backupStanzaPath = strNew("repo/backup/db");
    // String *backupInfoFileName = strNewFmt("%s/backup.info", strPtr(backupStanzaPath));
    // String *archiveStanzaPath = strNew("repo/archive/db");
    // String *archiveInfoFileName = strNewFmt("%s/archive.info", strPtr(archiveStanzaPath));

    StringList *argListBase = strLstNew();
    strLstAddZ(argListBase, "pgbackrest");
    strLstAddZ(argListBase, "--stanza=db");
    strLstAdd(argListBase, strNewFmt("--pg1-path=%s/db", testPath()));
    strLstAdd(argListBase, strNewFmt("--repo1-path=%s/repo", testPath()));
    strLstAddZ(argListBase, "stanza-create");

    // *****************************************************************************************************************************
    if (testBegin("cmdStanzaCreate()"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        storagePutNP(
            storageNewWriteNP(storageTest, strNew("db/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL)),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 0xFACEFACEFACEFACE}));

        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
