/***********************************************************************************************************************************
Database Helper
***********************************************************************************************************************************/
#include <build.h>

#include "common/debug.h"
#include "common/type/list.h"
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

/***********************************************************************************************************************************
Classify a standby as synchronous or asynchronous. When that cannot be determined the standby is unknown, never asynchronous.
***********************************************************************************************************************************/
typedef struct DbGetStandby
{
    unsigned int pgIdx;                                             // cfgOptGrpPg index of the standby
    Db *db;                                                         // Standby db object
} DbGetStandby;

typedef enum
{
    dbGetStandbyKindSync,
    dbGetStandbyKindAsync,
    dbGetStandbyKindUnknown,
} DbGetStandbyKind;

static const char *const dbGetStandbyKindName[] = {"synchronous", "asynchronous", "unclassified"};

static DbGetStandbyKind
dbGetStandbyKind(const DbGetStandby *const standby, const StringList *const syncList)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM_P(VOID, standby);
        FUNCTION_LOG_PARAM(STRING_LIST, syncList);
    FUNCTION_LOG_END();

    ASSERT(standby != NULL);

    DbGetStandbyKind result = dbGetStandbyKindUnknown;

    // Classifying requires the synchronous standby list from the primary (a warning has already been logged if it is missing)
    if (syncList != NULL)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            TRY_BEGIN()
            {
                result = strLstExists(syncList, dbWalReceiverAppName(standby->db)) ? dbGetStandbyKindSync : dbGetStandbyKindAsync;
            }
            CATCH_ANY()
            {
                LOG_WARN_FMT(
                    "unable to determine whether %s is a synchronous standby: [%s] %s",
                    cfgOptionGroupName(cfgOptGrpPg, standby->pgIdx), errorTypeName(errorType()), errorMessage());
            }
            TRY_END();
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN(ENUM, result);
}

/**********************************************************************************************************************************/
FN_EXTERN DbGetResult
dbGet(const bool primaryOnly, const bool primaryRequired, const StringId standbyRequired, const StringId standbyMode)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BOOL, primaryOnly);
        FUNCTION_LOG_PARAM(BOOL, primaryRequired);
        FUNCTION_LOG_PARAM(STRING_ID, standbyRequired);
        FUNCTION_LOG_PARAM(STRING_ID, standbyMode);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_STRUCT();

    ASSERT(!(primaryOnly && standbyRequired == CFGOPTVAL_BACKUP_STANDBY_Y));

    DbGetResult result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // When the standby mode is not any then all standbys are kept until the primary is found so they can be classified
        List *const standbyList =
            !primaryOnly && standbyMode != CFGOPTVAL_BACKUP_STANDBY_MODE_ANY ? lstNewP(sizeof(DbGetStandby)) : NULL;

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
                    // If the standby needs to be classified then keep it in the list for now
                    if (standbyList != NULL)
                        lstAdd(standbyList, &(DbGetStandby){.pgIdx = pgIdx, .db = db});
                    // Else if a standby has not already been found then assign it
                    else if (result.standby == NULL && !primaryOnly)
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

        // Select a standby based on whether it is synchronous
        if (standbyList != NULL && !lstEmpty(standbyList))
        {
            const DbGetStandbyKind kindWant =
                standbyMode == CFGOPTVAL_BACKUP_STANDBY_MODE_PREFER_SYNC || standbyMode == CFGOPTVAL_BACKUP_STANDBY_MODE_SYNC ?
                    dbGetStandbyKindSync : dbGetStandbyKindAsync;
            const bool require =
                standbyMode == CFGOPTVAL_BACKUP_STANDBY_MODE_SYNC || standbyMode == CFGOPTVAL_BACKUP_STANDBY_MODE_ASYNC;
            const StringList *syncList = NULL;

            // Without the synchronous standby list on the primary no standby can be classified
            if (result.primary != NULL)
            {
                TRY_BEGIN()
                {
                    syncList = dbSyncStandbyList(result.primary);
                }
                CATCH_ANY()
                {
                    LOG_WARN_FMT(
                        "unable to get synchronous standbys from %s: [%s] %s",
                        cfgOptionGroupName(cfgOptGrpPg, result.primaryIdx), errorTypeName(errorType()), errorMessage());
                }
                TRY_END();
            }

            DbGetStandby *fallback = NULL;
            DbGetStandbyKind fallbackKind = dbGetStandbyKindUnknown;

            for (unsigned int standbyIdx = 0; standbyIdx < lstSize(standbyList); standbyIdx++)
            {
                DbGetStandby *const standby = lstGet(standbyList, standbyIdx);

                if (result.standby == NULL)
                {
                    const DbGetStandbyKind kind = dbGetStandbyKind(standby, syncList);

                    if (kind == kindWant)
                    {
                        result.standbyIdx = standby->pgIdx;
                        result.standby = standby->db;
                        continue;
                    }

                    // Keep the best fallback in case no standby matches. A standby of the other kind is better than one that could
                    // not be classified. Strict modes never fall back.
                    if (!require &&
                        (fallback == NULL || (fallbackKind == dbGetStandbyKindUnknown && kind != dbGetStandbyKindUnknown)))
                    {
                        if (fallback != NULL)
                            dbFree(fallback->db);

                        fallback = standby;
                        fallbackKind = kind;
                        continue;
                    }
                }

                dbFree(standby->db);
            }

            if (result.standby == NULL && fallback != NULL)
            {
                LOG_WARN_FMT(
                    "unable to find %s standby, using %s standby %s", dbGetStandbyKindName[kindWant],
                    dbGetStandbyKindName[fallbackKind], cfgOptionGroupName(cfgOptGrpPg, fallback->pgIdx));

                result.standbyIdx = fallback->pgIdx;
                result.standby = fallback->db;
            }
            else if (fallback != NULL)
                dbFree(fallback->db);
        }

        // Error if no standby was found
        if (result.standby == NULL && standbyRequired == CFGOPTVAL_BACKUP_STANDBY_Y)
        {
            THROW_FMT(
                DbConnectError, "unable to find standby cluster - cannot proceed%s",
                standbyList != NULL ?
                    "\nHINT: is a standby available that matches the '" CFGOPT_BACKUP_STANDBY_MODE "' option?" : "");
        }

        dbMove(result.primary, memContextPrior());
        dbMove(result.standby, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}
