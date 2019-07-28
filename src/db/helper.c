/***********************************************************************************************************************************
Database Helper
***********************************************************************************************************************************/
#include "build.auto.h"

// #include "common/crypto/common.h"
#include "common/debug.h"
// #include "common/exec.h"
// #include "common/memContext.h"
#include "config/config.h"
// #include "config/exec.h"
// #include "config/protocol.h"
#include "db/helper.h"

/***********************************************************************************************************************************
Get specified cluster
***********************************************************************************************************************************/
Db *
dbGetId(unsigned int pgId)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, pgId);
    FUNCTION_LOG_END();

    Db *result = dbNew(
        pgClientNew(
            cfgOptionStr(cfgOptPgSocketPath + pgId - 1), cfgOptionUInt(cfgOptPgPort + pgId - 1), strNew("postgres"), NULL,
            (TimeMSec)(cfgOptionDbl(cfgOptDbTimeout) * MSEC_PER_SEC)));

    FUNCTION_LOG_RETURN(DB, result);
}

/***********************************************************************************************************************************
Get primary cluster or primary and standby cluster.
***********************************************************************************************************************************/
DbGetResult dbGet(bool primaryOnly)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BOOL, primaryOnly);
    FUNCTION_LOG_END();

    DbGetResult result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Loop through to look for primary and standby (if required)
        for (unsigned int pgIdx = 0; pgIdx < cfgOptionIndexTotal(cfgOptPgPath); pgIdx++)
        {
            if (cfgOptionTest(cfgOptPgHost + pgIdx) || cfgOptionTest(cfgOptPgPath + pgIdx))
            {
                Db *db = dbGetId(pgIdx + 1);
                bool standby = false;

                TRY_BEGIN()
                {
                    dbOpen(db);

                    !!! NEED TO FIX THIS
                    // TRY_BEGIN()
                    // {
                    //     dbOpen(db);
                    //     standby = dbIsStandby(db);
                    // }
                    // CATCH_ANY()
                    // {
                    //     dbClose(db);
                    //     db = NULL;
                    //
                    //     LOG_WARN("unable to check pg-%u: [%s] %s", pgIdx + 1, errorTypeName(errorType()), errorMessage());
                    // }
                    // TRY_END();
                }
                CATCH_ANY()
                {
                    db = NULL;
                    LOG_WARN("unable to open pg-%u: [%s] %s", pgIdx + 1, errorTypeName(errorType()), errorMessage());
                }
                TRY_END();

                // Was the connection successful
                if (db != NULL)
                {
                    // Is this cluster a standby
                    if (standby)
                    {
                        // If a standby has not already been found then assign it
                        if (result.standbyId == 0 && !primaryOnly)
                        {
                            result.standbyId = pgIdx + 1;
                            result.standby = db;
                        }
                        // Else close the connection since we don't need it
                        else
                            dbClose(db);
                    }
                    // Else is a primary
                    else
                    {
                        // Error if more than one primary was found
                        if (result.primaryId != 0)
                            THROW(DbConnectError, "more than one primary cluster found");

                        result.primaryId = pgIdx + 1;
                        result.primary = db;
                    }
                }
            }
        }

        // Error if no primary was found
        if (result.primaryId == 0)
            THROW(DbConnectError, "unable to find primary cluster - cannot proceed");

        dbMove(result.primary, MEM_CONTEXT_OLD());
        dbMove(result.standby, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(DB_GET_RESULT, result);
}
