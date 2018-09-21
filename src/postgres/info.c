/***********************************************************************************************************************************
PostgreSQL Info
***********************************************************************************************************************************/
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/convert.h"
#include "common/regExp.h"
#include "postgres/info.h"
#include "postgres/type.h"
#include "postgres/version.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Set control data size.  The control file is actually 8192 bytes but only the first 512 bytes are used to prevent torn pages even on
really old storage with 512-byte sectors.
***********************************************************************************************************************************/
#define PG_CONTROL_DATA_SIZE                                        512

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

    if (controlVersion == 1100 && catalogVersion == 201809051)
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
Convert version string to version number
***********************************************************************************************************************************/
unsigned int
pgVersionFromStr(const String *version)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STRING, version);

        FUNCTION_DEBUG_ASSERT(version != NULL);
    FUNCTION_DEBUG_END();

    unsigned int result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If format is not number.number (9.4) or number only (10) then error
        if (!regExpMatchOne(strNew("^[0-9]+[.]*[0-9]+$"), version))
            THROW_FMT(AssertError, "version %s format is invalid", strPtr(version));

        // If there is a dot set the major and minor versions, else just the major
        int idxStart = strChr(version, '.');
        unsigned int major;
        unsigned int minor = 0;

        if (idxStart != -1)
        {
            major = cvtZToUInt(strPtr(strSubN(version, 0, (size_t)idxStart)));
            minor = cvtZToUInt(strPtr(strSub(version, (size_t)idxStart + 1)));
        }
        else
            major = cvtZToUInt(strPtr(version));

        // No check to see if valid/supported PG version is on purpose
        result = major * 10000 + minor * 100;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(UINT, result);
}

/***********************************************************************************************************************************
Convert version number to string
***********************************************************************************************************************************/
String *
pgVersionToStr(unsigned int version)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(UINT, version);
    FUNCTION_DEBUG_END();

    String *result = version >= PG_VERSION_10 ?
        strNewFmt("%u", version / 10000) : strNewFmt("%u.%u", version / 10000, version % 10000 / 100);

    FUNCTION_DEBUG_RESULT(STRING, result);
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
        // Read control file
        PgControlFile *control = (PgControlFile *)bufPtr(
            storageGetP(
                storageNewReadNP(storageLocal(), strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(pgPath))),
                .exactSize = PG_CONTROL_DATA_SIZE));

        // Get PostgreSQL version
        result.systemId = control->systemId;
        result.controlVersion = control->controlVersion;
        result.catalogVersion = control->catalogVersion;
        result.version = pgVersionMap(result.controlVersion, result.catalogVersion);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(PG_CONTROL_INFO, result);
}
