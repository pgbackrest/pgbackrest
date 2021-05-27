/***********************************************************************************************************************************
Harness for PostgreSQL Interface
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/assert.h"

#include "common/harnessDebug.h"
#include "common/harnessPostgres.h"

/***********************************************************************************************************************************
Interface definition
***********************************************************************************************************************************/
uint32_t hrnPgInterfaceCatalogVersion083(void);
void hrnPgInterfaceControl083(PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal083(PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion084(void);
void hrnPgInterfaceControl084(PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal084(PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion090(void);
void hrnPgInterfaceControl090(PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal090(PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion091(void);
void hrnPgInterfaceControl091(PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal091(PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion092(void);
void hrnPgInterfaceControl092(PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal092(PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion093(void);
void hrnPgInterfaceControl093(PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal093(PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion094(void);
void hrnPgInterfaceControl094(PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal094(PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion095(void);
void hrnPgInterfaceControl095(PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal095(PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion096(void);
void hrnPgInterfaceControl096(PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal096(PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion100(void);
void hrnPgInterfaceControl100(PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal100(PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion110(void);
void hrnPgInterfaceControl110(PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal110(PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion120(void);
void hrnPgInterfaceControl120(PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal120(PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion130(void);
void hrnPgInterfaceControl130(PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal130(PgWal pgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion140(void);
void hrnPgInterfaceControl140(PgControl pgControl, unsigned char *buffer);
void hrnPgInterfaceWal140(PgWal pgWal, unsigned char *buffer);

typedef struct HrnPgInterface
{
    // Version of PostgreSQL supported by this interface
    unsigned int version;

    // Catalog version
    unsigned int (*catalogVersion)(void);

    // Create pg_control
    void (*control)(PgControl, unsigned char *);

    // Create WAL header
    void (*wal)(PgWal, unsigned char *);
} HrnPgInterface;

static const HrnPgInterface hrnPgInterface[] =
{
    {
        .version = PG_VERSION_14,

        .catalogVersion = hrnPgInterfaceCatalogVersion140,
        .control = hrnPgInterfaceControl140,
        .wal = hrnPgInterfaceWal140,
    },
    {
        .version = PG_VERSION_13,

        .catalogVersion = hrnPgInterfaceCatalogVersion130,
        .control = hrnPgInterfaceControl130,
        .wal = hrnPgInterfaceWal130,
    },
    {
        .version = PG_VERSION_12,

        .catalogVersion = hrnPgInterfaceCatalogVersion120,
        .control = hrnPgInterfaceControl120,
        .wal = hrnPgInterfaceWal120,
    },
    {
        .version = PG_VERSION_11,

        .catalogVersion = hrnPgInterfaceCatalogVersion110,
        .control = hrnPgInterfaceControl110,
        .wal = hrnPgInterfaceWal110,
    },
    {
        .version = PG_VERSION_10,

        .catalogVersion = hrnPgInterfaceCatalogVersion100,
        .control = hrnPgInterfaceControl100,
        .wal = hrnPgInterfaceWal100,
    },
    {
        .version = PG_VERSION_96,

        .catalogVersion = hrnPgInterfaceCatalogVersion096,
        .control = hrnPgInterfaceControl096,
        .wal = hrnPgInterfaceWal096,
    },
    {
        .version = PG_VERSION_95,

        .catalogVersion = hrnPgInterfaceCatalogVersion095,
        .control = hrnPgInterfaceControl095,
        .wal = hrnPgInterfaceWal095,
    },
    {
        .version = PG_VERSION_94,

        .catalogVersion = hrnPgInterfaceCatalogVersion094,
        .control = hrnPgInterfaceControl094,
        .wal = hrnPgInterfaceWal094,
    },
    {
        .version = PG_VERSION_93,

        .catalogVersion = hrnPgInterfaceCatalogVersion093,
        .control = hrnPgInterfaceControl093,
        .wal = hrnPgInterfaceWal093,
    },
    {
        .version = PG_VERSION_92,

        .catalogVersion = hrnPgInterfaceCatalogVersion092,
        .control = hrnPgInterfaceControl092,
        .wal = hrnPgInterfaceWal092,
    },
    {
        .version = PG_VERSION_91,
        .catalogVersion = hrnPgInterfaceCatalogVersion091,
        .control = hrnPgInterfaceControl091,
        .wal = hrnPgInterfaceWal091,
    },
    {
        .version = PG_VERSION_90,

        .catalogVersion = hrnPgInterfaceCatalogVersion090,
        .control = hrnPgInterfaceControl090,
        .wal = hrnPgInterfaceWal090,
    },
    {
        .version = PG_VERSION_84,

        .catalogVersion = hrnPgInterfaceCatalogVersion084,
        .control = hrnPgInterfaceControl084,
        .wal = hrnPgInterfaceWal084,
    },
    {
        .version = PG_VERSION_83,

        .catalogVersion = hrnPgInterfaceCatalogVersion083,
        .control = hrnPgInterfaceControl083,
        .wal = hrnPgInterfaceWal083,
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

    for (unsigned int interfaceIdx = 0; interfaceIdx < sizeof(hrnPgInterface) / sizeof(HrnPgInterface); interfaceIdx++)
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

    FUNCTION_HARNESS_RETURN(UINT, hrnPgInterfaceVersion(pgVersion)->catalogVersion());
}

/**********************************************************************************************************************************/
Buffer *
hrnPgControlToBuffer(PgControl pgControl)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(PG_CONTROL, pgControl);
    FUNCTION_HARNESS_END();

    // Set defaults if values are not passed
    pgControl.pageSize = pgControl.pageSize == 0 ? PG_PAGE_SIZE_DEFAULT : pgControl.pageSize;
    pgControl.walSegmentSize = pgControl.walSegmentSize == 0 ? PG_WAL_SEGMENT_SIZE_DEFAULT : pgControl.walSegmentSize;
    pgControl.catalogVersion = pgControl.catalogVersion == 0 ?
        hrnPgInterfaceVersion(pgControl.version)->catalogVersion() : pgControl.catalogVersion;

    // Create the buffer and clear it
    Buffer *result = bufNew(HRN_PG_CONTROL_SIZE);
    memset(bufPtr(result), 0, bufSize(result));
    bufUsedSet(result, bufSize(result));

    // Generate pg_control
    hrnPgInterfaceVersion(pgControl.version)->control(pgControl, bufPtr(result));

    FUNCTION_HARNESS_RETURN(BUFFER, result);
}

/**********************************************************************************************************************************/
void
hrnPgWalToBuffer(PgWal pgWal, Buffer *walBuffer)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(PG_WAL, pgWal);
        FUNCTION_HARNESS_PARAM(BUFFER, walBuffer);
    FUNCTION_TEST_END();

    ASSERT(walBuffer != NULL);

    // Set default WAL segment size if not specified
    if (pgWal.size == 0)
        pgWal.size = PG_WAL_SEGMENT_SIZE_DEFAULT;

    // Generate WAL
    hrnPgInterfaceVersion(pgWal.version)->wal(pgWal, bufPtr(walBuffer));

    FUNCTION_HARNESS_RETURN_VOID();
}
