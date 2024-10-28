/***********************************************************************************************************************************
PostgreSQL Interface
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "common/time.h"
#include "postgres/interface.h"
#include "postgres/interface/crc32.h"
#include "postgres/interface/static.vendor.h"
#include "postgres/version.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Defines for various Postgres paths and files
***********************************************************************************************************************************/
STRING_EXTERN(PG_FILE_PGVERSION_STR,                                PG_FILE_PGVERSION);
STRING_EXTERN(PG_FILE_POSTGRESQLAUTOCONF_STR,                       PG_FILE_POSTGRESQLAUTOCONF);
STRING_EXTERN(PG_FILE_POSTMTRPID_STR,                               PG_FILE_POSTMTRPID);
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

// Lsn name used in functions depending on version
STRING_STATIC(PG_NAME_LSN_STR,                                      "lsn");
STRING_STATIC(PG_NAME_LOCATION_STR,                                 "location");

/***********************************************************************************************************************************
The control file is 8192 bytes but only the first 512 bytes are used to prevent torn pages even on really old storage with 512-byte
sectors. This is true across all versions of PostgreSQL. Unfortunately, this is not sufficient to prevent torn pages during
concurrent reads and writes so retries are required.
***********************************************************************************************************************************/
#define PG_CONTROL_SIZE                                             8192
#define PG_CONTROL_DATA_SIZE                                        512

/***********************************************************************************************************************************
PostgreSQL interface definitions

The interface functions are documented here rather than the auto-generated file.
***********************************************************************************************************************************/
typedef struct PgInterface
{
    // Version of PostgreSQL supported by this interface
    unsigned int version;

    // Does pg_control match this version of PostgreSQL?
    bool (*controlIs)(const unsigned char *);

    // Convert pg_control to a common data structure
    PgControl (*control)(const unsigned char *);

    // Get control crc offset
    size_t (*controlCrcOffset)(void);

    // Invalidate control checkpoint
    void (*controlCheckpointInvalidate)(unsigned char *);

    // Get the control version for this version of PostgreSQL
    uint32_t (*controlVersion)(void);

    // Does the WAL header match this version of PostgreSQL?
    bool (*walIs)(const unsigned char *);

    // Convert WAL header to a common data structure
    PgWal (*wal)(const unsigned char *);
} PgInterface;

// Include auto-generated interfaces
#include "postgres/interface.auto.c.inc"

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
pgInterfaceVersion(const unsigned int pgVersion)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
    FUNCTION_TEST_END();

    const PgInterface *result = NULL;

    for (unsigned int interfaceIdx = 0; interfaceIdx < LENGTH_OF(pgInterface); interfaceIdx++)
    {
        if (pgInterface[interfaceIdx].version == pgVersion)
        {
            result = &pgInterface[interfaceIdx];
            break;
        }
    }

    // If the version was not found then error
    if (result == NULL)
    {
        THROW_FMT(
            VersionNotSupportedError,
            "invalid " PG_NAME " version %u\n"
            "HINT: is this version of PostgreSQL supported?",
            pgVersion);
    }

    FUNCTION_TEST_RETURN_TYPE_CONST_P(PgInterface, result);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
pgDbIsTemplate(const String *const name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(BOOL, strEqZ(name, "template0") || strEqZ(name, "template1"));
}

/**********************************************************************************************************************************/
FN_EXTERN bool
pgDbIsSystem(const String *const name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(BOOL, strEqZ(name, "postgres") || pgDbIsTemplate(name));
}

/**********************************************************************************************************************************/
FN_EXTERN bool
pgDbIsSystemId(const unsigned int id)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, id);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(BOOL, id < FirstNormalObjectId);
}

/***********************************************************************************************************************************
Check expected WAL segment size for older PostgreSQL versions
***********************************************************************************************************************************/
static void
pgWalSegmentSizeCheck(const unsigned int walSegmentSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, walSegmentSize);
    FUNCTION_TEST_END();

    // Check that the WAL segment size is valid
    if (!IsValidWalSegSize(walSegmentSize))
    {
        THROW_FMT(
            FormatError, "wal segment size is %u but must be a power of two between %d and %d inclusive", walSegmentSize,
            WalSegMinSize, WalSegMaxSize);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Validate the CRC in pg_control. If the version has been forced this may required scanning pg_control for the offset. Return the
offset found so it can be used to update the CRC.
***********************************************************************************************************************************/
static size_t
pgControlCrcValidate(const Buffer *const controlFile, const PgInterface *const interface, const bool pgVersionForce)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BUFFER, controlFile);
        FUNCTION_LOG_PARAM_P(VOID, interface);
        FUNCTION_LOG_PARAM(BOOL, pgVersionForce);
    FUNCTION_LOG_END();

    ASSERT(controlFile != NULL);
    ASSERT(interface != NULL);

    size_t result = interface->controlCrcOffset();

    do
    {
        // Calculate CRC and retrieve expected CRC
        const uint32_t crcCalculated = crc32cOne(bufPtrConst(controlFile), result);
        const uint32_t crcExpected = *((const uint32_t *)(bufPtrConst(controlFile) + result));

        // If CRC does not match
        if (crcCalculated != crcExpected)
        {
            // If version is forced then the CRC might be later in the file (assuming the fork added extra fields to pg_control).
            // Increment the offset by CRC data size and continue to try again.
            if (pgVersionForce)
            {
                result += sizeof(uint32_t);

                if (result <= bufUsed(controlFile) - sizeof(uint32_t))
                    continue;
            }

            // If no retry then error
            THROW_FMT(
                ChecksumError,
                "calculated " PG_FILE_PGCONTROL " checksum does not match expected value\n"
                "HINT: calculated 0x%x but expected value is 0x%x\n"
                "%s"
                "HINT: is " PG_FILE_PGCONTROL " corrupt?\n"
                "HINT: does " PG_FILE_PGCONTROL " have a different layout than expected?",
                crcCalculated, crcExpected,
                pgVersionForce ? "HINT: checksum values may be misleading due to forced version scan\n" : "");
        }

        // Do not retry if the CRC is valid
        break;
    }
    while (true);

    FUNCTION_LOG_RETURN(SIZE, result);
}

/***********************************************************************************************************************************
Update the CRC in pg_control
***********************************************************************************************************************************/
static void
pgControlCrcUpdate(Buffer *const controlFile, const unsigned int pgVersion, const size_t crcOffset)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BUFFER, controlFile);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(SIZE, crcOffset);
    FUNCTION_LOG_END();

    ASSERT(controlFile != NULL);
    ASSERT(pgVersion != 0);
    ASSERT(crcOffset != 0);

    *((uint32_t *)(bufPtr(controlFile) + crcOffset)) = crc32cOne(bufPtrConst(controlFile), crcOffset);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static PgControl
pgControlFromBuffer(const Buffer *const controlFile, const String *const pgVersionForce)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BUFFER, controlFile);
        FUNCTION_LOG_PARAM(STRING, pgVersionForce);
    FUNCTION_LOG_END();

    ASSERT(controlFile != NULL);

    // Search for the version of PostgreSQL that uses this control file
    const PgInterface *interface = NULL;

    if (pgVersionForce != NULL)
        interface = pgInterfaceVersion(pgVersionFromStr(pgVersionForce));
    else
    {
        for (unsigned int interfaceIdx = 0; interfaceIdx < LENGTH_OF(pgInterface); interfaceIdx++)
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
    }

    // Get info from the control file
    ASSERT(pgVersionForce != NULL || interface->controlIs(bufPtrConst(controlFile)));

    PgControl result = interface->control(bufPtrConst(controlFile));
    result.version = interface->version;

    // Validate the CRC
    pgControlCrcValidate(controlFile, interface, pgVersionForce != NULL);

    // Check the segment size
    pgWalSegmentSizeCheck(result.walSegmentSize);

    // Check the page size
    pgPageSizeCheck(result.pageSize);

    // Check the checksum version
    if (result.pageChecksumVersion != 0 && result.pageChecksumVersion != PG_DATA_CHECKSUM_VERSION)
    {
        THROW_FMT(
            FormatError, "page checksum version is %u but only 0 and " STRINGIFY(PG_DATA_CHECKSUM_VERSION) " are valid",
            result.pageChecksumVersion);
    }

    FUNCTION_LOG_RETURN(PG_CONTROL, result);
}

// Helper to compare control data to last read
static bool
pgControlBufferEq(const Buffer *const last, const Buffer *const current)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, last);
        FUNCTION_TEST_PARAM(BUFFER, current);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(BOOL, last != NULL && bufEq(last, current));
}

FN_EXTERN Buffer *
pgControlBufferFromFile(const Storage *const storage, const String *const pgVersionForce)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, pgVersionForce);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);

    Buffer *result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // On filesystems that do not implement atomicity of concurrent reads and writes, we might get garbage if the server is
        // writing the pg_control file at the same time as we try to read it. Keep trying until success or we read the same data
        // twice in a row. Do not use a timeout here because there are plenty of other errors that can happen and we don't want to
        // wait for them.
        Buffer *controlFileLast = NULL;
        bool done = false;

        do
        {
            // Read control file
            Buffer *const controlFile = storageGetP(
                storageNewReadP(storage, STRDEF(PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL)), .exactSize = PG_CONTROL_DATA_SIZE);

            TRY_BEGIN()
            {
                // Check that control data is valid
                pgControlFromBuffer(controlFile, pgVersionForce);

                // Create a buffer of the correct size to hold pg_control and zero out the remaining portion
                result = bufNew(PG_CONTROL_SIZE);

                bufCat(result, controlFile);
                memset(bufPtr(result) + bufUsed(controlFile), 0, bufSize(result) - bufUsed(controlFile));
                bufUsedSet(result, bufSize(result));
                bufMove(result, memContextPrior());

                done = true;
            }
            CATCH_ANY()
            {
                // If we get the same bad data twice in a row then error
                if (pgControlBufferEq(controlFileLast, controlFile))
                    RETHROW();

                // Copy data to last
                bufFree(controlFileLast);
                controlFileLast = controlFile;

                // Sleep to let data stabilize
                sleepMSec(50);
            }
            TRY_END();
        }
        while (!done);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BUFFER, result);
}

FN_EXTERN PgControl
pgControlFromFile(const Storage *const storage, const String *const pgVersionForce)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, pgVersionForce);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);

    Buffer *const buffer = pgControlBufferFromFile(storage, pgVersionForce);
    const PgControl result = pgControlFromBuffer(buffer, pgVersionForce);

    bufFree(buffer);

    FUNCTION_LOG_RETURN(PG_CONTROL, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
pgControlCheckpointInvalidate(Buffer *const buffer, const String *const pgVersionForce)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(STRING, pgVersionForce);
    FUNCTION_LOG_END();

    ASSERT(buffer != NULL);

    const PgControl pgControl = pgControlFromBuffer(buffer, pgVersionForce);
    const PgInterface *const interface = pgInterfaceVersion(pgControl.version);
    const size_t crcOffset = pgControlCrcValidate(buffer, interface, pgVersionForce);

    pgInterfaceVersion(pgControl.version)->controlCheckpointInvalidate(bufPtr(buffer));
    pgControlCrcUpdate(buffer, interface->version, crcOffset);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN uint32_t
pgControlVersion(const unsigned int pgVersion)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(UINT32, pgInterfaceVersion(pgVersion)->controlVersion());
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
FN_EXTERN PgWal
pgWalFromBuffer(const Buffer *const walBuffer, const String *const pgVersionForce)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BUFFER, walBuffer);
        FUNCTION_LOG_PARAM(STRING, pgVersionForce);
    FUNCTION_LOG_END();

    ASSERT(walBuffer != NULL);

    // Check that this is a long format WAL header
    if (!(((const PgWalCommon *)bufPtrConst(walBuffer))->flag & PG_WAL_LONG_HEADER))
        THROW_FMT(FormatError, "first page header in WAL file is expected to be in long format");

    // Search for the version of PostgreSQL that uses this WAL magic
    const PgInterface *interface = NULL;

    if (pgVersionForce != NULL)
        interface = pgInterfaceVersion(pgVersionFromStr(pgVersionForce));
    else
    {
        for (unsigned int interfaceIdx = 0; interfaceIdx < LENGTH_OF(pgInterface); interfaceIdx++)
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
    }

    // Get info from the control file
    ASSERT(pgVersionForce != NULL || interface->walIs(bufPtrConst(walBuffer)));

    PgWal result = interface->wal(bufPtrConst(walBuffer));
    result.version = interface->version;

    // Check the segment size
    pgWalSegmentSizeCheck(result.size);

    FUNCTION_LOG_RETURN(PG_WAL, result);
}

FN_EXTERN PgWal
pgWalFromFile(const String *const walFile, const Storage *const storage, const String *const pgVersionForce)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, walFile);
        FUNCTION_LOG_PARAM(STRING, pgVersionForce);
    FUNCTION_LOG_END();

    ASSERT(walFile != NULL);

    PgWal result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Read WAL segment header
        const Buffer *const walBuffer = storageGetP(storageNewReadP(storage, walFile), .exactSize = PG_WAL_HEADER_SIZE);

        result = pgWalFromBuffer(walBuffer, pgVersionForce);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PG_WAL, result);
}

/**********************************************************************************************************************************/
FN_EXTERN String *
pgTablespaceId(const unsigned int pgVersion, const unsigned int pgCatalogVersion)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
        FUNCTION_TEST_PARAM(UINT, pgCatalogVersion);
    FUNCTION_TEST_END();

    String *result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const pgVersionStr = pgVersionToStr(pgVersion);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = strNewFmt("PG_%s_%u", strZ(pgVersionStr), pgCatalogVersion);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN uint64_t
pgLsnFromStr(const String *lsn)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, lsn);
    FUNCTION_TEST_END();

    uint64_t result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const StringList *const lsnPart = strLstNewSplit(lsn, FSLASH_STR);

        CHECK(FormatError, strLstSize(lsnPart) == 2, "lsn requires two parts");

        result = (cvtZToUInt64Base(strZ(strLstGet(lsnPart, 0)), 16) << 32) + cvtZToUInt64Base(strZ(strLstGet(lsnPart, 1)), 16);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(UINT64, result);
}

FN_EXTERN String *
pgLsnToStr(const uint64_t lsn)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT64, lsn);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(STRING, strNewFmt("%x/%x", (unsigned int)(lsn >> 32), (unsigned int)(lsn & 0xFFFFFFFF)));
}

/**********************************************************************************************************************************/
FN_EXTERN String *
pgLsnToWalSegment(const uint32_t timeline, const uint64_t lsn, const unsigned int walSegmentSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, timeline);
        FUNCTION_TEST_PARAM(UINT64, lsn);
        FUNCTION_TEST_PARAM(UINT, walSegmentSize);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(
        STRING, strNewFmt("%08X%08X%08X", timeline, (unsigned int)(lsn >> 32), (unsigned int)(lsn & 0xFFFFFFFF) / walSegmentSize));
}

/**********************************************************************************************************************************/
FN_EXTERN uint32_t
pgTimelineFromWalSegment(const String *const walSegment)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, walSegment);
    FUNCTION_TEST_END();

    ASSERT(walSegment != NULL);
    ASSERT(strSize(walSegment) == 24);

    FUNCTION_TEST_RETURN(UINT32, cvtZSubNToUIntBase(strZ(walSegment), 0, 8, 16));
}

/**********************************************************************************************************************************/
FN_EXTERN StringList *
pgLsnRangeToWalSegmentList(
    const uint32_t timeline, const uint64_t lsnStart, const uint64_t lsnStop, const unsigned int walSegmentSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, timeline);
        FUNCTION_TEST_PARAM(UINT64, lsnStart);
        FUNCTION_TEST_PARAM(UINT64, lsnStop);
        FUNCTION_TEST_PARAM(UINT, walSegmentSize);
    FUNCTION_TEST_END();

    ASSERT(timeline != 0);
    ASSERT(lsnStart <= lsnStop);
    ASSERT(walSegmentSize != 0);

    StringList *const result = strLstNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Calculate the start and stop segments
        unsigned int startMajor = (unsigned int)(lsnStart >> 32);
        unsigned int startMinor = (unsigned int)(lsnStart & 0xFFFFFFFF) / walSegmentSize;

        const unsigned int stopMajor = (unsigned int)(lsnStop >> 32);
        const unsigned int stopMinor = (unsigned int)(lsnStop & 0xFFFFFFFF) / walSegmentSize;

        const unsigned int minorPerMajor = 0xFFFFFFFF / walSegmentSize;

        // Create list
        strLstAddFmt(result, "%08X%08X%08X", timeline, startMajor, startMinor);

        while (!(startMajor == stopMajor && startMinor == stopMinor))
        {
            startMinor++;

            if (startMinor > minorPerMajor)
            {
                startMajor++;
                startMinor = 0;
            }

            strLstAddFmt(result, "%08X%08X%08X", timeline, startMajor, startMinor);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(STRING_LIST, result);
}

/**********************************************************************************************************************************/
FN_EXTERN const String *
pgLsnName(const unsigned int pgVersion)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN_CONST(STRING, pgVersion >= PG_VERSION_WAL_RENAME ? PG_NAME_LSN_STR : PG_NAME_LOCATION_STR);
}

/***********************************************************************************************************************************
Get WAL name (wal/xlog) for a PostgreSQL version
***********************************************************************************************************************************/
FN_EXTERN const String *
pgWalName(const unsigned int pgVersion)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN_CONST(STRING, pgVersion >= PG_VERSION_WAL_RENAME ? PG_NAME_WAL_STR : PG_NAME_XLOG_STR);
}

/**********************************************************************************************************************************/
FN_EXTERN const String *
pgWalPath(const unsigned int pgVersion)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN_CONST(STRING, pgVersion >= PG_VERSION_WAL_RENAME ? PG_PATH_PGWAL_STR : PG_PATH_PGXLOG_STR);
}

/**********************************************************************************************************************************/
FN_EXTERN const String *
pgXactPath(const unsigned int pgVersion)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN_CONST(STRING, pgVersion >= PG_VERSION_WAL_RENAME ? PG_PATH_PGXACT_STR : PG_PATH_PGCLOG_STR);
}

/**********************************************************************************************************************************/
FN_EXTERN unsigned int
pgVersionFromStr(const String *const version)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, version);
    FUNCTION_TEST_END();

    ASSERT(version != NULL);

    // If format not number.number (9.6) or number only (10) then error. No check for valid/supported PG version is on purpose.
    if (!regExpMatchOne(STRDEF("^[0-9]+[.]*[0-9]+$"), version))
        THROW_FMT(AssertError, "version %s format is invalid", strZ(version));

    // If there is no dot then only the major version is needed
    const int idxStart = strChr(version, '.');

    if (idxStart == -1)
        FUNCTION_TEST_RETURN(UINT, cvtZToUInt(strZ(version)) * 10000);

    // Major and minor version are needed
    FUNCTION_TEST_RETURN(
        UINT, cvtZSubNToUInt(strZ(version), 0, (size_t)idxStart) * 10000 + cvtZToUInt(strZ(version) + (size_t)idxStart + 1) * 100);
}

FN_EXTERN String *
pgVersionToStr(const unsigned int version)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, version);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(
        STRING,
        version >= PG_VERSION_10 ?
            strNewFmt("%u", version / 10000) : strNewFmt("%u.%u", version / 10000, version % 10000 / 100));
}

/**********************************************************************************************************************************/
FN_EXTERN void
pgControlToLog(const PgControl *const pgControl, StringStatic *const debugLog)
{
    strStcFmt(
        debugLog, "{version: %u, systemId: %" PRIu64 ", walSegmentSize: %u, pageChecksumVersion: %u}", pgControl->version,
        pgControl->systemId, pgControl->walSegmentSize, pgControl->pageChecksumVersion);
}

FN_EXTERN void
pgWalToLog(const PgWal *const pgWal, StringStatic *const debugLog)
{
    strStcFmt(debugLog, "{version: %u, systemId: %" PRIu64 "}", pgWal->version, pgWal->systemId);
}
