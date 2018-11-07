/***********************************************************************************************************************************
Test Info Command
***********************************************************************************************************************************/
#include "storage/driver/posix/storage.h"

#include "common/harnessConfig.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create the repo directories
    String *repoPath = strNewFmt("%s/repo", testPath());
    String *archivePath = strNewFmt("%s/%s", strPtr(repoPath), STORAGE_PATH_ARCHIVE);
    String *backupPath = strNewFmt("%s/%s", strPtr(repoPath), STORAGE_PATH_BACKUP);
    storagePathCreateNP(storageLocalWrite(), archivePath);
    storagePathCreateNP(storageLocalWrite(), backupPath);

    // *****************************************************************************************************************************
    if (testBegin("infoRender()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAdd(argList, strNewFmt("--repo-path=%s", strPtr(repoPath)));
        strLstAddZ(argList, "--output=json");
        strLstAddZ(argList, "info");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_STR(strPtr(infoRender()), "[]\n", "empty json");

        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), strCat(backupPath, "/stanza1")), "create stanza1 directory");
        TEST_RESULT_STR(strPtr(infoRender()),
            "[\n"
            "    {\n"
            "        \"name\" : \"stanza1\"\n"
            "    }\n"
            "]\n", "single stanza");  // CSHANG Flesh this test out for stanza without backup or archive info
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
