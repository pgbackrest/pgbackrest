/***********************************************************************************************************************************
PostgreSQL Interface
***********************************************************************************************************************************/
#include "build.auto.h"

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
Defines for various Postgres paths and files
***********************************************************************************************************************************/
STRING_EXTERN(PG_FILE_PGVERSION_STR,                                PG_FILE_PGVERSION);
STRING_EXTERN(PG_FILE_POSTGRESQLAUTOCONF_STR,                       PG_FILE_POSTGRESQLAUTOCONF);
STRING_EXTERN(PG_FILE_POSTMASTERPID_STR,                            PG_FILE_POSTMASTERPID);
STRING_EXTERN(PG_FILE_RECOVERYCONF_STR,                             PG_FILE_RECOVERYCONF);
STRING_EXTERN(PG_FILE_RECOVERYDONE_STR,                             PG_FILE_RECOVERYDONE);
STRING_EXTERN(PG_FILE_RECOVERYSIGNAL_STR,                           PG_FILE_RECOVERYSIGNAL);
STRING_EXTERN(PG_FILE_STANDBYSIGNAL_STR,                            PG_FILE_STANDBYSIGNAL);

STRING_EXTERN(PG_PATH_GLOBAL_STR,                                   PG_PATH_GLOBAL);

STRING_EXTERN(PG_NAME_WAL_STR,                                      PG_NAME_WAL);
STRING_EXTERN(PG_NAME_XLOG_STR,                                     PG_NAME_XLOG);

// Wal path names depending on version
STRING_STATIC(PG_PATH_PGWAL_STR,                                    "pg_wal");
STRING_STATIC(PG_PATH_PGXLOG_STR,                                   "pg_xlog");

// Transaction commit log path names depending on version
STRING_STATIC(PG_PATH_PGCLOG_STR,                                   "pg_clog");
STRING_STATIC(PG_PATH_PGXACT_STR,                                   "pg_xact");

// Lsn name used in functions depnding on version
STRING_STATIC(PG_NAME_LSN_STR,                                      "lsn");
STRING_STATIC(PG_NAME_LOCATION_STR,                                 "location");

/***********************************************************************************************************************************
Control file size.  The control file is actually 8192 bytes but only the first 512 bytes are used to prevent torn pages even on
really old storage with 512-byte sectors.  This is true across all versions of PostgreSQL.
***********************************************************************************************************************************/
#define PG_CONTROL_SIZE                                             ((unsigned int)(8 * 1024))
#define PG_CONTROL_DATA_SIZE                                        ((unsigned int)(512))

/***********************************************************************************************************************************
Name of default PostgreSQL database used for running all queries and commands
***********************************************************************************************************************************/
STRING_EXTERN(PG_DB_POSTGRES_STR,                                   PG_DB_POSTGRES);

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

    // Get the catalog version for this version of PostgreSQL
    uint32_t (*catalogVersion)(void);

    // Does pg_control match this version of PostgreSQL?
    bool (*controlIs)(const unsigned char *);

    // Convert pg_control to a common data structure
    PgControl (*control)(const unsigned char *);

    // Get the control version for this version of PostgreSQL
    uint32_t (*controlVersion)(void);

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
        .version = PG_VERSION_13,

        .catalogVersion = pgInterfaceCatalogVersion130,

        .controlIs = pgInterfaceControlIs130,
        .control = pgInterfaceControl130,
        .controlVersion = pgInterfaceControlVersion130,

        .walIs = pgInterfaceWalIs130,
        .wal = pgInterfaceWal130,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest130,
        .walTest = pgInterfaceWalTest130,
#endif
    },
    {
        .version = PG_VERSION_12,

        .catalogVersion = pgInterfaceCatalogVersion120,

        .controlIs = pgInterfaceControlIs120,
        .control = pgInterfaceControl120,
        .controlVersion = pgInterfaceControlVersion120,

        .walIs = pgInterfaceWalIs120,
        .wal = pgInterfaceWal120,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest120,
        .walTest = pgInterfaceWalTest120,
#endif
    },
    {
        .version = PG_VERSION_11,

        .catalogVersion = pgInterfaceCatalogVersion110,

        .controlIs = pgInterfaceControlIs110,
        .control = pgInterfaceControl110,
        .controlVersion = pgInterfaceControlVersion110,

        .walIs = pgInterfaceWalIs110,
        .wal = pgInterfaceWal110,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest110,
        .walTest = pgInterfaceWalTest110,
#endif
    },
    {
        .version = PG_VERSION_10,

        .catalogVersion = pgInterfaceCatalogVersion100,

        .controlIs = pgInterfaceControlIs100,
        .control = pgInterfaceControl100,
        .controlVersion = pgInterfaceControlVersion100,

        .walIs = pgInterfaceWalIs100,
        .wal = pgInterfaceWal100,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest100,
        .walTest = pgInterfaceWalTest100,
#endif
    },
    {
        .version = PG_VERSION_96,

        .catalogVersion = pgInterfaceCatalogVersion096,

        .controlIs = pgInterfaceControlIs096,
        .control = pgInterfaceControl096,
        .controlVersion = pgInterfaceControlVersion096,

        .walIs = pgInterfaceWalIs096,
        .wal = pgInterfaceWal096,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest096,
        .walTest = pgInterfaceWalTest096,
#endif
    },
    {
        .version = PG_VERSION_95,

        .catalogVersion = pgInterfaceCatalogVersion095,

        .controlIs = pgInterfaceControlIs095,
        .control = pgInterfaceControl095,
        .controlVersion = pgInterfaceControlVersion095,

        .walIs = pgInterfaceWalIs095,
        .wal = pgInterfaceWal095,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest095,
        .walTest = pgInterfaceWalTest095,
#endif
    },
    {
        .version = PG_VERSION_94,

        .catalogVersion = pgInterfaceCatalogVersion094,

        .controlIs = pgInterfaceControlIs094,
        .control = pgInterfaceControl094,
        .controlVersion = pgInterfaceControlVersion094,

        .walIs = pgInterfaceWalIs094,
        .wal = pgInterfaceWal094,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest094,
        .walTest = pgInterfaceWalTest094,
#endif
    },
    {
        .version = PG_VERSION_93,

        .catalogVersion = pgInterfaceCatalogVersion093,

        .controlIs = pgInterfaceControlIs093,
        .control = pgInterfaceControl093,
        .controlVersion = pgInterfaceControlVersion093,

        .walIs = pgInterfaceWalIs093,
        .wal = pgInterfaceWal093,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest093,
        .walTest = pgInterfaceWalTest093,
#endif
    },
    {
        .version = PG_VERSION_92,

        .catalogVersion = pgInterfaceCatalogVersion092,

        .controlIs = pgInterfaceControlIs092,
        .control = pgInterfaceControl092,
        .controlVersion = pgInterfaceControlVersion092,

        .walIs = pgInterfaceWalIs092,
        .wal = pgInterfaceWal092,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest092,
        .walTest = pgInterfaceWalTest092,
#endif
    },
    {
        .version = PG_VERSION_91,

        .catalogVersion = pgInterfaceCatalogVersion091,

        .controlIs = pgInterfaceControlIs091,
        .control = pgInterfaceControl091,
        .controlVersion = pgInterfaceControlVersion091,

        .walIs = pgInterfaceWalIs091,
        .wal = pgInterfaceWal091,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest091,
        .walTest = pgInterfaceWalTest091,
#endif
    },
    {
        .version = PG_VERSION_90,

        .catalogVersion = pgInterfaceCatalogVersion090,

        .controlIs = pgInterfaceControlIs090,
        .control = pgInterfaceControl090,
        .controlVersion = pgInterfaceControlVersion090,

        .walIs = pgInterfaceWalIs090,
        .wal = pgInterfaceWal090,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest090,
        .walTest = pgInterfaceWalTest090,
#endif
    },
    {
        .version = PG_VERSION_84,

        .catalogVersion = pgInterfaceCatalogVersion084,

        .controlIs = pgInterfaceControlIs084,
        .control = pgInterfaceControl084,
        .controlVersion = pgInterfaceControlVersion084,

        .walIs = pgInterfaceWalIs084,
        .wal = pgInterfaceWal084,

#ifdef DEBUG
        .controlTest = pgInterfaceControlTest084,
        .walTest = pgInterfaceWalTest084,
#endif
    },
    {
        .version = PG_VERSION_83,

        .catalogVersion = pgInterfaceCatalogVersion083,

        .controlIs = pgInterfaceControlIs083,
        .control = pgInterfaceControl083,
        .controlVersion = pgInterfaceControlVersion083,

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
Get the interface for a PostgreSQL version
***********************************************************************************************************************************/
static const PgInterface *
pgInterfaceVersion(unsigned int pgVersion)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
    FUNCTION_TEST_END();

    const PgInterface *result = NULL;

    for (unsigned int interfaceIdx = 0; interfaceIdx < PG_INTERFACE_SIZE; interfaceIdx++)
    {
        if (pgInterface[interfaceIdx].version == pgVersion)
        {
            result = &pgInterface[interfaceIdx];
            break;
        }
    }

    // If the version was not found then error
    if (result == NULL)
        THROW_FMT(AssertError, "invalid " PG_NAME " version %u", pgVersion);

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
uint32_t
pgCatalogVersion(unsigned int pgVersion)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(pgInterfaceVersion(pgVersion)->catalogVersion());
}

/***********************************************************************************************************************************
Check expected WAL segment size for older PostgreSQL versions
***********************************************************************************************************************************/
static void
pgWalSegmentSizeCheck(unsigned int pgVersion, unsigned int walSegmentSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
        FUNCTION_TEST_PARAM(UINT, walSegmentSize);
    FUNCTION_TEST_END();

    if (pgVersion < PG_VERSION_11 && walSegmentSize != PG_WAL_SEGMENT_SIZE_DEFAULT)
    {
        THROW_FMT(
            FormatError, "wal segment size is %u but must be %u for " PG_NAME " <= " PG_VERSION_10_STR, walSegmentSize,
            PG_WAL_SEGMENT_SIZE_DEFAULT);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
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
        if (pgInterface[interfaceIdx].controlIs(bufPtrConst(controlFile)))
        {
            interface = &pgInterface[interfaceIdx];
            break;
        }
    }

    // If the version was not found then error with the control and catalog version that were found
    if (interface == NULL)
    {
        const PgControlCommon *controlCommon = (const PgControlCommon *)bufPtrConst(controlFile);

        THROW_FMT(
            VersionNotSupportedError,
            "unexpected control version = %u and catalog version = %u\n"
                "HINT: is this version of PostgreSQL supported?",
            controlCommon->controlVersion, controlCommon->catalogVersion);
    }

    // Get info from the control file
    PgControl result = interface->control(bufPtrConst(controlFile));
    result.version = interface->version;

    // Check the segment size
    pgWalSegmentSizeCheck(result.version, result.walSegmentSize);

    // Check the page size
    if (result.pageSize != PG_PAGE_SIZE_DEFAULT)
        THROW_FMT(FormatError, "page size is %u but must be %u", result.pageSize, PG_PAGE_SIZE_DEFAULT);

    FUNCTION_LOG_RETURN(PG_CONTROL, result);
}

PgControl
pgControlFromFile(const Storage *storage)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);

    PgControl result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Read control file
        Buffer *controlFile = storageGetP(
            storageNewReadP(storage, STRDEF(PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL)), .exactSize = PG_CONTROL_DATA_SIZE);

        result = pgControlFromBuffer(controlFile);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PG_CONTROL, result);
}

/**********************************************************************************************************************************/
uint32_t
pgControlVersion(unsigned int pgVersion)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(pgInterfaceVersion(pgVersion)->controlVersion());
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

/**********************************************************************************************************************************/
PgWal
pgWalFromBuffer(const Buffer *walBuffer)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BUFFER, walBuffer);
    FUNCTION_LOG_END();

    ASSERT(walBuffer != NULL);

    // Check that this is a long format WAL header
    if (!(((const PgWalCommon *)bufPtrConst(walBuffer))->flag & PG_WAL_LONG_HEADER))
        THROW_FMT(FormatError, "first page header in WAL file is expected to be in long format");

    // Search for the version of PostgreSQL that uses this WAL magic
    const PgInterface *interface = NULL;

    for (unsigned int interfaceIdx = 0; interfaceIdx < PG_INTERFACE_SIZE; interfaceIdx++)
    {
        if (pgInterface[interfaceIdx].walIs(bufPtrConst(walBuffer)))
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
            ((const PgWalCommon *)bufPtrConst(walBuffer))->magic);
    }

    // Get info from the control file
    PgWal result = interface->wal(bufPtrConst(walBuffer));
    result.version = interface->version;

    // Check the segment size
    pgWalSegmentSizeCheck(result.version, result.size);

    FUNCTION_LOG_RETURN(PG_WAL, result);
}

PgWal
pgWalFromFile(const String *walFile, const Storage *storage)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, walFile);
    FUNCTION_LOG_END();

    ASSERT(walFile != NULL);

    PgWal result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Read WAL segment header
        Buffer *walBuffer = storageGetP(storageNewReadP(storage, walFile), .exactSize = PG_WAL_HEADER_SIZE);

        result = pgWalFromBuffer(walBuffer);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PG_WAL, result);
}

/**********************************************************************************************************************************/
String *
pgTablespaceId(unsigned int pgVersion)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
    FUNCTION_TEST_END();

    String *result = NULL;

    if (pgVersion >= PG_VERSION_90)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            String *pgVersionStr = pgVersionToStr(pgVersion);

            MEM_CONTEXT_PRIOR_BEGIN()
            {
                result = strNewFmt("PG_%s_%u", strZ(pgVersionStr), pgCatalogVersion(pgVersion));
            }
            MEM_CONTEXT_PRIOR_END();
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
uint64_t
pgLsnFromStr(const String *lsn)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, lsn);
    FUNCTION_TEST_END();

    uint64_t result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        StringList *lsnPart = strLstNewSplit(lsn, FSLASH_STR);

        CHECK(strLstSize(lsnPart) == 2);

        result = (cvtZToUInt64Base(strZ(strLstGet(lsnPart, 0)), 16) << 32) + cvtZToUInt64Base(strZ(strLstGet(lsnPart, 1)), 16);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

String *
pgLsnToStr(uint64_t lsn)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT64, lsn);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(strNewFmt("%x/%x", (unsigned int)(lsn >> 32), (unsigned int)(lsn & 0xFFFFFFFF)));
}

/**********************************************************************************************************************************/
String *
pgLsnToWalSegment(uint32_t timeline, uint64_t lsn, unsigned int walSegmentSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, timeline);
        FUNCTION_TEST_PARAM(UINT64, lsn);
        FUNCTION_TEST_PARAM(UINT, walSegmentSize);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(
        strNewFmt("%08X%08X%08X", timeline, (unsigned int)(lsn >> 32), (unsigned int)(lsn & 0xFFFFFFFF) / walSegmentSize));
}

uint64_t
pgLsnFromWalSegment(const String *walSegment, unsigned int walSegmentSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, walSegment);
        FUNCTION_TEST_PARAM(UINT, walSegmentSize);
    FUNCTION_TEST_END();

    ASSERT(walSegment != NULL);
    ASSERT(strSize(walSegment) == 24);
    ASSERT(walSegmentSize > 0);

    FUNCTION_TEST_RETURN(
        (cvtZToUInt64Base(strZ(strSubN(walSegment, 8, 8)), 16) << 32) +
        (cvtZToUInt64Base(strZ(strSubN(walSegment, 16, 8)), 16) * walSegmentSize));
}

/**********************************************************************************************************************************/
StringList *
pgLsnRangeToWalSegmentList(
    unsigned int pgVersion, uint32_t timeline, uint64_t lsnStart, uint64_t lsnStop, unsigned int walSegmentSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
        FUNCTION_TEST_PARAM(UINT, timeline);
        FUNCTION_TEST_PARAM(UINT64, lsnStart);
        FUNCTION_TEST_PARAM(UINT64, lsnStop);
        FUNCTION_TEST_PARAM(UINT, walSegmentSize);
    FUNCTION_TEST_END();

    ASSERT(pgVersion != 0);
    ASSERT(timeline != 0);
    ASSERT(lsnStart <= lsnStop);
    ASSERT(walSegmentSize != 0);
    ASSERT(pgVersion > PG_VERSION_92 || walSegmentSize == PG_WAL_SEGMENT_SIZE_DEFAULT);

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = strLstNew();

        // Skip the FF segment when PostgreSQL <= 9.2 (in this case segment size should always be 16MB)
        bool skipFF = pgVersion <= PG_VERSION_92;

        // Calculate the start and stop segments
        unsigned int startMajor = (unsigned int)(lsnStart >> 32);
        unsigned int startMinor = (unsigned int)(lsnStart & 0xFFFFFFFF) / walSegmentSize;

        unsigned int stopMajor = (unsigned int)(lsnStop >> 32);
        unsigned int stopMinor = (unsigned int)(lsnStop & 0xFFFFFFFF) / walSegmentSize;

        unsigned int minorPerMajor = 0xFFFFFFFF / walSegmentSize;

        // Create list
        strLstAdd(result, strNewFmt("%08X%08X%08X", timeline, startMajor, startMinor));

        while (!(startMajor == stopMajor && startMinor == stopMinor))
        {
            startMinor++;

            if ((skipFF && startMinor == 0xFF) || (!skipFF && startMinor > minorPerMajor))
            {
                startMajor++;
                startMinor = 0;
            }

            strLstAdd(result, strNewFmt("%08X%08X%08X", timeline, startMajor, startMinor));
        }

        strLstMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
const String *
pgLsnName(unsigned int pgVersion)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(pgVersion >= PG_VERSION_WAL_RENAME ? PG_NAME_LSN_STR : PG_NAME_LOCATION_STR);
}

/***********************************************************************************************************************************
Get WAL name (wal/xlog) for a PostgreSQL version
***********************************************************************************************************************************/
const String *
pgWalName(unsigned int pgVersion)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(pgVersion >= PG_VERSION_WAL_RENAME ? PG_NAME_WAL_STR : PG_NAME_XLOG_STR);
}

/**********************************************************************************************************************************/
const String *
pgWalPath(unsigned int pgVersion)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(pgVersion >= PG_VERSION_WAL_RENAME ? PG_PATH_PGWAL_STR : PG_PATH_PGXLOG_STR);
}

/**********************************************************************************************************************************/
const String *
pgXactPath(unsigned int pgVersion)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(pgVersion >= PG_VERSION_WAL_RENAME ? PG_PATH_PGXACT_STR : PG_PATH_PGCLOG_STR);
}

/**********************************************************************************************************************************/
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

    // Generate pg_control
    pgInterfaceVersion(pgControl.version)->controlTest(pgControl, bufPtr(result));

    FUNCTION_TEST_RETURN(result);
}

#endif

/**********************************************************************************************************************************/
#ifdef DEBUG

void
pgWalTestToBuffer(PgWal pgWal, Buffer *walBuffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PG_WAL, pgWal);
        FUNCTION_TEST_PARAM(BUFFER, walBuffer);
    FUNCTION_TEST_END();

    ASSERT(walBuffer != NULL);

    // Set default WAL segment size if not specified
    if (pgWal.size == 0)
        pgWal.size = PG_WAL_SEGMENT_SIZE_DEFAULT;

    // Generate WAL
    pgInterfaceVersion(pgWal.version)->walTest(pgWal, bufPtr(walBuffer));

    FUNCTION_TEST_RETURN_VOID();
}

#endif

/**********************************************************************************************************************************/
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
        if (!regExpMatchOne(STRDEF("^[0-9]+[.]*[0-9]+$"), version))
            THROW_FMT(AssertError, "version %s format is invalid", strZ(version));

        // If there is a dot set the major and minor versions, else just the major
        int idxStart = strChr(version, '.');
        unsigned int major;
        unsigned int minor = 0;

        if (idxStart != -1)
        {
            major = cvtZToUInt(strZ(strSubN(version, 0, (size_t)idxStart)));
            minor = cvtZToUInt(strZ(strSub(version, (size_t)idxStart + 1)));
        }
        else
            major = cvtZToUInt(strZ(version));

        // No check to see if valid/supported PG version is on purpose
        result = major * 10000 + minor * 100;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(UINT, result);
}

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

/**********************************************************************************************************************************/
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
