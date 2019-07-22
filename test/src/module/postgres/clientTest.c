/***********************************************************************************************************************************
Test PostgreSQL Client
***********************************************************************************************************************************/

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
            THROW(AssertError, "unable to create cluster");

        PgClient *client = NULL;
        TEST_ASSIGN(client, pgClientNew(NULL, 5432, strNew("postgres"), NULL), "new client");
        TEST_RESULT_VOID(pgClientFree(client), "free client");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
