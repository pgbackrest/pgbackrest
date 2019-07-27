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
Init local mem context and data structure
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
                PgClient *client = pgClientNew(
                    cfgOptionStr(cfgOptPgSocketPath + pgIdx), cfgOptionUInt(cfgOptPgPort + pgIdx), strNew("postgres"), NULL,
                    (TimeMSec)(cfgOptionDbl(cfgOptDbTimeout) * MSEC_PER_SEC));
                Db *db = NULL;

                bool success = false;
                bool standby = false;

                TRY_BEGIN()
                {
                    pgClientOpen(client);
                    db = dbNew(client);
                    standby = dbIsStandby(db);
                    success = true;
                }
                CATCH_ANY()
                {
                    pgClientClose(client);
                    LOG_WARN("unable to check pg-%u: [%s] %s", pgIdx + 1, errorTypeName(errorType()), errorMessage());
                }
                TRY_END();

                // Was the connection successful
                if (success)
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
                            dbFree(db);
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
