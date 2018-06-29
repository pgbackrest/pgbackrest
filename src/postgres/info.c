/***********************************************************************************************************************************
PostgreSQL Info
***********************************************************************************************************************************/
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "postgres/info.h"
#include "postgres/type.h"
#include "postgres/version.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Map control/catalog version to PostgreSQL version
***********************************************************************************************************************************/
static unsigned int
pgVersionMap(uint32_t controlVersion, uint32_t catalogVersion)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT32, controlVersion);
        FUNCTION_TEST_PARAM(UINT32, catalogVersion);
    FUNCTION_TEST_END();

    unsigned int result = 0;

    if (controlVersion == 1100 && catalogVersion == 201804191)
        result = PG_VERSION_11;
    else if (controlVersion == 1002 && catalogVersion == 201707211)
        result = PG_VERSION_10;
    else if (controlVersion == 960 && catalogVersion == 201608131)
        result = PG_VERSION_96;
    else if (controlVersion == 942 && catalogVersion == 201510051)
        result = PG_VERSION_95;
    else if (controlVersion == 942 && catalogVersion == 201409291)
        result = PG_VERSION_94;
    else if (controlVersion == 937 && catalogVersion == 201306121)
        result = PG_VERSION_93;
    else if (controlVersion == 922 && catalogVersion == 201204301)
        result = PG_VERSION_92;
    else if (controlVersion == 903 && catalogVersion == 201105231)
        result = PG_VERSION_91;
    else if (controlVersion == 903 && catalogVersion == 201008051)
        result = PG_VERSION_90;
    else if (controlVersion == 843 && catalogVersion == 200904091)
        result = PG_VERSION_84;
    else if (controlVersion == 833 && catalogVersion == 200711281)
        result = PG_VERSION_83;
    else
    {
        THROW_FMT(
            VersionNotSupportedError,
            "unexpected control version = %u and catalog version = %u\n"
                "HINT: is this version of PostgreSQL supported?",
            controlVersion, catalogVersion);
    }

    FUNCTION_TEST_RESULT(UINT, result);
}

/***********************************************************************************************************************************
Get info from pg_control
***********************************************************************************************************************************/
PgControlInfo
pgControlInfo(const String *pgPath)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STRING, pgPath);

        FUNCTION_TEST_ASSERT(pgPath != NULL);
    FUNCTION_DEBUG_END();

    PgControlInfo result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Open control file for read
        StorageFileRead *controlRead = storageNewReadNP(
            storageLocal(), strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(pgPath)));
        storageFileReadOpen(controlRead);

        // Read contents
        PgControlFile *control = (PgControlFile *)bufPtr(storageFileRead(controlRead));

        // Copy to result structure and get PostgreSQL version
        result.systemId = control->systemId;
        result.controlVersion = control->controlVersion;
        result.catalogVersion = control->catalogVersion;
        result.version = pgVersionMap(result.controlVersion, result.catalogVersion);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(PG_CONTROL_INFO, result);
}


/***********************************************************************************************************************************
Validate PostgreSQL version
***********************************************************************************************************************************/
void
pgVersionValid(const unsigned int version)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, version);
    FUNCTION_TEST_END();

    if (version < PG_MIN_VERSION || version > PG_MAX_VERSION)
    {
        THROW_FMT(VersionNotSupportedError, "version %u is not valid or is not a supported PostgreSQL version", version);
    }

    FUNCTION_TEST_RESULT_VOID();
}
