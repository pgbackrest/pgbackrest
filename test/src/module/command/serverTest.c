/***********************************************************************************************************************************
Test Server Command
***********************************************************************************************************************************/
#include "common/harnessConfig.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("cmdServer()"))
    {
        TEST_TITLE("!!!");

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptTlsServerCert, HRN_PATH_REPO "/test/certificate/pgbackrest-test.crt");
        hrnCfgArgRawZ(argList, cfgOptTlsServerKey, HRN_PATH_REPO "/test/certificate/pgbackrest-test.key");
        HRN_CFG_LOAD(cfgCmdServer, argList);

        TEST_RESULT_VOID(cmdServer(), "server");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
