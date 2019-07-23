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

        String *query = strNew(
            "select oid, null, relname, relname = 'pg_class' from pg_class where relname in ('pg_class', 'pg_proc')"
                " order by relname");

        TEST_RESULT_STR(
            strPtr(jsonFromVar(varNewVarLst(pgClientQuery(client, query)), 0)),
            "[[1259,null,\"pg_class\",true],[1255,null,\"pg_proc\",false]]", "simple query");
        TEST_RESULT_VOID(pgClientClose(client), "close client");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
