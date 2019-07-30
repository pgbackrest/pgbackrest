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
            HRNPQ_MACRO_OPEN(1, "dbname='postgres' port=5432"),
            HRNPQ_MACRO_SET_SEARCH_PATH(1),
            HRNPQ_MACRO_VALIDATE_QUERY(1, PG_VERSION_84, strPtr(pg1Path)),
            HRNPQ_MACRO_CLOSE(1),

            HRNPQ_MACRO_DONE()
        });

        cmdCheck();
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
