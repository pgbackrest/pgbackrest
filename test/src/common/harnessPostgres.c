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
uint32_t hrnPgInterfaceCatalogVersion093(void);
void hrnPgInterfaceControl093(HrnPgControl hrnPgControl, unsigned char *buffer);
void hrnPgInterfaceWal093(HrnPgWal hrnPgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion094(void);
void hrnPgInterfaceControl094(HrnPgControl hrnPgControl, unsigned char *buffer);
void hrnPgInterfaceWal094(HrnPgWal hrnPgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion095(void);
void hrnPgInterfaceControl095(HrnPgControl hrnPgControl, unsigned char *buffer);
void hrnPgInterfaceWal095(HrnPgWal hrnPgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion096(void);
void hrnPgInterfaceControl096(HrnPgControl hrnPgControl, unsigned char *buffer);
void hrnPgInterfaceWal096(HrnPgWal hrnPgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion100(void);
void hrnPgInterfaceControl100(HrnPgControl hrnPgControl, unsigned char *buffer);
void hrnPgInterfaceWal100(HrnPgWal hrnPgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion110(void);
void hrnPgInterfaceControl110(HrnPgControl hrnPgControl, unsigned char *buffer);
void hrnPgInterfaceWal110(HrnPgWal hrnPgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion120(void);
void hrnPgInterfaceControl120(HrnPgControl hrnPgControl, unsigned char *buffer);
void hrnPgInterfaceWal120(HrnPgWal hrnPgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion130(void);
void hrnPgInterfaceControl130(HrnPgControl hrnPgControl, unsigned char *buffer);
void hrnPgInterfaceWal130(HrnPgWal hrnPgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion140(void);
void hrnPgInterfaceControl140(HrnPgControl hrnPgControl, unsigned char *buffer);
void hrnPgInterfaceWal140(HrnPgWal hrnPgWal, unsigned char *buffer);

uint32_t hrnPgInterfaceCatalogVersion150(void);
void hrnPgInterfaceControl150(HrnPgControl hrnPgControl, unsigned char *buffer);
void hrnPgInterfaceWal150(HrnPgWal hrnPgWal, unsigned char *buffer);

typedef struct HrnPgInterface
{
    // Version of PostgreSQL supported by this interface
    unsigned int version;

    // Catalog version
    unsigned int (*catalogVersion)(void);

    // Create pg_control
    void (*control)(HrnPgControl, unsigned char *);

    // Create WAL header
    void (*wal)(HrnPgWal, unsigned char *);
} HrnPgInterface;

static const HrnPgInterface hrnPgInterface[] =
{
    {
        .version = PG_VERSION_15,

        .catalogVersion = hrnPgInterfaceCatalogVersion150,
        .control = hrnPgInterfaceControl150,
        .wal = hrnPgInterfaceWal150,
    },
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

    FUNCTION_HARNESS_RETURN(UINT, hrnPgInterfaceVersion(pgVersion)->catalogVersion());
}

/**********************************************************************************************************************************/
Buffer *
hrnPgControlToBuffer(HrnPgControl hrnPgControl)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(VOID, hrnPgControl);
    FUNCTION_HARNESS_END();

    ASSERT(hrnPgControl.version != 0);

    // Set defaults if values are not passed
    hrnPgControl.pageSize = hrnPgControl.pageSize == 0 ? PG_PAGE_SIZE_DEFAULT : hrnPgControl.pageSize;
    hrnPgControl.walSegmentSize = hrnPgControl.walSegmentSize == 0 ? PG_WAL_SEGMENT_SIZE_DEFAULT : hrnPgControl.walSegmentSize;
    hrnPgControl.controlVersion =
        hrnPgControl.controlVersion == 0 ? pgControlVersion(hrnPgControl.version) : hrnPgControl.controlVersion;
    hrnPgControl.catalogVersion =
        hrnPgControl.catalogVersion == 0 ?
            hrnPgInterfaceVersion(hrnPgControl.version)->catalogVersion() : hrnPgControl.catalogVersion;
    hrnPgControl.systemId =
        hrnPgControl.systemId < 100 ? hrnPgSystemId(hrnPgControl.version) + hrnPgControl.systemId : hrnPgControl.systemId;
    hrnPgControl.checkpoint = hrnPgControl.checkpoint == 0 ? 1 : hrnPgControl.checkpoint;
    hrnPgControl.timeline = hrnPgControl.timeline == 0 ? 1 : hrnPgControl.timeline;

    // Create the buffer and clear it
    Buffer *result = bufNew(HRN_PG_CONTROL_SIZE);
    memset(bufPtr(result), 0, bufSize(result));
    bufUsedSet(result, bufSize(result));

    // Generate pg_control
    hrnPgInterfaceVersion(hrnPgControl.version)->control(hrnPgControl, bufPtr(result));

    FUNCTION_HARNESS_RETURN(BUFFER, result);
}

/**********************************************************************************************************************************/
void
hrnPgWalToBuffer(HrnPgWal hrnPgWal, Buffer *const walBuffer)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(VOID, hrnPgWal);
        FUNCTION_HARNESS_PARAM(BUFFER, walBuffer);
    FUNCTION_HARNESS_END();

    ASSERT(walBuffer != NULL);

    // Set default WAL segment size if not specified
    if (hrnPgWal.size == 0)
        hrnPgWal.size = PG_WAL_SEGMENT_SIZE_DEFAULT;

    // Set default system id if not specified
    if (hrnPgWal.systemId < 100)
        hrnPgWal.systemId = hrnPgSystemId(hrnPgWal.version) + hrnPgWal.systemId;

    // Generate WAL
    hrnPgInterfaceVersion(hrnPgWal.version)->wal(hrnPgWal, bufPtr(walBuffer));

    FUNCTION_HARNESS_RETURN_VOID();
}
