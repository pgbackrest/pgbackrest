/***********************************************************************************************************************************
PostgreSQL Version Interface

Macros for building version-specific functions that interface with the types in version.vendor.h. Due to the way PostgreSQL types
evolve over time, this seems to be the easiest way to extract information from them.

These macros should be kept as simple as possible, with most of the logic contained in postgres/interface.c.
***********************************************************************************************************************************/
#include "postgres/interface/version.vendor.h"
#include "postgres/version.h"

/***********************************************************************************************************************************
Determine if the supplied pg_control is for this version of PostgreSQL. When CATALOG_VERSION_NO_MAX is defined then the catalog will
be accepted as a range that lasts until the end of the encoded year. This allows pgBackRest to work with PostgreSQL during the
alpha/beta/rc period without needing to be updated, unless of course the actual interface changes.
***********************************************************************************************************************************/
#if PG_VERSION > PG_VERSION_MAX

#elif PG_VERSION >= PG_VERSION_90

#ifdef CATALOG_VERSION_NO_MAX

#define PG_INTERFACE_CONTROL_IS(version)                                                                                           \
    static bool                                                                                                                    \
    pgInterfaceControlIs##version(const unsigned char *controlFile)                                                                \
    {                                                                                                                              \
        ASSERT(controlFile != NULL);                                                                                               \
                                                                                                                                   \
        return                                                                                                                     \
            ((ControlFileData *)controlFile)->pg_control_version == PG_CONTROL_VERSION &&                                          \
            ((ControlFileData *)controlFile)->catalog_version_no >= CATALOG_VERSION_NO &&                                          \
            ((ControlFileData *)controlFile)->catalog_version_no < (CATALOG_VERSION_NO / 100000 + 1) * 100000;                     \
    }

#else

#define PG_INTERFACE_CONTROL_IS(version)                                                                                           \
    static bool                                                                                                                    \
    pgInterfaceControlIs##version(const unsigned char *controlFile)                                                                \
    {                                                                                                                              \
        ASSERT(controlFile != NULL);                                                                                               \
                                                                                                                                   \
        return                                                                                                                     \
            ((ControlFileData *)controlFile)->pg_control_version == PG_CONTROL_VERSION &&                                          \
            ((ControlFileData *)controlFile)->catalog_version_no == CATALOG_VERSION_NO;                                            \
    }

#endif

#endif

/***********************************************************************************************************************************
Read the version specific pg_control into a general data structure
***********************************************************************************************************************************/
#if PG_VERSION > PG_VERSION_MAX

#elif PG_VERSION >= PG_VERSION_93

#define PG_INTERFACE_CONTROL(version)                                                                                              \
    static PgControl                                                                                                               \
    pgInterfaceControl##version(const unsigned char *controlFile)                                                                  \
    {                                                                                                                              \
        ASSERT(controlFile != NULL);                                                                                               \
        ASSERT(pgInterfaceControlIs##version(controlFile));                                                                        \
                                                                                                                                   \
        return (PgControl)                                                                                                         \
        {                                                                                                                          \
            .systemId = ((ControlFileData *)controlFile)->system_identifier,                                                       \
            .catalogVersion = ((ControlFileData *)controlFile)->catalog_version_no,                                                \
            .checkpoint = ((ControlFileData *)controlFile)->checkPoint,                                                            \
            .timeline = ((ControlFileData *)controlFile)->checkPointCopy.ThisTimeLineID,                                           \
            .pageSize = ((ControlFileData *)controlFile)->blcksz,                                                                  \
            .walSegmentSize = ((ControlFileData *)controlFile)->xlog_seg_size,                                                     \
            .pageChecksum = ((ControlFileData *)controlFile)->data_checksum_version != 0,                                          \
        };                                                                                                                         \
    }

#elif PG_VERSION >= PG_VERSION_90

#define PG_INTERFACE_CONTROL(version)                                                                                              \
    static PgControl                                                                                                               \
    pgInterfaceControl##version(const unsigned char *controlFile)                                                                  \
    {                                                                                                                              \
        ASSERT(controlFile != NULL);                                                                                               \
        ASSERT(pgInterfaceControlIs##version(controlFile));                                                                        \
                                                                                                                                   \
        return (PgControl)                                                                                                         \
        {                                                                                                                          \
            .systemId = ((ControlFileData *)controlFile)->system_identifier,                                                       \
            .catalogVersion = ((ControlFileData *)controlFile)->catalog_version_no,                                                \
            .checkpoint =                                                                                                          \
                (uint64_t)((ControlFileData *)controlFile)->checkPoint.xlogid << 32 |                                              \
                ((ControlFileData *)controlFile)->checkPoint.xrecoff,                                                              \
            .timeline = ((ControlFileData *)controlFile)->checkPointCopy.ThisTimeLineID,                                           \
            .pageSize = ((ControlFileData *)controlFile)->blcksz,                                                                  \
            .walSegmentSize = ((ControlFileData *)controlFile)->xlog_seg_size,                                                     \
        };                                                                                                                         \
    }

#endif

/***********************************************************************************************************************************
Get the control version
***********************************************************************************************************************************/
#if PG_VERSION > PG_VERSION_MAX

#elif PG_VERSION >= PG_VERSION_90

#define PG_INTERFACE_CONTROL_VERSION(version)                                                                                      \
    static uint32_t                                                                                                                \
    pgInterfaceControlVersion##version(void)                                                                                       \
    {                                                                                                                              \
        return PG_CONTROL_VERSION;                                                                                                 \
    }

#endif

/***********************************************************************************************************************************
Determine if the supplied WAL is for this version of PostgreSQL
***********************************************************************************************************************************/
#if PG_VERSION > PG_VERSION_MAX

#elif PG_VERSION >= PG_VERSION_90

#define PG_INTERFACE_WAL_IS(version)                                                                                               \
    static bool                                                                                                                    \
    pgInterfaceWalIs##version(const unsigned char *walFile)                                                                        \
    {                                                                                                                              \
        ASSERT(walFile != NULL);                                                                                                   \
                                                                                                                                   \
        return ((XLogPageHeaderData *)walFile)->xlp_magic == XLOG_PAGE_MAGIC;                                                      \
    }

#endif

/***********************************************************************************************************************************
Read the version specific WAL header into a general data structure
***********************************************************************************************************************************/
#if PG_VERSION > PG_VERSION_MAX

#elif PG_VERSION >= PG_VERSION_90

#define PG_INTERFACE_WAL(version)                                                                                                  \
    static PgWal                                                                                                                   \
    pgInterfaceWal##version(const unsigned char *walFile)                                                                          \
    {                                                                                                                              \
        ASSERT(walFile != NULL);                                                                                                   \
        ASSERT(pgInterfaceWalIs##version(walFile));                                                                                \
                                                                                                                                   \
        return (PgWal)                                                                                                             \
        {                                                                                                                          \
            .systemId = ((XLogLongPageHeaderData *)walFile)->xlp_sysid,                                                            \
            .size = ((XLogLongPageHeaderData *)walFile)->xlp_seg_size,                                                             \
        };                                                                                                                         \
    }

#endif

/***********************************************************************************************************************************
Get the tablespace identifier used to distinguish versions in a tablespace directory
***********************************************************************************************************************************/
#ifdef FORK_GPDB
#define PG_INTERFACE_TABLESPACE_ID(version)                                                                                        \
    static String*                                                                                                                 \
    pgInterfaceTablespaceId##version(unsigned int pgCatalogVersion)                                                                \
    {                                                                                                                              \
        return strNewFmt("GPDB_%i_%u", (PG_CONTROL_VERSION % 10000) / 100, pgCatalogVersion);                                      \
    }

#else
#define PG_INTERFACE_TABLESPACE_ID(version)                                                                                        \
    static String*                                                                                                                 \
    pgInterfaceTablespaceId##version(unsigned int pgCatalogVersion)                                                                \
    {                                                                                                                              \
        String *result = NULL;                                                                                                     \
                                                                                                                                   \
        MEM_CONTEXT_TEMP_BEGIN()                                                                                                   \
        {                                                                                                                          \
            String *pgVersionStr = pgVersionToStr(PG_VERSION);                                                                     \
                                                                                                                                   \
            MEM_CONTEXT_PRIOR_BEGIN()                                                                                              \
            {                                                                                                                      \
                result = strNewFmt("PG_%s_%u", strZ(pgVersionStr), pgCatalogVersion);                                              \
            }                                                                                                                      \
            MEM_CONTEXT_PRIOR_END();                                                                                               \
        }                                                                                                                          \
        MEM_CONTEXT_TEMP_END();                                                                                                    \
                                                                                                                                   \
        return result;                                                                                                             \
    }

#endif

/***********************************************************************************************************************************
Get default WAL segment size
***********************************************************************************************************************************/
#define PG_INTERFACE_WAL_SEGMENT_SIZE_DEFAULT(version)                                                                             \
    static unsigned int                                                                                                            \
    pgInterfaceWalSegmentSizeDefault##version(void)                                                                                \
    {                                                                                                                              \
        return PG_WAL_SEGMENT_SIZE_DEFAULT;                                                                                        \
    }

/***********************************************************************************************************************************
Get default page size
***********************************************************************************************************************************/
#define PG_INTERFACE_PAGE_SIZE_DEFAULT(version)                                                                                    \
    static unsigned int                                                                                                            \
    pgInterfacePageSizeDefault##version(void)                                                                                      \
    {                                                                                                                              \
        return PG_PAGE_SIZE_DEFAULT;                                                                                               \
    }
