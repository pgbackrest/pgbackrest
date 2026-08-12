/***********************************************************************************************************************************
Harness for PostgreSQL Interface
***********************************************************************************************************************************/
#include <build.h>

#include "common/assert.h"
#include "postgres/interface/crc32.h"

#include "harness/debug.h"
#include "harness/postgres.h"

/***********************************************************************************************************************************
Include shimmed C modules

The interface the code reads is shimmed so the harness can write with the types it declares rather than declaring them again, and so
the values a test gets by default are the ones the code matches on rather than a second copy of them.
***********************************************************************************************************************************/
{[SHIM_MODULE]}

/***********************************************************************************************************************************
Interface definition
***********************************************************************************************************************************/
typedef struct HrnPgInterface
{
    // Version of PostgreSQL supported by this interface
    unsigned int version;

    // Create pg_control
    void (*control)(unsigned int, unsigned int, PgControl, uint8_t *);

    // Create WAL header
    void (*wal)(unsigned int, PgWal, uint8_t *);
} HrnPgInterface;

// Include auto-generated interfaces
#include "harness/postgres/interface.auto.c.inc"

/***********************************************************************************************************************************
Get the interface for a PostgreSQL version
***********************************************************************************************************************************/
static const HrnPgInterface *
hrnPgInterfaceVersion(unsigned int pgVersion)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(UINT, pgVersion);
    FUNCTION_HARNESS_END();

    const HrnPgInterface *result = NULL;

    for (unsigned int interfaceIdx = 0; interfaceIdx < LENGTH_OF(hrnPgInterface); interfaceIdx++)
    {
        if (hrnPgInterface[interfaceIdx].version == pgVersion)
        {
            result = &hrnPgInterface[interfaceIdx];
            break;
        }
    }

    // If the version was not found then error
    if (result == NULL)
        THROW_FMT(AssertError, "invalid " PG_NAME " version %u", pgVersion);

    FUNCTION_HARNESS_RETURN(STRUCT, result);
}

/**********************************************************************************************************************************/
unsigned int
hrnPgCatalogVersion(unsigned int pgVersion)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(UINT, pgVersion);
    FUNCTION_HARNESS_END();

    FUNCTION_HARNESS_RETURN(UINT, pgInterfaceVersion(pgVersion)->catalogVersion);
}

/**********************************************************************************************************************************/
Buffer *
hrnPgControlToBuffer(unsigned int controlVersion, const unsigned int crc, PgControl pgControl)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(UINT, controlVersion);
        FUNCTION_HARNESS_PARAM(UINT, crc);
        FUNCTION_HARNESS_PARAM(PG_CONTROL, pgControl);
    FUNCTION_HARNESS_END();

    ASSERT(pgControl.version != 0);

    // Set defaults if values are not passed
    pgControl.pageSize = pgControl.pageSize == 0 ? pgPageSize8 : pgControl.pageSize;
    pgControl.walSegmentSize =
        pgControl.walSegmentSize == UINT_MAX ?
            0 : (pgControl.walSegmentSize == 0 ? HRN_PG_WAL_SEGMENT_SIZE_DEFAULT : pgControl.walSegmentSize);
    pgControl.catalogVersion =
        pgControl.catalogVersion == 0 ? pgInterfaceVersion(pgControl.version)->catalogVersion : pgControl.catalogVersion;
    pgControl.systemId = pgControl.systemId < 100 ? hrnPgSystemId(pgControl.version) + pgControl.systemId : pgControl.systemId;
    pgControl.checkpoint = pgControl.checkpoint == 0 ? 1 : pgControl.checkpoint;
    pgControl.timeline = pgControl.timeline == 0 ? 1 : pgControl.timeline;

    // Create the buffer and clear it
    Buffer *result = bufNew(HRN_PG_CONTROL_SIZE);
    memset(bufPtr(result), 0, bufSize(result));
    bufUsedSet(result, bufSize(result));

    // Generate pg_control
    if (controlVersion == 0)
        controlVersion = pgInterfaceVersion(pgControl.version)->controlVersion;

    hrnPgInterfaceVersion(pgControl.version)->control(controlVersion, crc, pgControl, bufPtr(result));

    FUNCTION_HARNESS_RETURN(BUFFER, result);
}

/**********************************************************************************************************************************/
void
hrnPgWalToBuffer(Buffer *const walBuffer, unsigned int magic, PgWal pgWal)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(BUFFER, walBuffer);
        FUNCTION_HARNESS_PARAM(UINT, magic);
        FUNCTION_HARNESS_PARAM(PG_WAL, pgWal);
    FUNCTION_HARNESS_END();

    ASSERT(walBuffer != NULL);

    // Set default WAL segment size if not specified
    if (pgWal.size == 0)
        pgWal.size = HRN_PG_WAL_SEGMENT_SIZE_DEFAULT;

    // Set default system id if not specified
    if (pgWal.systemId < 100)
        pgWal.systemId = hrnPgSystemId(pgWal.version) + pgWal.systemId;

    // Generate WAL
    if (magic == 0)
        magic = pgInterfaceVersion(pgWal.version)->walMagic;

    hrnPgInterfaceVersion(pgWal.version)->wal(magic, pgWal, bufPtr(walBuffer));

    FUNCTION_HARNESS_RETURN_VOID();
}
