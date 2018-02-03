/***********************************************************************************************************************************
Test Common Command Routines
***********************************************************************************************************************************/
#include <fcntl.h>
#include <unistd.h>

#include "storage/storage.h"
#include "version.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    // *****************************************************************************************************************************
    if (testBegin("cmdBegin() and cmdEnd()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        cfgInit();
        cfgCommandSet(cfgCmdArchiveGet);

        cfgOptionValidSet(cfgOptCompress, true);
        cfgOptionSet(cfgOptCompress, cfgSourceParam, varNewBool(true));

        cfgOptionValidSet(cfgOptConfig, true);
        cfgOptionNegateSet(cfgOptConfig, true);
        cfgOptionSet(cfgOptConfig, cfgSourceParam, NULL);

        cfgOptionValidSet(cfgOptPgHost, true);
        cfgOptionSet(cfgOptPgHost, cfgSourceConfig, NULL);

        cfgOptionValidSet(cfgOptRepoHost, true);
        cfgOptionSet(cfgOptRepoHost, cfgSourceConfig, varNewStr(strNew("backup1")));

        cfgOptionValidSet(cfgOptRepoPath, true);
        cfgOptionSet(cfgOptRepoPath, cfgSourceConfig, varNewStr(strNew("/path/to the/repo")));

        cfgOptionValidSet(cfgOptRepoS3Key, true);
        cfgOptionSet(cfgOptRepoS3Key, cfgSourceConfig, varNewStr(strNew("SECRET-STUFF")));

        cmdBegin();
        testLogResult(
            "P00   INFO: archive-get command begin " PGBACKREST_VERSION ": --compress --no-config --repo1-host=backup1 "
                "--repo1-path=\"/path/to the/repo\" --repo1-s3-key=<redacted>");

        // -------------------------------------------------------------------------------------------------------------------------
        cmdEnd(0);
        testLogResult("P00   INFO: archive-get command end: completed successfully");

        // -------------------------------------------------------------------------------------------------------------------------
        cmdEnd(25);
        testLogResult("P00   INFO: archive-get command end: aborted with exception [025]");
    }
}
