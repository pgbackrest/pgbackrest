/***********************************************************************************************************************************
Harness for PostgreSQL Interface
***********************************************************************************************************************************/
#include "build.auto.h"
#include "config/config.h"

#include "common/assert.h"

#include "common/harnessDebug.h"
#include "common/harnessPostgres.h"

/***********************************************************************************************************************************
Interface definition
***********************************************************************************************************************************/
uint32_t hrnPgInterfaceCatalogVersion094(void);
void hrnPgInterfaceControl094(unsigned int controlVersion, unsigned int crc, PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal094(unsigned int magic, PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion94GPDB(void);
void hrnPgInterfaceControl94GPDB(unsigned int controlVersion, unsigned int crc, PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal94GPDB(unsigned int magic, PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion095(void);
void hrnPgInterfaceControl095(unsigned int controlVersion, unsigned int crc, PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal095(unsigned int magic, PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion096(void);
void hrnPgInterfaceControl096(unsigned int controlVersion, unsigned int crc, PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal096(unsigned int magic, PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion100(void);
void hrnPgInterfaceControl100(unsigned int controlVersion, unsigned int crc, PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal100(unsigned int magic, PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion110(void);
void hrnPgInterfaceControl110(unsigned int controlVersion, unsigned int crc, PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal110(unsigned int magic, PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion120(void);
void hrnPgInterfaceControl120(unsigned int controlVersion, unsigned int crc, PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal120(unsigned int magic, PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion120GPDB(void);
void hrnPgInterfaceControl120GPDB(unsigned int controlVersion, unsigned int crc, PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal120GPDB(unsigned int magic, PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion130(void);
void hrnPgInterfaceControl130(unsigned int controlVersion, unsigned int crc, PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal130(unsigned int magic, PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion140(void);
void hrnPgInterfaceControl140(unsigned int controlVersion, unsigned int crc, PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal140(unsigned int magic, PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion150(void);
void hrnPgInterfaceControl150(unsigned int controlVersion, unsigned int crc, PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal150(unsigned int magic, PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion160(void);
void hrnPgInterfaceControl160(unsigned int controlVersion, unsigned int crc, PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal160(unsigned int magic, PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion170(void);
void hrnPgInterfaceControl170(unsigned int controlVersion, unsigned int crc, PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal170(unsigned int magic, PgWal pgWal, unsigned char *buffer);

typedef struct HrnPgInterface
{
    // Version of PostgreSQL supported by this interface
    unsigned int version;

    StringId fork;

    // Catalog version
    unsigned int (*catalogVersion)(void);

    // Create pg_control
    void (*control)(unsigned int, unsigned int, PgControl, unsigned char *);

    // Create WAL header
    void (*wal)(unsigned int, PgWal, unsigned char *);
} HrnPgInterface;

static const HrnPgInterface hrnPgInterface[] =
{
    {
        .version = PG_VERSION_17,
        .fork = CFGOPTVAL_FORK_POSTGRESQL,

        .catalogVersion = hrnPgInterfaceCatalogVersion170,
        .control = hrnPgInterfaceControl170,
        .wal = hrnPgInterfaceWal170,
    },
    {
        .version = PG_VERSION_16,
        .fork = CFGOPTVAL_FORK_POSTGRESQL,

        .catalogVersion = hrnPgInterfaceCatalogVersion160,
        .control = hrnPgInterfaceControl160,
        .wal = hrnPgInterfaceWal160,
    },
    {
        .version = PG_VERSION_15,
        .fork = CFGOPTVAL_FORK_POSTGRESQL,

        .catalogVersion = hrnPgInterfaceCatalogVersion150,
        .control = hrnPgInterfaceControl150,
        .wal = hrnPgInterfaceWal150,
    },
    {
        .version = PG_VERSION_14,
        .fork = CFGOPTVAL_FORK_POSTGRESQL,

        .catalogVersion = hrnPgInterfaceCatalogVersion140,
        .control = hrnPgInterfaceControl140,
        .wal = hrnPgInterfaceWal140,
    },
    {
        .version = PG_VERSION_13,
        .fork = CFGOPTVAL_FORK_POSTGRESQL,

        .catalogVersion = hrnPgInterfaceCatalogVersion130,
        .control = hrnPgInterfaceControl130,
        .wal = hrnPgInterfaceWal130,
    },
    {
        .version = PG_VERSION_12,
        .fork = CFGOPTVAL_FORK_POSTGRESQL,

        .catalogVersion = hrnPgInterfaceCatalogVersion120,
        .control = hrnPgInterfaceControl120,
        .wal = hrnPgInterfaceWal120,
    },
    {
        .version = PG_VERSION_12,
        .fork = CFGOPTVAL_FORK_GPDB,

        .catalogVersion = hrnPgInterfaceCatalogVersion120GPDB,
        .control = hrnPgInterfaceControl120GPDB,
        .wal = hrnPgInterfaceWal120GPDB,
    },
    {
        .version = PG_VERSION_11,
        .fork = CFGOPTVAL_FORK_POSTGRESQL,

        .catalogVersion = hrnPgInterfaceCatalogVersion110,
        .control = hrnPgInterfaceControl110,
        .wal = hrnPgInterfaceWal110,
    },
    {
        .version = PG_VERSION_10,
        .fork = CFGOPTVAL_FORK_POSTGRESQL,

        .catalogVersion = hrnPgInterfaceCatalogVersion100,
        .control = hrnPgInterfaceControl100,
        .wal = hrnPgInterfaceWal100,
    },
    {
        .version = PG_VERSION_96,
        .fork = CFGOPTVAL_FORK_POSTGRESQL,

        .catalogVersion = hrnPgInterfaceCatalogVersion096,
        .control = hrnPgInterfaceControl096,
        .wal = hrnPgInterfaceWal096,
    },
    {
        .version = PG_VERSION_95,
        .fork = CFGOPTVAL_FORK_POSTGRESQL,

        .catalogVersion = hrnPgInterfaceCatalogVersion095,
        .control = hrnPgInterfaceControl095,
        .wal = hrnPgInterfaceWal095,
    },
    {
        .version = PG_VERSION_94,
        .fork = CFGOPTVAL_FORK_POSTGRESQL,

        .catalogVersion = hrnPgInterfaceCatalogVersion094,
        .control = hrnPgInterfaceControl094,
        .wal = hrnPgInterfaceWal094,
    },
    {
        .version = PG_VERSION_94,
        .fork = CFGOPTVAL_FORK_GPDB,

        .catalogVersion = hrnPgInterfaceCatalogVersion94GPDB,
        .control = hrnPgInterfaceControl94GPDB,
        .wal = hrnPgInterfaceWal94GPDB,
    },
};

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
    const StringId fork = cfgOptionStrId(cfgOptFork);

    for (unsigned int interfaceIdx = 0; interfaceIdx < LENGTH_OF(hrnPgInterface); interfaceIdx++)
    {
        if (hrnPgInterface[interfaceIdx].version == pgVersion && hrnPgInterface[interfaceIdx].fork == fork)
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

    FUNCTION_HARNESS_RETURN(UINT, hrnPgInterfaceVersion(pgVersion)->catalogVersion());
}

/**********************************************************************************************************************************/
Buffer *
hrnPgControlToBuffer(const unsigned int controlVersion, const unsigned int crc, PgControl pgControl)
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
            0 : (pgControl.walSegmentSize == 0 ? PG_WAL_SEGMENT_SIZE_DEFAULT : pgControl.walSegmentSize);
    pgControl.catalogVersion =
        pgControl.catalogVersion == 0 ? hrnPgInterfaceVersion(pgControl.version)->catalogVersion() : pgControl.catalogVersion;
    pgControl.systemId = pgControl.systemId < 100 ? hrnPgSystemId(pgControl.version) + pgControl.systemId : pgControl.systemId;
    pgControl.checkpoint = pgControl.checkpoint == 0 ? 1 : pgControl.checkpoint;
    pgControl.timeline = pgControl.timeline == 0 ? 1 : pgControl.timeline;

    // Create the buffer and clear it
    Buffer *result = bufNew(HRN_PG_CONTROL_SIZE);
    memset(bufPtr(result), 0, bufSize(result));
    bufUsedSet(result, bufSize(result));

    // Generate pg_control
    hrnPgInterfaceVersion(pgControl.version)->control(controlVersion, crc, pgControl, bufPtr(result));

    FUNCTION_HARNESS_RETURN(BUFFER, result);
}

/**********************************************************************************************************************************/
void
hrnPgWalToBuffer(Buffer *const walBuffer, const unsigned int magic, PgWal pgWal)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(BUFFER, walBuffer);
        FUNCTION_HARNESS_PARAM(UINT, magic);
        FUNCTION_HARNESS_PARAM(PG_WAL, pgWal);
    FUNCTION_HARNESS_END();

    ASSERT(walBuffer != NULL);

    // Set default WAL segment size if not specified
    if (pgWal.size == 0)
        pgWal.size = PG_WAL_SEGMENT_SIZE_DEFAULT;

    // Set default system id if not specified
    if (pgWal.systemId < 100)
        pgWal.systemId = hrnPgSystemId(pgWal.version) + pgWal.systemId;

    // Generate WAL
    hrnPgInterfaceVersion(pgWal.version)->wal(magic, pgWal, bufPtr(walBuffer));

    FUNCTION_HARNESS_RETURN_VOID();
}
