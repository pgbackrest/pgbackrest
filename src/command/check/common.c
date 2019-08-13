/***********************************************************************************************************************************
Check Common Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "config/config.h"
#include "db/db.h"
#include "db/helper.h"
#include "postgres/interface.h"

/***********************************************************************************************************************************
Check the database path and version are configured correctly.
***********************************************************************************************************************************/
void
checkDbConfig(const unsigned int pgVersion, const Db *dbObject, const unsigned int dbIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
        FUNCTION_TEST_PARAM(DB, dbObject);
        FUNCTION_TEST_PARAM(UINT, dbIdx);
    FUNCTION_TEST_END();

    unsigned int pgPath = dbIdx == 1 ? cfgOptPgPath : cfgOptPgPath + dbIdx;

    // Error if the version from the control file and the configured pg-path do not match the values obtained from the database
    if (pgVersion != dbPgVersion(dbObject) || strCmp(cfgOptionStr(pgPath), dbPgDataPath(dbObject)) != 0)
    {
// CSHANG We don't check for this message anywhere that I can find so need to add to a mock or real test...
        THROW_FMT(
            DbMismatchError, "version '%s' and path '%s' queried from cluster do not match version '%s' and '%s' read from '%s/"
            PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL "'\nHINT: the %s and %s settings likely reference different clusters",
            strPtr(pgVersionToStr(dbPgVersion(dbObject))), strPtr(dbPgDataPath(dbObject)),
            strPtr(pgVersionToStr(pgVersion)), strPtr(cfgOptionStr(pgPath)), strPtr(cfgOptionStr(pgPath)), cfgOptionName(pgPath),
            cfgOptionName(dbIdx == 1 ? cfgOptPgPort : cfgOptPgPort + dbIdx));
    }

    FUNCTION_TEST_RETURN_VOID();
}
