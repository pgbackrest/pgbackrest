/***********************************************************************************************************************************
PostgreSQL Interface
***********************************************************************************************************************************/
#include <string.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "postgres/interface.h"
#include "postgres/interface/version.h"
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
WAL header size.  It doesn't seem worth tracking the exact size of the WAL header across versions of PostgreSQL so just set it to
something far larger needed but <= the minimum read size on just about any system.
***********************************************************************************************************************************/
#define PG_WAL_HEADER_SIZE                                          ((unsigned int)(512))

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
    bool (*controlIs)(const unsigned char *);

    // Convert pg_control to a common data structure
    PgControl (*control)(const unsigned char *);

    // Does the WAL header match this version of PostgreSQL?
    bool (*walIs)(const unsigned char *);

    // Convert WAL header to a common data structure
    PgWal (*wal)(const unsigned char *);

#ifdef DEBUG

    // Create pg_control for testing
    void (*controlTest)(PgControl, unsigned char *);

    // Create WAL header for testing
    void (*walTest)(PgWal, unsigned char *);
#endif
} PgInterface;

static const PgInterface pgInterface[] =
{
    {
        .version = PG_VERSION_11,

        .controlIs = pgInterfaceControlIs110,
        .control = pgInterfaceControl110,

        .walIs = pgInterfaceWalIs110,
        .wal = pgInterfaceWal110,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest110,
        .walTest = pgInterfaceWalTest110,
#endif
    },
    {
        .version = PG_VERSION_10,

        .controlIs = pgInterfaceControlIs100,
        .control = pgInterfaceControl100,

        .walIs = pgInterfaceWalIs100,
        .wal = pgInterfaceWal100,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest100,
        .walTest = pgInterfaceWalTest100,
#endif
    },
    {
        .version = PG_VERSION_96,

        .controlIs = pgInterfaceControlIs096,
        .control = pgInterfaceControl096,

        .walIs = pgInterfaceWalIs096,
        .wal = pgInterfaceWal096,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest096,
        .walTest = pgInterfaceWalTest096,
#endif
    },
    {
        .version = PG_VERSION_95,

        .controlIs = pgInterfaceControlIs095,
        .control = pgInterfaceControl095,

        .walIs = pgInterfaceWalIs095,
        .wal = pgInterfaceWal095,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest095,
        .walTest = pgInterfaceWalTest095,
#endif
    },
    {
        .version = PG_VERSION_94,

        .controlIs = pgInterfaceControlIs094,
        .control = pgInterfaceControl094,

        .walIs = pgInterfaceWalIs094,
        .wal = pgInterfaceWal094,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest094,
        .walTest = pgInterfaceWalTest094,
#endif
    },
    {
        .version = PG_VERSION_93,

        .controlIs = pgInterfaceControlIs093,
        .control = pgInterfaceControl093,

        .walIs = pgInterfaceWalIs093,
        .wal = pgInterfaceWal093,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest093,
        .walTest = pgInterfaceWalTest093,
#endif
    },
    {
        .version = PG_VERSION_92,

        .controlIs = pgInterfaceControlIs092,
        .control = pgInterfaceControl092,

        .walIs = pgInterfaceWalIs092,
        .wal = pgInterfaceWal092,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest092,
        .walTest = pgInterfaceWalTest092,
#endif
    },
    {
        .version = PG_VERSION_91,

        .controlIs = pgInterfaceControlIs091,
        .control = pgInterfaceControl091,

        .walIs = pgInterfaceWalIs091,
        .wal = pgInterfaceWal091,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest091,
        .walTest = pgInterfaceWalTest091,
#endif
    },
    {
        .version = PG_VERSION_90,

        .controlIs = pgInterfaceControlIs090,
        .control = pgInterfaceControl090,

        .walIs = pgInterfaceWalIs090,
        .wal = pgInterfaceWal090,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest090,
        .walTest = pgInterfaceWalTest090,
#endif
    },
    {
        .version = PG_VERSION_84,

        .controlIs = pgInterfaceControlIs084,
        .control = pgInterfaceControl084,

        .walIs = pgInterfaceWalIs084,
        .wal = pgInterfaceWal084,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest084,
        .walTest = pgInterfaceWalTest084,
#endif
    },
    {
        .version = PG_VERSION_83,

        .controlIs = pgInterfaceControlIs083,
        .control = pgInterfaceControl083,

        .walIs = pgInterfaceWalIs083,
        .wal = pgInterfaceWal083,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest083,
        .walTest = pgInterfaceWalTest083,
#endif
    },
};

// Total PostgreSQL versions in pgInterface
#define PG_INTERFACE_SIZE                                           (sizeof(pgInterface) / sizeof(PgInterface))

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

    for (unsigned int interfaceIdx = 0; interfaceIdx < PG_INTERFACE_SIZE; interfaceIdx++)
    {
        if (pgInterface[interfaceIdx].controlIs(bufPtr(controlFile)))
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
    PgControl result = interface->control(bufPtr(controlFile));
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
These WAL header fields are common to all versions of PostgreSQL, so we can use them to generate error messages when the WAL magic
cannot be found.
***********************************************************************************************************************************/
typedef struct PgWalCommon
{
    uint16_t magic;
    uint16_t flag;
} PgWalCommon;

#define PG_WAL_LONG_HEADER                                          0x0002

/***********************************************************************************************************************************
Get info from WAL header
***********************************************************************************************************************************/
PgWal
pgWalFromBuffer(const Buffer *walBuffer)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BUFFER, walBuffer);
    FUNCTION_LOG_END();

    ASSERT(walBuffer != NULL);

    // Check that this is a long format WAL header
    if (!(((PgWalCommon *)bufPtr(walBuffer))->flag & PG_WAL_LONG_HEADER))
        THROW_FMT(FormatError, "first page header in WAL file is expected to be in long format");

    // Search for the version of PostgreSQL that uses this WAL magic
    const PgInterface *interface = NULL;

    for (unsigned int interfaceIdx = 0; interfaceIdx < PG_INTERFACE_SIZE; interfaceIdx++)
    {
        if (pgInterface[interfaceIdx].walIs(bufPtr(walBuffer)))
        {
            interface = &pgInterface[interfaceIdx];
            break;
        }
    }

    // If the version was not found then error with the magic that was found
    if (interface == NULL)
    {
        THROW_FMT(
            VersionNotSupportedError,
            "unexpected WAL magic %u\n"
                "HINT: is this version of PostgreSQL supported?",
            ((PgWalCommon *)bufPtr(walBuffer))->magic);
    }

    // Get info from the control file
    PgWal result = interface->wal(bufPtr(walBuffer));
    result.version = interface->version;

    FUNCTION_LOG_RETURN(PG_WAL, result);
}

/***********************************************************************************************************************************
Get info from a WAL segment
***********************************************************************************************************************************/
PgWal
pgWalFromFile(const String *walFile)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, walFile);
    FUNCTION_LOG_END();

    ASSERT(walFile != NULL);

    PgWal result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Read WAL segment header
        Buffer *walBuffer = storageGetP(storageNewReadNP(storageLocal(), walFile), .exactSize = PG_WAL_HEADER_SIZE);

        result = pgWalFromBuffer(walBuffer);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PG_WAL, result);
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

    for (unsigned int interfaceIdx = 0; interfaceIdx < PG_INTERFACE_SIZE; interfaceIdx++)
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
    interface->controlTest(pgControl, bufPtr(result));

    FUNCTION_TEST_RETURN(result);
}

void
pgWalTestToBuffer(PgWal pgWal, Buffer *walBuffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PG_WAL, pgWal);
        FUNCTION_TEST_PARAM(BUFFER, walBuffer);
    FUNCTION_TEST_END();

    ASSERT(walBuffer != NULL);

    // Find the interface for the version of PostgreSQL
    const PgInterface *interface = NULL;

    for (unsigned int interfaceIdx = 0; interfaceIdx < PG_INTERFACE_SIZE; interfaceIdx++)
    {
        if (pgInterface[interfaceIdx].version == pgWal.version)
        {
            interface = &pgInterface[interfaceIdx];
            break;
        }
    }

    // If the version was not found then error
    if (interface == NULL)
        THROW_FMT(AssertError, "invalid version %u", pgWal.version);

    // Generate pg_control
    interface->walTest(pgWal, bufPtr(walBuffer));

    FUNCTION_TEST_RETURN_VOID();
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

String *
pgWalToLog(const PgWal *pgWal)
{
    return strNewFmt("{version: %u, systemId: %" PRIu64 "}", pgWal->version, pgWal->systemId);
}
