/***********************************************************************************************************************************
Database Helper
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "config/config.h"
#include "db/helper.h"
#include "postgres/interface.h"
#include "protocol/helper.h"
#include "storage/helper.h"
#include "version.h"

/**********************************************************************************************************************************/
// Helper to get a connection to the specified pg cluster
static Db *
dbGetIdx(const unsigned int pgIdx)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, pgIdx);
    FUNCTION_LOG_END();

    ASSERT(pgIdx < cfgOptionGroupIdxTotal(cfgOptGrpPg));

    Db *result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *applicationName = strNewFmt(PROJECT_NAME " [%s]", cfgCommandName());

        if (pgIsLocal(pgIdx))
        {
            result = dbNew(
                pgClientNew(
                    cfgOptionIdxStrNull(cfgOptPgSocketPath, pgIdx), cfgOptionIdxUInt(cfgOptPgPort, pgIdx),
                    cfgOptionIdxStr(cfgOptPgDatabase, pgIdx), cfgOptionIdxStrNull(cfgOptPgUser, pgIdx),
                    cfgOptionUInt64(cfgOptDbTimeout)),
                NULL, storagePgIdx(pgIdx), applicationName);
        }
        else
            result = dbNew(NULL, protocolRemoteGet(protocolStorageTypePg, pgIdx, true), storagePgIdx(pgIdx), applicationName);

        dbMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(DB, result);
}

FN_EXTERN DbGetResult
dbGet(const bool primaryOnly, const bool primaryRequired, const StringId standbyRequired)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BOOL, primaryOnly);
        FUNCTION_LOG_PARAM(BOOL, primaryRequired);
        FUNCTION_LOG_PARAM(STRING_ID, standbyRequired);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_STRUCT();

    ASSERT(!(primaryOnly && standbyRequired == CFGOPTVAL_BACKUP_STANDBY_Y));

    DbGetResult result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Loop through to look for primary and standby (if required)
        for (unsigned int pgIdx = 0; pgIdx < cfgOptionGroupIdxTotal(cfgOptGrpPg); pgIdx++)
        {
            Db *db = NULL;
            bool standby = false;

            TRY_BEGIN()
            {
                db = dbGetIdx(pgIdx);

                // This needs to be nested because db can be reset to NULL on an error in the outer try but we need the pointer
                // to be able to free it.
                TRY_BEGIN()
                {
                    dbOpen(db);
                    standby = dbIsStandby(db);
                }
                CATCH_ANY()
                {
                    dbFree(db);
                    RETHROW();
                }
                TRY_END();
            }
            CATCH_ANY()
            {
                LOG_WARN_FMT(
                    "unable to check %s: [%s] %s", cfgOptionGroupName(cfgOptGrpPg, pgIdx), errorTypeName(errorType()),
                    errorMessage());
                db = NULL;
            }
            TRY_END();

            // Was the connection successful
            if (db != NULL)
            {
                // Is this cluster a standby
                if (standby)
                {
                    // If a standby has not already been found then assign it
                    if (result.standby == NULL && !primaryOnly)
                    {
                        result.standbyIdx = pgIdx;
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
                    if (result.primary != NULL)
                        THROW(DbConnectError, "more than one primary cluster found");

                    result.primaryIdx = pgIdx;
                    result.primary = db;
                }
            }
        }

        // Error if no primary was found
        if (result.primary == NULL && primaryRequired)
        {
            THROW(
                DbConnectError,
                "unable to find primary cluster - cannot proceed\n"
                "HINT: are all available clusters in recovery?");
        }

        // Error if no standby was found
        if (result.standby == NULL && standbyRequired == CFGOPTVAL_BACKUP_STANDBY_Y)
            THROW(DbConnectError, "unable to find standby cluster - cannot proceed");

        dbMove(result.primary, memContextPrior());
        dbMove(result.standby, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}
