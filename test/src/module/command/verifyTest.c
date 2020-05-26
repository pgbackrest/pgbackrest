/***********************************************************************************************************************************
Test Stanza Commands
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"
#include "common/harnessPq.h"
#include "postgres/interface.h"
#include "postgres/version.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storagePosixNewP(strNew(testPath()), .write = true);

    String *stanza = strNew("db");
    // String *fileName = strNew("test.info"); // CSHANG Do we need?
    String *backupStanzaPath = strNewFmt("repo/backup/%s", strPtr(stanza));
    String *backupInfoFileName = strNewFmt("%s/backup.info", strPtr(backupStanzaPath));
    // String *archiveStanzaPath = strNewFmt("repo/archive/%s", strPtr(stanza));
    // String *archiveInfoFileName = strNewFmt("%s/archive.info", strPtr(archiveStanzaPath));

    StringList *argListBase = strLstNew();
    // strLstAddZ(argListBase, "--no-online");  // CSHANG not a valid verify options - is that ok?
    strLstAdd(argListBase, strNewFmt("--stanza=%s", strPtr(stanza)));
    strLstAdd(argListBase, strNewFmt("--repo1-path=%s/repo", testPath()));

    String *backupInfoContent = strNewFmt(
        "[backup:current]\n"
        "20181119-152138F={"
        "\"backrest-format\":5,\"backrest-version\":\"2.28dev\","
        "\"backup-archive-start\":\"000000010000000000000002\",\"backup-archive-stop\":\"000000010000000000000002\","
        "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
        "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
        "\"backup-timestamp-start\":1482182846,\"backup-timestamp-stop\":1482182861,\"backup-type\":\"full\","
        "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
        "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
        "\n"
        "[db]\n"
        "db-catalog-version=201409291\n"
        "db-control-version=942\n"
        "db-id=1\n"
        "db-system-id=6625592122879095702\n"
        "db-version=\"9.4\"\n"
        "\n"
        "[db:history]\n"
        "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6625592122879095702,"
            "\"db-version\":\"9.4\"}");

    const Buffer *backupInfoBase = harnessInfoChecksumZ(strPtr(backupInfoContent));

    // *****************************************************************************************************************************
    if (testBegin("cmdVerify()"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        harnessCfgLoad(cfgCmdVerify, argList);

        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, backupInfoFileName), backupInfoBase), "write valid backup.info");
        TEST_RESULT_VOID(cmdVerify(), "valid backup.info but missing copy");
        harnessLogResult(
            "P00  ERROR: [055]: unable to open missing file '/home/vagrant/test/test-0/repo/backup/db/backup.info.copy' for read");

        // Buffer *contentLoad = BUFSTRDEF(
        //             "[backrest]\n"
        //             "backrest-checksum=\"BOGUS\"\n"
        //             "backrest-format=5\n"
        //             "backrest-version=\"2.28\"\n");
        //
        // bufCat(contentLoad, BUF(BUFSTR(backupInfoContent))); // CSHANG this does not work
        // TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageTest, backupInfoFileName), backupInfoBase), "write invalid backup.info");
        //
        // TEST_ERROR_FMT(
        //     cmdVerify(), HostInvalidError, "expire command must be run on the repository host");

        // CSHANG Tests - parens for logging, e.g. (ERROR) means LOG_ERROR and continue:
        //
        // 1) Backup.info:
        //     - only backup.info exists (WARN) and is OK
        //     - only backup.info exists (WARN) and is NOT OK (ERROR):
        //         - checksum in file (corrupt it) does not match generated checksum
        //     - only backup.info.copy exists (WARN) and is OK
        //     - both info & copy exist and are valid but don't match each other (in this case are we always reading the main file? If so, then we must check the main file in the code against the archive.info file)
        // 2) Local and remote tests
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
