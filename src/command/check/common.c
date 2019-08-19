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
checkDbConfig(const unsigned int pgVersion, const unsigned int dbIdx, const unsigned int dbVersion, const String *dbPath)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
        FUNCTION_TEST_PARAM(UINT, dbIdx);
        FUNCTION_TEST_PARAM(UINT, dbVersion);
        FUNCTION_TEST_PARAM(STRING, dbPath);
    FUNCTION_TEST_END();

    ASSERT(dbIdx > 0);
    ASSERT(dbPath != NULL);

    unsigned int pgPath = cfgOptPgPath + (dbIdx - 1);

    // Error if the version from the control file and the configured pg-path do not match the values obtained from the database
    if (pgVersion != dbVersion || strCmp(cfgOptionStr(pgPath), dbPath) != 0)
    {
        THROW_FMT(
            DbMismatchError, "version '%s' and path '%s' queried from cluster do not match version '%s' and '%s' read from '%s/"
            PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL "'\nHINT: the %s and %s settings likely reference different clusters",
            strPtr(pgVersionToStr(dbVersion)), strPtr(dbPath), strPtr(pgVersionToStr(pgVersion)), strPtr(cfgOptionStr(pgPath)),
            strPtr(cfgOptionStr(pgPath)), cfgOptionName(pgPath), cfgOptionName(cfgOptPgPort + (dbIdx - 1)));
    }

    FUNCTION_TEST_RETURN_VOID();
}
