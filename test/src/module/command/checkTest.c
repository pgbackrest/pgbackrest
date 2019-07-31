/***********************************************************************************************************************************
Test Check Command
***********************************************************************************************************************************/
#include "postgres/version.h"

#include "common/harnessConfig.h"
#include "common/harnessPq.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("cmdCheck()"))
    {
        String *pg1Path = strNewFmt("--repo1-path=%s/repo", testPath());

        // Single primary
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, pg1Path);
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg1", testPath()));
        strLstAddZ(argList, "check");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Set script
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_84(1, "dbname='postgres' port=5432", strPtr(pg1Path)),
            HRNPQ_MACRO_WAL_SWITCH(1, "xlog", "000000010000000100000001"),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

        TEST_RESULT_VOID(cmdCheck(), "check");

        // Single standby
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, pg1Path);
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg1", testPath()));
        strLstAddZ(argList, "check");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Set script
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_92(1, "dbname='postgres' port=5432", strPtr(pg1Path), true),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

        TEST_RESULT_VOID(cmdCheck(), "check");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
