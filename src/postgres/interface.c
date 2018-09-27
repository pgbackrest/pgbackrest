/***********************************************************************************************************************************
PostgreSQL Interface
***********************************************************************************************************************************/
#include <string.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "postgres/interface.h"
#include "postgres/interface/v083.h"
#include "postgres/interface/v084.h"
#include "postgres/interface/v090.h"
#include "postgres/interface/v091.h"
#include "postgres/interface/v092.h"
#include "postgres/interface/v093.h"
#include "postgres/interface/v094.h"
#include "postgres/interface/v095.h"
#include "postgres/interface/v096.h"
#include "postgres/interface/v100.h"
#include "postgres/interface/v110.h"
#include "postgres/version.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Define default page size

Page size can only be changed at compile time and is not known to be well-tested, so only the default page size is supported.
***********************************************************************************************************************************/
#define PG_PAGE_SIZE_DEFAULT                                        ((unsigned int)(8 * 1024))

/***********************************************************************************************************************************
Define default wal segment size

Page size can only be changed at compile time and and is not known to be well-tested, so only the default page size is supported.
***********************************************************************************************************************************/
#define PG_WAL_SEGMENT_SIZE_DEFAULT                                 ((unsigned int)(16 * 1024 * 1024))

/***********************************************************************************************************************************
Control file size.  The control file is actually 8192 bytes but only the first 512 bytes are used to prevent torn pages even on
really old storage with 512-byte sectors.  This is true across all versions of PostgreSQL.
***********************************************************************************************************************************/
#define PG_CONTROL_SIZE                                             ((unsigned int)(8 * 1024))
#define PG_CONTROL_DATA_SIZE                                        ((unsigned int)(512))

/***********************************************************************************************************************************
PostgreSQL interface definitions
***********************************************************************************************************************************/
typedef struct PgInterface
{
    unsigned int version;
    PgControl (*control)(const Buffer *);
    bool (*is)(const Buffer *);
    void (*controlTest)(PgControl, Buffer *);
} PgInterface;

static const PgInterface pgInterface[] =
{
    {
        .version = PG_VERSION_11,
        .control = pgInterfaceControl110,
        .is = pgInterfaceIs110,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest110,
#endif
    },
    {
        .version = PG_VERSION_10,
        .control = pgInterfaceControl100,
        .is = pgInterfaceIs100,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest100,
#endif
    },
    {
        .version = PG_VERSION_96,
        .control = pgInterfaceControl096,
        .is = pgInterfaceIs096,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest096,
#endif
    },
    {
        .version = PG_VERSION_95,
        .control = pgInterfaceControl095,
        .is = pgInterfaceIs095,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest095,
#endif
    },
    {
        .version = PG_VERSION_94,
        .control = pgInterfaceControl094,
        .is = pgInterfaceIs094,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest094,
#endif
    },
    {
        .version = PG_VERSION_93,
        .control = pgInterfaceControl093,
        .is = pgInterfaceIs093,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest093,
#endif
    },
    {
        .version = PG_VERSION_92,
        .control = pgInterfaceControl092,
        .is = pgInterfaceIs092,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest092,
#endif
    },
    {
        .version = PG_VERSION_91,
        .control = pgInterfaceControl091,
        .is = pgInterfaceIs091,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest091,
#endif
    },
    {
        .version = PG_VERSION_90,
        .control = pgInterfaceControl090,
        .is = pgInterfaceIs090,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest090,
#endif
    },
    {
        .version = PG_VERSION_84,
        .control = pgInterfaceControl084,
        .is = pgInterfaceIs084,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest084,
#endif
    },
    {
        .version = PG_VERSION_83,
        .control = pgInterfaceControl083,
        .is = pgInterfaceIs083,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest083,
#endif
    },
};

/***********************************************************************************************************************************
These pg_control fields are common to all versions of PostgreSQL, so we can use them to generate error messages when the pg_control
version cannot be found.
***********************************************************************************************************************************/
typedef struct PgControlCommon
{
    uint64_t systemId;
    uint32_t controlVersion;
    uint32_t catalogVersion;
} PgControlCommon;

/***********************************************************************************************************************************
Get info from pg_control
***********************************************************************************************************************************/
PgControl
pgControlFromBuffer(const Buffer *controlFile)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(BUFFER, controlFile);

        FUNCTION_TEST_ASSERT(controlFile != NULL);
    FUNCTION_DEBUG_END();

    // Search for the version of PostgreSQL that uses this control file
    const PgInterface *interface = NULL;

    for (unsigned int interfaceIdx = 0; interfaceIdx < sizeof(pgInterface) / sizeof(PgInterface); interfaceIdx++)
    {
        if (pgInterface[interfaceIdx].is(controlFile))
        {
            interface = &pgInterface[interfaceIdx];
            break;
        }
    }

    // If the version was not found then error with the control and catalog version that were found
    if (interface == NULL)
    {
        PgControlCommon *controlCommon = (PgControlCommon *)bufPtr(controlFile);

        THROW_FMT(
            VersionNotSupportedError,
            "unexpected control version = %u and catalog version = %u\n"
                "HINT: is this version of PostgreSQL supported?",
            controlCommon->controlVersion, controlCommon->catalogVersion);
    }

    // Get info from the control file
    PgControl result = interface->control(controlFile);
    result.version = interface->version;

    // Check the segment size
    if (result.version < PG_VERSION_11 && result.walSegmentSize != PG_WAL_SEGMENT_SIZE_DEFAULT)
    {
        THROW_FMT(
            FormatError, "wal segment size is %u but must be %u for " PG_NAME " <= " PG_VERSION_10_STR, result.walSegmentSize,
            PG_WAL_SEGMENT_SIZE_DEFAULT);
    }

    // Check the page size
    if (result.pageSize != PG_PAGE_SIZE_DEFAULT)
        THROW_FMT(FormatError, "page size is %u but must be %u", result.pageSize, PG_PAGE_SIZE_DEFAULT);

    FUNCTION_DEBUG_RESULT(PG_CONTROL, result);
}

/***********************************************************************************************************************************
Get info from pg_control
***********************************************************************************************************************************/
PgControl
pgControlFromFile(const String *pgPath)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STRING, pgPath);

        FUNCTION_TEST_ASSERT(pgPath != NULL);
    FUNCTION_DEBUG_END();

    PgControl result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Read control file
        Buffer *controlFile = storageGetP(
            storageNewReadNP(storageLocal(), strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strPtr(pgPath))),
            .exactSize = PG_CONTROL_DATA_SIZE);

        result = pgControlFromBuffer(controlFile);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(PG_CONTROL, result);
}

/***********************************************************************************************************************************
Create pg_control for testing
***********************************************************************************************************************************/
#ifdef DEBUG

Buffer *
pgControlTestToBuffer(PgControl pgControl)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PG_CONTROL, pgControl);
    FUNCTION_TEST_END();

    // Set defaults if values are not passed
    pgControl.pageSize = pgControl.pageSize == 0 ? PG_PAGE_SIZE_DEFAULT : pgControl.pageSize;
    pgControl.walSegmentSize = pgControl.walSegmentSize == 0 ? PG_WAL_SEGMENT_SIZE_DEFAULT : pgControl.walSegmentSize;

    // Create the buffer and clear it
    Buffer *result = bufNew(PG_CONTROL_SIZE);
    memset(bufPtr(result), 0, bufSize(result));
    bufUsedSet(result, bufSize(result));

    // Find the interface for the version of PostgreSQL
    const PgInterface *interface = NULL;

    for (unsigned int interfaceIdx = 0; interfaceIdx < sizeof(pgInterface) / sizeof(PgInterface); interfaceIdx++)
    {
        if (pgInterface[interfaceIdx].version == pgControl.version)
        {
            interface = &pgInterface[interfaceIdx];
            break;
        }
    }

    // If the version was not found then error
    if (interface == NULL)
        THROW_FMT(AssertError, "invalid version %u", pgControl.version);

    // Generate pg_control
    interface->controlTest(pgControl, result);

    FUNCTION_TEST_RESULT(BUFFER, result);
}

#endif

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
Render as string for logging
***********************************************************************************************************************************/
String *
pgControlToLog(const PgControl *pgControl)
{
    return strNewFmt(
        "{version: %u, systemId: %" PRIu64 ", walSegmentSize: %u, pageChecksum: %s}", pgControl->version, pgControl->systemId,
        pgControl->walSegmentSize, cvtBoolToConstZ(pgControl->pageChecksum));
}
