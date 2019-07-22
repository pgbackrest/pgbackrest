/***********************************************************************************************************************************
Test PostgreSQL Client
***********************************************************************************************************************************/
#include "common/type/json.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("pgClient"))
    {
        if (system("sudo pg_createcluster 11 test") != 0)
            THROW(AssertError, "unable to create cluster");

        if (system("sudo pg_ctlcluster 11 test start") != 0)
            THROW(AssertError, "unable to start cluster");

        if (system("sudo -u postgres psql -c 'create user vagrant superuser'") != 0)
            THROW(AssertError, "unable to create superuser");

        PgClient *client = NULL;
        TEST_ASSIGN(client, pgClientOpen(pgClientNew(NULL, 5432, strNew("postgres"), NULL)), "new client");
        TEST_RESULT_VOID(pgClientFree(client), "free client");

        TEST_ASSIGN(client, pgClientOpen(pgClientNew(NULL, 5432, strNew("postgres"), NULL)), "new client");
        TEST_RESULT_STR(
            strPtr(
                jsonFromVar(
                    varNewVarLst(
                        pgClientQuery(client, strNew("select relname, reltype from pg_class where relname = 'pg_class'"))), 0)),
            "[]", "simple query");
        TEST_RESULT_VOID(pgClientClose(client), "close client");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
