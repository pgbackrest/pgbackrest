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

Each supported version of PostgreSQL must have interface files named postgres/interface/vXXX.c/h that implement the functions
specified in the interface structure below.  The functions are documented here rather than in the interface files so that a change
in wording does not need to be propagated through N source files.
***********************************************************************************************************************************/
typedef struct PgInterface
{
    // Version of PostgreSQL supported by this interface
    unsigned int version;

    // Does pg_control match this version of PostgreSQL?
    bool (*controlIs)(const Buffer *);

    // Convert pg_control to a common data structure
    PgControl (*control)(const Buffer *);

#ifdef DEBUG
    // Create pg_control for testing
    void (*controlTest)(PgControl, Buffer *);
#endif
} PgInterface;

static const PgInterface pgInterface[] =
{
    {
        .version = PG_VERSION_11,

        .controlIs = pgInterfaceControlIs110,
        .control = pgInterfaceControl110,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest110,
#endif
    },
    {
        .version = PG_VERSION_10,

        .controlIs = pgInterfaceControlIs100,
        .control = pgInterfaceControl100,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest100,
#endif
    },
    {
        .version = PG_VERSION_96,

        .controlIs = pgInterfaceControlIs096,
        .control = pgInterfaceControl096,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest096,
#endif
    },
    {
        .version = PG_VERSION_95,

        .controlIs = pgInterfaceControlIs095,
        .control = pgInterfaceControl095,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest095,
#endif
    },
    {
        .version = PG_VERSION_94,

        .controlIs = pgInterfaceControlIs094,
        .control = pgInterfaceControl094,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest094,
#endif
    },
    {
        .version = PG_VERSION_93,

        .controlIs = pgInterfaceControlIs093,
        .control = pgInterfaceControl093,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest093,
#endif
    },
    {
        .version = PG_VERSION_92,

        .controlIs = pgInterfaceControlIs092,
        .control = pgInterfaceControl092,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest092,
#endif
    },
    {
        .version = PG_VERSION_91,

        .controlIs = pgInterfaceControlIs091,
        .control = pgInterfaceControl091,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest091,
#endif
    },
    {
        .version = PG_VERSION_90,

        .controlIs = pgInterfaceControlIs090,
        .control = pgInterfaceControl090,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest090,
#endif
    },
    {
        .version = PG_VERSION_84,

        .controlIs = pgInterfaceControlIs084,
        .control = pgInterfaceControl084,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest084,
#endif
    },
    {
        .version = PG_VERSION_83,

        .controlIs = pgInterfaceControlIs083,
        .control = pgInterfaceControl083,

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
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BUFFER, controlFile);
    FUNCTION_LOG_END();

    ASSERT(controlFile != NULL);

    // Search for the version of PostgreSQL that uses this control file
    const PgInterface *interface = NULL;

    for (unsigned int interfaceIdx = 0; interfaceIdx < sizeof(pgInterface) / sizeof(PgInterface); interfaceIdx++)
    {
        if (pgInterface[interfaceIdx].controlIs(controlFile))
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

    FUNCTION_LOG_RETURN(PG_CONTROL, result);
}

/***********************************************************************************************************************************
Get info from pg_control
***********************************************************************************************************************************/
PgControl
pgControlFromFile(const String *pgPath)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, pgPath);
    FUNCTION_LOG_END();

    ASSERT(pgPath != NULL);

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

    FUNCTION_LOG_RETURN(PG_CONTROL, result);
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

    FUNCTION_TEST_RETURN(result);
}

#endif

/***********************************************************************************************************************************
Convert version string to version number
***********************************************************************************************************************************/
unsigned int
pgVersionFromStr(const String *version)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, version);
    FUNCTION_LOG_END();

    ASSERT(version != NULL);

    unsigned int result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If format is not number.number (9.4) or number only (10) then error
        if (!regExpMatchOne(STRING_CONST("^[0-9]+[.]*[0-9]+$"), version))
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

    FUNCTION_LOG_RETURN(UINT, result);
}

/***********************************************************************************************************************************
Convert version number to string
***********************************************************************************************************************************/
String *
pgVersionToStr(unsigned int version)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(UINT, version);
    FUNCTION_LOG_END();

    String *result = version >= PG_VERSION_10 ?
        strNewFmt("%u", version / 10000) : strNewFmt("%u.%u", version / 10000, version % 10000 / 100);

    FUNCTION_LOG_RETURN(STRING, result);
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
