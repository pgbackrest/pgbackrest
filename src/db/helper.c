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
Classify a standby as synchronous or asynchronous. When that cannot be determined the standby is unclassified, never asynchronous.
***********************************************************************************************************************************/
typedef enum
{
    dbGetStandbyKindSync,
    dbGetStandbyKindAsync,
    dbGetStandbyKindUnclassified,
} DbGetStandbyKind;

static const char *const dbGetStandbyKindName[] = {"synchronous", "asynchronous", "unclassified"};

typedef struct DbGetStandby
{
    unsigned int pgIdx;                                             // cfgOptGrpPg index of the standby
    Db *db;                                                         // Standby db object
    const String *name;                                             // Application name, NULL when it cannot be derived
    DbGetStandbyKind kind;                                          // Standby kind
} DbGetStandby;

// Number of times a name appears in a list
static unsigned int
dbGetStandbyNameTotal(const StringList *const nameList, const String *const name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, nameList);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    unsigned int result = 0;

    for (unsigned int nameIdx = 0; nameIdx < strLstSize(nameList); nameIdx++)
    {
        if (strEq(strLstGet(nameList, nameIdx), name))
            result++;
    }

    FUNCTION_TEST_RETURN(UINT, result);
}

static DbGetStandbyKind
dbGetStandbyKind(
    const DbGetStandby *const standby, const StringList *const standbyNameList, const DbStandbyNameResult *const nameList)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM_P(VOID, standby);
        FUNCTION_LOG_PARAM(STRING_LIST, standbyNameList);
        FUNCTION_LOG_PARAM_P(VOID, nameList);
    FUNCTION_LOG_END();

    ASSERT(standby != NULL);

    DbGetStandbyKind result = dbGetStandbyKindUnclassified;

    // Classifying requires the standby name (a warning has already been logged if it is missing)
    if (standby->name != NULL)
    {
        ASSERT(standbyNameList != NULL);
        ASSERT(nameList != NULL);

        // Application names must be unique since the row on the primary is found by name. A name with more than one row on the
        // primary or shared by configured standbys cannot be classified, nor can a name with no row. A configured standby that
        // shares its name with a standby that is not configured, e.g. a cascaded standby named like a standby connected to the
        // primary, cannot be detected.
        const unsigned int syncTotal = dbGetStandbyNameTotal(nameList->syncList, standby->name);
        const unsigned int rowTotal = syncTotal + dbGetStandbyNameTotal(nameList->asyncList, standby->name);
        const unsigned int standbyTotal = dbGetStandbyNameTotal(standbyNameList, standby->name);

        if (rowTotal == 0)
        {
            LOG_WARN_FMT(
                "unable to determine whether %s is a synchronous standby: [%s] application name '%s' has no row in"
                " pg_stat_replication on the primary",
                cfgOptionGroupName(cfgOptGrpPg, standby->pgIdx), errorTypeName(&DbQueryError), strZ(standby->name));
        }
        else if (rowTotal > 1 || standbyTotal > 1)
        {
            LOG_WARN_FMT(
                "unable to determine whether %s is a synchronous standby: [%s] application name '%s' is not unique (%u row(s) on"
                " the primary, %u configured standby(s))",
                cfgOptionGroupName(cfgOptGrpPg, standby->pgIdx), errorTypeName(&DbQueryError), strZ(standby->name), rowTotal,
                standbyTotal);
        }
        else
            result = syncTotal > 0 ? dbGetStandbyKindSync : dbGetStandbyKindAsync;
    }

    FUNCTION_LOG_RETURN(ENUM, result);
}

/***********************************************************************************************************************************
Decode the standby mode
***********************************************************************************************************************************/
typedef struct DbGetStandbyMode
{
    bool classify;                                                  // Are standbys classified, i.e. the mode is not any?
    DbGetStandbyKind kindWant;                                      // Kind of standby wanted
    bool require;                                                   // Is only the wanted kind allowed?
} DbGetStandbyMode;

static DbGetStandbyMode
dbGetStandbyModeDecode(const StringId standbyMode)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_ID, standbyMode);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN_TYPE(
        DbGetStandbyMode,
        (DbGetStandbyMode){
            .classify = standbyMode != CFGOPTVAL_BACKUP_STANDBY_MODE_ANY,
            .kindWant =
                standbyMode == CFGOPTVAL_BACKUP_STANDBY_MODE_SYNC || standbyMode == CFGOPTVAL_BACKUP_STANDBY_MODE_PREFER_SYNC ?
                    dbGetStandbyKindSync : dbGetStandbyKindAsync,
            .require =
                standbyMode == CFGOPTVAL_BACKUP_STANDBY_MODE_SYNC || standbyMode == CFGOPTVAL_BACKUP_STANDBY_MODE_ASYNC});
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
    const DbGetStandbyMode mode = dbGetStandbyModeDecode(standbyMode);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // When standbys are classified then all standbys are kept until the primary is found
        List *const standbyList = !primaryOnly && mode.classify ? lstNewP(sizeof(DbGetStandby)) : NULL;

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
                        lstAdd(standbyList, &(DbGetStandby){.pgIdx = pgIdx, .db = db, .kind = dbGetStandbyKindUnclassified});
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
            const DbStandbyNameResult *nameList = NULL;
            DbStandbyNameResult nameResult;

            // Without the standby names on the primary no standby can be classified
            if (result.primary != NULL)
            {
                TRY_BEGIN()
                {
                    nameResult = dbStandbyNameList(result.primary);
                    nameList = &nameResult;
                }
                CATCH_ANY()
                {
                    LOG_WARN_FMT(
                        "unable to get standby names from %s: [%s] %s", cfgOptionGroupName(cfgOptGrpPg, result.primaryIdx),
                        errorTypeName(errorType()), errorMessage());
                }
                TRY_END();
            }

            // Derive the name of every standby, since all names are needed to detect standbys that share one
            StringList *const standbyNameList = strLstNew();

            if (nameList != NULL)
            {
                for (unsigned int standbyIdx = 0; standbyIdx < lstSize(standbyList); standbyIdx++)
                {
                    DbGetStandby *const standby = lstGet(standbyList, standbyIdx);

                    TRY_BEGIN()
                    {
                        standby->name = dbWalReceiverAppName(standby->db);
                        strLstAdd(standbyNameList, standby->name);
                    }
                    CATCH_ANY()
                    {
                        LOG_WARN_FMT(
                            "unable to determine whether %s is a synchronous standby: [%s] %s",
                            cfgOptionGroupName(cfgOptGrpPg, standby->pgIdx), errorTypeName(errorType()), errorMessage());
                    }
                    TRY_END();
                }
            }

            // Rank standbys: the wanted kind first, then the other kind, then unclassified. Strict modes only allow the wanted
            // kind. Stop at the first standby of the wanted kind.
            const DbGetStandby *selected = NULL;
            unsigned int selectedRank = mode.require ? 1 : 3;

            for (unsigned int standbyIdx = 0; standbyIdx < lstSize(standbyList) && selectedRank > 0; standbyIdx++)
            {
                DbGetStandby *const standby = lstGet(standbyList, standbyIdx);
                standby->kind = dbGetStandbyKind(standby, standbyNameList, nameList);

                const unsigned int rank =
                    standby->kind == mode.kindWant ? 0 : standby->kind == dbGetStandbyKindUnclassified ? 2 : 1;

                if (rank < selectedRank)
                {
                    selected = standby;
                    selectedRank = rank;
                }
            }

            // Free the standbys that were not selected
            for (unsigned int standbyIdx = 0; standbyIdx < lstSize(standbyList); standbyIdx++)
            {
                const DbGetStandby *const standby = lstGet(standbyList, standbyIdx);

                if (standby != selected)
                    dbFree(standby->db);
            }

            if (selected != NULL)
            {
                if (selected->kind != mode.kindWant)
                {
                    LOG_WARN_FMT(
                        "unable to find %s standby, using %s standby %s", dbGetStandbyKindName[mode.kindWant],
                        dbGetStandbyKindName[selected->kind], cfgOptionGroupName(cfgOptGrpPg, selected->pgIdx));
                }

                result.standbyIdx = selected->pgIdx;
                result.standby = selected->db;
            }
        }

        // Error if no standby was found
        if (result.standby == NULL && standbyRequired == CFGOPTVAL_BACKUP_STANDBY_Y)
        {
            THROW_FMT(
                DbConnectError, "unable to find standby cluster - cannot proceed%s",
                mode.require && !lstEmpty(standbyList) ?
                    "\nHINT: is a standby available that matches the '" CFGOPT_BACKUP_STANDBY_MODE "' option?" : "");
        }

        dbMove(result.primary, memContextPrior());
        dbMove(result.standby, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}
